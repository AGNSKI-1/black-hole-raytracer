#pragma once

#include <vector>

#include "relray/vec3.h"

namespace relray {

struct BlackHole {
    double mass = 8.54e36;
    double r_s = 2.0 * 6.67430e-11 * mass / (299792458.0 * 299792458.0);
};

struct AccretionDisk {
    double innerRadius = 0.0;
    double outerRadius = 0.0;
    double halfThickness = 0.0;
};

struct SphereObject {
    Vec3 center{0.0, 0.0, 0.0};
    double radius = 0.0;
    Vec3 color{0.0, 0.0, 0.0};
    double mass = 0.0;
};

struct Scene {
    BlackHole blackHole;
    AccretionDisk disk{};
    std::vector<SphereObject> objects;

    Scene() {
        blackHole.mass = 8.54e36;
        blackHole.r_s = 2.0 * 6.67430e-11 * blackHole.mass / (299792458.0 * 299792458.0);
        disk.innerRadius = blackHole.r_s * 2.2;
        disk.outerRadius = blackHole.r_s * 12.0;
        disk.halfThickness = 1e9;
        objects.push_back(SphereObject{Vec3(-5e11, 0.0, 5e10), 1.6e10, Vec3(1.0, 0.9, 0.7), 1.98892e30});
    }
};

} // namespace relray
