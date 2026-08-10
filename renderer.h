#pragma once
#include <cmath>
#include <algorithm>
#include <iostream>
#include "vec3.h"
#include "camera.h"
#include "scene.h"
#include "geodesic.h"
#include "image.h"
#include "disk_noise.h"

// Render parameters; defaults here are overridden by config.h / CLI flags
// in main.cpp.
struct RenderParams {
    int    width        = 800;
    int    height       = 600;
    int    maxSteps     = 60000;   // max geodesic integration steps per ray
    double dLambda      = 1e7;     // affine parameter step size (metres, roughly)
    double escapeRadius = 1e13;    // beyond this the ray has escaped to "infinity"
    double diskPhase    = 0.0;     // base rotation angle at inner disk edge (radians);
                                    // disk matter at radius r is rotated by
                                    // diskPhase * (diskInner/r)^(3/2)
                                    // (Keplerian differential rotation)
    bool   quiet        = false;   // suppress all stdout progress/info prints
};

// Hit outcome per ray.
enum class HitType { None, BlackHole, Disk, Sphere };

struct HitResult {
    HitType type   = HitType::None;
    Vec3    color  = {0, 0, 0};
    Vec3    pos    = {0, 0, 0};
    Vec3    normal = {0, 0, 0};
};

// Shades a sphere surface with limb darkening and procedural granulation.
// Mirrors the GPU implementation in renderer.cu.
static Vec3 sphereShade(const Vec3& P, const Vec3& center,
                         const Vec3& camPos, const Vec3& baseColor)
{
    Vec3 N = (P - center).normalize();
    Vec3 V = (camPos - P).normalize();
    double mu = std::max(0.0, N.dot(V));

    // Quadratic limb darkening
    double limb = 1.0 - 0.5*(1.0-mu) - 0.2*(1.0-mu)*(1.0-mu);

    // Surface granulation
    float raw   = diskFbm((float)N.x*5.0f, (float)N.y*5.0f, (float)N.z*5.0f);
    float sharp = raw * raw * (3.0f - 2.0f * raw);   // smoothstep contrast boost
    double grain = 0.55 + 0.60*(double)sharp;         // [0.55, 1.15]

    // Limb colour shift
    double rBoost = 1.0 + 0.18*(1.0-mu);
    double bDamp  = 1.0 - 0.25*(1.0-mu);

    double I = limb * grain;
    return Vec3(std::min(1.0, baseColor.x * rBoost * I),
                baseColor.y * I,
                baseColor.z * bDamp  * I);
}

// Traces a single ray through the scene via RK4 null-geodesic integration.
// Disk rendering is volumetric and accumulates color along the ray path.
static HitResult traceRay(const Vec3& origin, const Vec3& dir,
                            const Scene& scene, const RenderParams& p)
{
    HitResult result;
    const double r_s        = scene.blackHole.r_s;
    const double diskInner  = scene.disk.innerRadius;
    const double diskOuter  = scene.disk.outerRadius;

    GeoRay<double> ray = initGeoRay(origin, dir, r_s);

    double colR = 0.0, colG = 0.0, colB = 0.0;

    for (int step = 0; step < p.maxSteps; ++step) {

        // 1. Event horizon
        if (ray.r <= r_s) {
            result.type  = HitType::BlackHole;
            result.color = {0, 0, 0};
            return result;
        }

        // 2. Advance ray
        rk4Step(ray, p.dLambda, r_s);

        // 3. Volumetric accretion disk
        {
            double r2d = std::sqrt(ray.x*ray.x + ray.z*ray.z);
            if (r2d >= diskInner * 0.9 && r2d <= diskOuter * 1.1) {
                double h       = 0.025 * r2d;
                double yh      = ray.y / h;
                double density = std::exp(-0.5 * yh * yh);

                // Soft radial edge falloff
                double inner_fade = std::min(1.0, (r2d - diskInner) / (0.15 * diskInner));
                double outer_fade = std::min(1.0, (diskOuter - r2d) / (0.15 * diskOuter));
                double edge = std::max(0.0, inner_fade) * std::max(0.0, outer_fade);
                density *= edge;

                if (density > 0.01) {
                    // Doppler + gravitational redshift
                    double beta    = std::sqrt(r_s / (2.0 * r2d));
                    double vx_orb  = -ray.z / r2d;
                    double vz_orb  =  ray.x / r2d;
                    double dcx = origin.x - ray.x;
                    double dcy = origin.y - ray.y;
                    double dcz = origin.z - ray.z;
                    double dcl = std::sqrt(dcx*dcx + dcy*dcy + dcz*dcz);
                    dcx /= dcl; dcy /= dcl; dcz /= dcl;
                    double cos_alpha = vx_orb*dcx + vz_orb*dcz;
                    double grav = std::sqrt(1.0 - r_s / r2d);
                    double g    = grav * std::sqrt(1.0 - beta*beta)
                                  / (1.0 - beta * cos_alpha);
                    double g4   = g*g*g*g;

                    double t_radial = std::max(0.0, std::min(1.0,
                        (r2d - diskInner) / (diskOuter - diskInner)));
                    double t_eff = std::max(0.0, std::min(1.0,
                        t_radial - (g - 1.0) * 0.6));

                    double cr, cg_col, cb;
                    if (t_eff < 0.5) {
                        double s = t_eff * 2.0;
                        cr     = 0.70 + s*(1.0  - 0.70);
                        cg_col = 0.85 + s*(1.0  - 0.85);
                        cb     = 1.0;
                    } else {
                        double s = (t_eff - 0.5) * 2.0;
                        cr     = 1.0;
                        cg_col = 1.0 + s*(0.35 - 1.0);
                        cb     = 1.0 + s*(0.05 - 1.0);
                    }

                    // Procedural swirl/turbulence
                    float phi_disk   = (float)std::atan2(ray.z, ray.x);
                    float rratio     = (float)(diskInner / r2d);
                    float keplerian  = rratio * std::sqrt(rratio); // (diskInner/r)^(3/2)
                    float phi_rotated = phi_disk - (float)p.diskPhase * keplerian;
                    float rcp_w     = 1.0f / (float)(diskOuter - diskInner);
                    float phi_swirl = phi_rotated
                                      + 4.0f * logf(1.0f + (float)(r2d - diskInner) * rcp_w);
                    float noise_r   = std::max(0.0f, (float)(r2d - diskInner) * rcp_w * 6.0f);
                    float raw       = diskFbm(noise_r * cosf(phi_swirl),
                                             (float)yh * 1.5f,
                                             noise_r * sinf(phi_swirl));
                    float turb      = raw * raw * (3.0f - 2.0f * raw);
                    turb            = 0.05f + 0.95f * turb;

                    // Henyey-Greenstein scattering approximation
                    float lx_hg  = (float)(ray.x / (r2d + 1e-9));
                    float lz_hg  = (float)(ray.z / (r2d + 1e-9));
                    float cos_hg = lx_hg * (float)dcx + lz_hg * (float)dcz;
                    float phase  = diskHenyeyGreenstein(cos_hg, 0.45f);

                    double rho  = density * (double)turb;
                    double emit = (2.5*(1.0-t_radial)*(1.0-t_radial) + 0.15)
                                  * g4 * rho * 0.015 * (double)phase;

                    colR += cr     * emit;
                    colG += cg_col * emit;
                    colB += cb     * emit;
                }
            }
        }

        // 4. Sphere objects (e.g. background star, for lensing)
        Vec3 P(ray.x, ray.y, ray.z);
        for (const auto& obj : scene.objects) {
            if ((P - obj.center).length() <= obj.radius) {
                result.type   = HitType::Sphere;
                result.pos    = P;
                result.normal = (P - obj.center).normalize();
                Vec3 sc       = sphereShade(P, obj.center, origin, obj.color);
                result.color  = Vec3(colR + sc.x, colG + sc.y, colB + sc.z);
                return result;
            }
        }

        // 5. Escape check
        if (ray.r > p.escapeRadius) break;
    }

    // Ray escaped or hit the step budget.
    result.type  = (colR + colG + colB > 0.0) ? HitType::Disk : HitType::None;
    result.color = Vec3(colR, colG, colB);
    return result;
}

// CPU (OpenMP) reference renderer.
inline Image renderCPU(const Camera& cam, const Scene& scene,
                       const RenderParams& params)
{
    Image img(params.width, params.height);
    Vec3  origin = cam.position();

    int totalPixels = params.width * params.height;
    int reportEvery = totalPixels / 20; // print progress every 5 %

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic, 8)
#endif
    for (int idx = 0; idx < totalPixels; ++idx) {

        int px = idx % params.width;
        int py = idx / params.width;

        // Convert pixel to NDC
        double u =  (2.0 * (px + 0.5) / params.width  - 1.0);
        double v = -(2.0 * (py + 0.5) / params.height - 1.0); // flip y

        Vec3 dir = cam.generateRay(u, v);

        HitResult hit = traceRay(origin, dir, scene, params);

        img.setPixel(px, py,
                     (float)hit.color.x,
                     (float)hit.color.y,
                     (float)hit.color.z);

        // Progress reporting
#ifdef _OPENMP
        #pragma omp critical
#endif
        {
            if (!params.quiet && reportEvery > 0 && idx % reportEvery == 0) {
                int pct = (int)(100.0 * idx / totalPixels);
                std::cout << "\r  Rendering... " << pct << "% " << std::flush;
            }
        }
    }

    if (!params.quiet) std::cout << "\r  Rendering... 100%\n" << std::flush;
    return img;
}
