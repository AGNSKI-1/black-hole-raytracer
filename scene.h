#pragma once
#include "vec3.h"
#include <vector>
#include <cmath>

static constexpr double G_CONST = 6.67430e-11;
static constexpr double C_CONST = 299792458.0;

struct BlackHole {
    double mass;
    double r_s; // Schwarzschild radius

    BlackHole(double massKg = 8.54e36)
        : mass(massKg)
        , r_s(2.0 * G_CONST * massKg / (C_CONST * C_CONST))
    {}
};

// Volumetric accretion disk used for Doppler-shifted emission and lensing.
struct AccretionDisk {
    double innerRadius;
    double outerRadius;
    double halfThickness;

    AccretionDisk(double inner, double outer, double thickness = 1e9)
        : innerRadius(inner)
        , outerRadius(outer)
        , halfThickness(thickness)
    {}
};

// Optional massive/massless sphere, used e.g. to demonstrate gravitational
// lensing of a background object.
struct SphereObject {
    Vec3   center;
    double radius;
    Vec3   color;
    double mass;

    SphereObject(Vec3 c, double r, Vec3 col, double m = 0.0)
        : center(c), radius(r), color(col), mass(m)
    {}
};

// Default scene: a Sgr A*-mass black hole with an accretion disk and one
// companion star, positioned to demonstrate lensing near the photon ring.
struct Scene {
    BlackHole     blackHole;
    AccretionDisk disk;
    std::vector<SphereObject> objects;

    Scene()
        : blackHole(8.54e36)                                     // Sgr A* mass
        , disk(blackHole.r_s * 2.2, blackHole.r_s * 12.0, 1e9)    // ISCO-ish inner edge, wider outer
    {
        objects.push_back(SphereObject(
            Vec3(-5e11, 0.0, 5e10), 1.6e10, Vec3(1.0, 0.9, 0.7), 1.98892e30));
    }
};
