#pragma once
#include <cmath>
#include <cstdint>
#include "vec3.h"   // for the HD macro (__host__ __device__ on CUDA, inline on CPU)

// Procedural value noise for disk texture, and a Henyey-Greenstein phase
// function approximating scattering in the accretion disk's gaseous matter.

// Hashes an integer lattice point to a pseudo-random 32-bit value.
HD static uint32_t _dnHashU3(int32_t x, int32_t y, int32_t z)
{
    uint32_t h = (uint32_t)x * 1664525u
               ^ (uint32_t)y * 1013904223u
               ^ (uint32_t)z * 22695477u;
    h ^= h >> 16;
    h *= 0x45d9f3bu;
    h ^= h >> 16;
    return h;
}

// Converts a lattice hash to a float in [0, 1).
HD static float _dnHash3f(int32_t x, int32_t y, int32_t z)
{
    return (float)(_dnHashU3(x, y, z) & 0xFFFFFFu) * (1.0f / 16777216.0f);
}

// Trilinearly-interpolated value noise, used as the base signal for fBm.
HD static float diskValueNoise(float x, float y, float z)
{
    int32_t ix = (int32_t)floorf(x);
    int32_t iy = (int32_t)floorf(y);
    int32_t iz = (int32_t)floorf(z);
    float fx = x - (float)ix;
    float fy = y - (float)iy;
    float fz = z - (float)iz;

    // Hermite smoothstep: 3t^2 - 2t^3
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    fz = fz * fz * (3.0f - 2.0f * fz);

    // 8-corner trilinear interpolation
    float v000 = _dnHash3f(ix,   iy,   iz  );
    float v100 = _dnHash3f(ix+1, iy,   iz  );
    float v010 = _dnHash3f(ix,   iy+1, iz  );
    float v110 = _dnHash3f(ix+1, iy+1, iz  );
    float v001 = _dnHash3f(ix,   iy,   iz+1);
    float v101 = _dnHash3f(ix+1, iy,   iz+1);
    float v011 = _dnHash3f(ix,   iy+1, iz+1);
    float v111 = _dnHash3f(ix+1, iy+1, iz+1);

    float v00 = v000 + fx * (v100 - v000);
    float v10 = v010 + fx * (v110 - v010);
    float v01 = v001 + fx * (v101 - v001);
    float v11 = v011 + fx * (v111 - v011);
    float v0  = v00  + fy * (v10  - v00 );
    float v1  = v01  + fy * (v11  - v01 );
    return v0 + fz * (v1 - v0);
}

// Fractional Brownian motion (5 octaves) over the value noise above, used
// to give the disk a natural, turbulent-looking texture.
HD static float diskFbm(float x, float y, float z)
{
    float n    = 0.0f;
    float amp  = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < 5; ++i) {
        n    += amp * diskValueNoise(x * freq, y * freq, z * freq);
        amp  *= 0.5f;
        freq *= 2.0f;
    }
    return n;  // ~[0, 1]
}

// Henyey-Greenstein phase function approximation for scattering in the
// disk's gaseous matter; g controls forward (g>0) vs. backward (g<0) scatter.
HD static float diskHenyeyGreenstein(float cos_theta, float g)
{
    float g2    = g * g;
    float denom = 1.0f + g2 - 2.0f * g * cos_theta;
    // denom^(3/2) = denom * sqrt(denom)
    return (1.0f - g2) / (denom * sqrtf(denom));
}
