#pragma once

#include <cmath>

#include "relray/vec3.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace relray {

struct Camera {
    double radius = 6.34194e10;
    double azimuth = 0.0;
    double elevation = M_PI / 2.0;
    double fovDeg = 75.0;
    double aspect = 4.0 / 3.0;

    Camera() = default;
    Camera(double radius_, double azimuth_, double elevation_, double fovDeg_, double aspect_)
        : radius(radius_), azimuth(azimuth_), elevation(elevation_), fovDeg(fovDeg_), aspect(aspect_) {}

    Vec3 position() const {
        double el = elevation < 1e-4 ? 1e-4 : elevation > M_PI - 1e-4 ? M_PI - 1e-4 : elevation;
        return Vec3(
            radius * std::sin(el) * std::cos(azimuth),
            radius * std::cos(el),
            radius * std::sin(el) * std::sin(azimuth)
        );
    }

    void basis(Vec3& right, Vec3& up, Vec3& forward) const {
        Vec3 pos = position();
        forward = (Vec3(0.0, 0.0, 0.0) - pos).normalize();
        Vec3 worldUp(0.0, 1.0, 0.0);
        right = forward.cross(worldUp).normalize();
        up = right.cross(forward);
    }

    Vec3 generateRay(double u, double v) const {
        Vec3 right, up, forward;
        basis(right, up, forward);
        const double tanHalf = std::tan((fovDeg * M_PI / 180.0) * 0.5);
        Vec3 dir = forward + right * (u * aspect * tanHalf) + up * (v * tanHalf);
        return dir.normalize();
    }
};

} // namespace relray
