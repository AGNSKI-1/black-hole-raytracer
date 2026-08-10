// renderer.cu — CUDA GPU renderer.
//
// One CUDA thread integrates one photon's geodesic (one pixel), so the
// kernel is embarrassingly parallel across the image. Design notes:
//   - Scene scalars and up to 8 sphere objects live in __constant__ memory
//     (broadcast to every thread in a warp on a single read).
//   - Output is written to three separate float planes (R/G/B) rather than
//     an interleaved float3 buffer, so each warp's writes to a given plane
//     are coalesced.
//   - Per-ray termination is tracked with a bool + __ballot_sync so the
//     whole warp can exit its loop together once every lane is done,
//     instead of diverging on individual `return`s.

#include <cuda_runtime.h>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <vector>
#include <string>

#include "camera.h"
#include "scene.h"
#include "image.h"
#include "renderer.h"
#include "geodesic.h"
#include "disk_noise.h"

__constant__ float c_rs;
__constant__ float c_dLambda;
__constant__ int   c_maxSteps;
__constant__ float c_escapeRadius;
__constant__ float c_diskInner;
__constant__ float c_diskOuter;
__constant__ float c_diskPhase;

__constant__ int   c_numObjects;
__constant__ float c_objCx[8];
__constant__ float c_objCy[8];
__constant__ float c_objCz[8];
__constant__ float c_objRadius[8];
__constant__ float c_objColorR[8];
__constant__ float c_objColorG[8];
__constant__ float c_objColorB[8];

#define CUDA_CHECK(call)                                                        \
    do {                                                                        \
        cudaError_t _e = (call);                                                \
        if (_e != cudaSuccess)                                                  \
            throw std::runtime_error(std::string("CUDA error: ")               \
                                   + cudaGetErrorString(_e)                    \
                                   + " at " __FILE__ ":"                       \
                                   + std::to_string(__LINE__));                \
    } while(0)

// Geodesic integration kernel — one thread per pixel, SoA output.
__global__ void geodesic_kernel(
    float* __restrict__ d_R,        // W*H floats — red   plane
    float* __restrict__ d_G,        // W*H floats — green plane
    float* __restrict__ d_B,        // W*H floats — blue  plane
    int width, int height,
    float camX,  float camY,  float camZ,
    float rightX,float rightY,float rightZ,
    float upX,   float upY,   float upZ,
    float fwdX,  float fwdY,  float fwdZ,
    float tanHalf, float aspect)
{
    int px = (int)(blockIdx.x * blockDim.x + threadIdx.x);
    int py = (int)(blockIdx.y * blockDim.y + threadIdx.y);
    if (px >= width || py >= height) return;

    // NDC coordinates
    float u =  (2.0f * (px + 0.5f) / width  - 1.0f);
    float v = -(2.0f * (py + 0.5f) / height - 1.0f);

    // Ray direction
    float dx = fwdX + rightX*(u*aspect*tanHalf) + upX*(v*tanHalf);
    float dy = fwdY + rightY*(u*aspect*tanHalf) + upY*(v*tanHalf);
    float dz = fwdZ + rightZ*(u*aspect*tanHalf) + upZ*(v*tanHalf);
    float rlen = sqrtf(dx*dx + dy*dy + dz*dz);
    dx /= rlen; dy /= rlen; dz /= rlen;

    // Initialise geodesic ray
    Vec3   pos(camX, camY, camZ);
    Vec3   dir(dx, dy, dz);
    GeoRay<float> ray = initGeoRay(pos, dir, c_rs);

    // Integrate until escape/capture/hit, accumulating disk emission along
    // the way. `alive` plus __ballot_sync lets the whole warp exit together.
    float colR = 0.0f, colG = 0.0f, colB = 0.0f;
    bool  alive = true;

    for (int step = 0; step < c_maxSteps; ++step) {

        // All lanes vote; exit the loop when none remain active.
        if (__ballot_sync(0xFFFFFFFFu, (unsigned)alive) == 0u) break;

        if (alive) {
            // 1. Event-horizon capture
            if (ray.r <= c_rs) {
                alive = false;
            } else {
                // 2. Advance one RK4 step
                rk4Step(ray, c_dLambda, c_rs);

                // 3. Volumetric accretion disk.
                {
                    float r2d = sqrtf(ray.x*ray.x + ray.z*ray.z);
                    if (r2d >= c_diskInner * 0.9f && r2d <= c_diskOuter * 1.1f) {
                        float h       = 0.015f * r2d;          // disk half-height
                        float yh      = ray.y / h;
                        float density = expf(-0.5f * yh * yh); // Gaussian

                        // Soft radial edge falloff
                        float inner_fade = fminf(1.0f, (r2d - c_diskInner) / (0.15f * c_diskInner));
                        float outer_fade = fminf(1.0f, (c_diskOuter - r2d) / (0.15f * c_diskOuter));
                        float edge = fmaxf(0.0f, inner_fade) * fmaxf(0.0f, outer_fade);
                        density *= edge;

                        if (density > 0.01f) {
                            // Doppler + gravitational redshift
                            float beta   = sqrtf(c_rs / (2.0f * r2d));
                            float vx_orb = -ray.z / r2d;
                            float vz_orb =  ray.x / r2d;
                            float dcx = camX - ray.x;
                            float dcz = camZ - ray.z;
                            float dcl = sqrtf(dcx*dcx + (camY-ray.y)*(camY-ray.y) + dcz*dcz);
                            dcx /= dcl; dcz /= dcl;
                            float cos_alpha = vx_orb*dcx + vz_orb*dcz;
                            float grav = sqrtf(1.0f - c_rs / r2d);
                            float g    = grav * sqrtf(1.0f - beta*beta)
                                         / (1.0f - beta * cos_alpha);
                            float g4   = g*g*g*g;

                            float t = fmaxf(0.0f, fminf(1.0f,
                                (r2d - c_diskInner) / (c_diskOuter - c_diskInner)));
                            float t_eff = fmaxf(0.0f, fminf(1.0f, t - (g - 1.0f) * 0.6f));

                            float cr, cg_col, cb;
                            if (t_eff < 0.5f) {
                                float s = t_eff * 2.0f;
                                cr     = 0.70f + s*(1.0f  - 0.70f);
                                cg_col = 0.85f + s*(1.0f  - 0.85f);
                                cb     = 1.0f;
                            } else {
                                float s = (t_eff - 0.5f) * 2.0f;
                                cr     = 1.0f;
                                cg_col = 1.0f + s*(0.35f - 1.0f);
                                cb     = 1.0f + s*(0.05f - 1.0f);
                            }

                            // Procedural swirl/turbulence for a clumpy, natural look
                            float phi_disk   = atan2f(ray.z, ray.x);
                            float rratio     = c_diskInner / r2d;
                            float keplerian  = rratio * sqrtf(rratio);
                            float phi_rotated = phi_disk - c_diskPhase * keplerian;
                            float rcp_w      = 1.0f / (c_diskOuter - c_diskInner);
                            float phi_swirl  = phi_rotated
                                               + 4.0f * logf(1.0f + (r2d - c_diskInner) * rcp_w);
                            float noise_r    = fmaxf(0.0f, (r2d - c_diskInner) * rcp_w * 6.0f);
                            float raw        = diskFbm(noise_r * cosf(phi_swirl),
                                                       yh      * 1.5f,
                                                       noise_r * sinf(phi_swirl));
                            float turb       = raw * raw * (3.0f - 2.0f * raw);
                            turb             = 0.05f + 0.95f * turb;

                            // Henyey-Greenstein scattering approximation
                            float lx_hg  = ray.x / (r2d + 1e-9f);
                            float lz_hg  = ray.z / (r2d + 1e-9f);
                            float cos_hg = lx_hg * dcx + lz_hg * dcz;
                            float phase  = diskHenyeyGreenstein(cos_hg, 0.45f);

                            float rho    = density * turb;
                            float emit   = (2.5f*(1.0f-t)*(1.0f-t) + 0.15f)
                                           * g4 * rho * 0.015f * phase;
                            colR += cr     * emit;
                            colG += cg_col * emit;
                            colB += cb     * emit;
                        }
                    }
                }

                // 4. Sphere objects (e.g. background star, for lensing)
                if (alive) {
                    for (int o = 0; o < c_numObjects; ++o) {
                        float ex = ray.x - c_objCx[o];
                        float ey = ray.y - c_objCy[o];
                        float ez = ray.z - c_objCz[o];
                        if (ex*ex + ey*ey + ez*ez <= c_objRadius[o]*c_objRadius[o]) {
                            float nl  = sqrtf(ex*ex + ey*ey + ez*ez);
                            float nx  = ex/nl, ny = ey/nl, nz = ez/nl;
                            float vx  = camX-ray.x, vy = camY-ray.y, vz = camZ-ray.z;
                            float vl  = sqrtf(vx*vx + vy*vy + vz*vz);
                            float mu  = fmaxf(0.0f, nx*(vx/vl) + ny*(vy/vl) + nz*(vz/vl));

                            // Quadratic limb darkening
                            float limb  = 1.0f - 0.5f*(1.0f-mu) - 0.2f*(1.0f-mu)*(1.0f-mu);
                            // Surface granulation
                            float raw_  = diskFbm(nx*5.0f, ny*5.0f, nz*5.0f);
                            float sharp_= raw_ * raw_ * (3.0f - 2.0f * raw_);
                            float grain = 0.55f + 0.60f * sharp_;
                            // Limb colour shift
                            float rBoost = 1.0f + 0.18f*(1.0f-mu);
                            float bDamp  = 1.0f - 0.25f*(1.0f-mu);
                            float I = limb * grain;
                            colR += fminf(1.0f, c_objColorR[o] * rBoost * I);
                            colG += c_objColorG[o] * I;
                            colB += c_objColorB[o] * bDamp  * I;
                            alive = false; break;
                        }
                    }
                }

                // 5. Escape condition
                if (alive && ray.r > c_escapeRadius) alive = false;
            }
        }
    }

    // Write HDR pixel output
    int idx = py * width + px;
    d_R[idx] = (float)colR;
    d_G[idx] = (float)colG;
    d_B[idx] = (float)colB;
}

// Host wrapper called from main.cpp.
Image renderGPU(const Camera& cam, const Scene& scene, const RenderParams& params)
{
    const int W = params.width;
    const int H = params.height;

    // Scene scalars to constant memory
    float rs_f   = (float)scene.blackHole.r_s;
    float dL_f   = (float)params.dLambda;
    int   steps  = params.maxSteps;
    float esc_f  = (float)params.escapeRadius;
    float di_f   = (float)scene.disk.innerRadius;
    float dout_f = (float)scene.disk.outerRadius;
    float dp_f   = (float)params.diskPhase;

    CUDA_CHECK(cudaMemcpyToSymbol(c_rs,           &rs_f,   sizeof(float)));
    CUDA_CHECK(cudaMemcpyToSymbol(c_dLambda,      &dL_f,   sizeof(float)));
    CUDA_CHECK(cudaMemcpyToSymbol(c_maxSteps,     &steps,  sizeof(int)));
    CUDA_CHECK(cudaMemcpyToSymbol(c_escapeRadius, &esc_f,  sizeof(float)));
    CUDA_CHECK(cudaMemcpyToSymbol(c_diskInner,    &di_f,   sizeof(float)));
    CUDA_CHECK(cudaMemcpyToSymbol(c_diskOuter,    &dout_f, sizeof(float)));
    CUDA_CHECK(cudaMemcpyToSymbol(c_diskPhase,    &dp_f,   sizeof(float)));

    // Sphere objects to constant memory
    int nObj = (int)scene.objects.size();
    if (nObj > 8) nObj = 8;
    CUDA_CHECK(cudaMemcpyToSymbol(c_numObjects, &nObj, sizeof(int)));

    if (nObj > 0) {
        float cx[8]={}, cy[8]={}, cz[8]={}, cr[8]={},
              oR[8]={}, oG[8]={}, oB[8]={};
        for (int i = 0; i < nObj; ++i) {
            cx[i] = (float)scene.objects[i].center.x;
            cy[i] = (float)scene.objects[i].center.y;
            cz[i] = (float)scene.objects[i].center.z;
            cr[i] = (float)scene.objects[i].radius;
            oR[i] = (float)scene.objects[i].color.x;
            oG[i] = (float)scene.objects[i].color.y;
            oB[i] = (float)scene.objects[i].color.z;
        }
        CUDA_CHECK(cudaMemcpyToSymbol(c_objCx,     cx, nObj*sizeof(float)));
        CUDA_CHECK(cudaMemcpyToSymbol(c_objCy,     cy, nObj*sizeof(float)));
        CUDA_CHECK(cudaMemcpyToSymbol(c_objCz,     cz, nObj*sizeof(float)));
        CUDA_CHECK(cudaMemcpyToSymbol(c_objRadius, cr, nObj*sizeof(float)));
        CUDA_CHECK(cudaMemcpyToSymbol(c_objColorR, oR, nObj*sizeof(float)));
        CUDA_CHECK(cudaMemcpyToSymbol(c_objColorG, oG, nObj*sizeof(float)));
        CUDA_CHECK(cudaMemcpyToSymbol(c_objColorB, oB, nObj*sizeof(float)));
    }

    // Camera basis on host
    Vec3 pos = cam.position();
    Vec3 right, up, fwd;
    cam.basis(right, up, fwd);
    float tanHalf = (float)std::tan((cam.fovDeg * M_PI / 180.0) * 0.5);

    // Allocate output buffers on device: one plane per color channel for
    // coalesced writes.
    size_t nPix    = (size_t)W * H;
    size_t nFloats = nPix * 3;
    float* d_buf   = nullptr;
    CUDA_CHECK(cudaMalloc(&d_buf, nFloats * sizeof(float)));
    CUDA_CHECK(cudaMemset(d_buf, 0, nFloats * sizeof(float)));

    float* d_R = d_buf + 0 * nPix;
    float* d_G = d_buf + 1 * nPix;
    float* d_B = d_buf + 2 * nPix;

    dim3 block(16, 16);
    dim3 grid((W + block.x - 1) / block.x,
              (H + block.y - 1) / block.y);

    if (!params.quiet)
        std::cout << "  Launching GPU kernel  grid=("
                  << grid.x << "x" << grid.y << ")  block=("
                  << block.x << "x" << block.y << ")\n" << std::flush;

    geodesic_kernel<<<grid, block>>>(
        d_R, d_G, d_B, W, H,
        (float)pos.x,   (float)pos.y,   (float)pos.z,
        (float)right.x, (float)right.y, (float)right.z,
        (float)up.x,    (float)up.y,    (float)up.z,
        (float)fwd.x,   (float)fwd.y,   (float)fwd.z,
        tanHalf, (float)cam.aspect);

    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());

    // Copy results to host
    std::vector<float> h_buf(nFloats);
    CUDA_CHECK(cudaMemcpy(h_buf.data(), d_buf,
                          nFloats * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaFree(d_buf));

    // Convert SoA back to AoS and write to Image struct
    Image img(W, H);
    const float* h_R = h_buf.data() + 0 * nPix;
    const float* h_G = h_buf.data() + 1 * nPix;
    const float* h_B = h_buf.data() + 2 * nPix;
    for (size_t i = 0; i < nPix; ++i) {
        img.data[3*i + 0] = h_R[i];
        img.data[3*i + 1] = h_G[i];
        img.data[3*i + 2] = h_B[i];
    }

    if (!params.quiet) std::cout << "  GPU render complete.\n" << std::flush;
    return img;
}
