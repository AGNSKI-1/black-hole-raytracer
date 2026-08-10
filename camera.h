#pragma once
#include <cmath>
#include "vec3.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Camera orbits around the origin in spherical coordinates.
struct Camera {
    double radius;    // distance from origin
    double azimuth;   // horizontal angle
    double elevation; // polar angle from +Y axis
    double fovDeg;    // vertical field of view
    double aspect;    // width / height

    HD Camera(double radius_    = 6.34194e10,
              double azimuth_   = 0.0,
              double elevation_ = M_PI / 2.0,
              double fovDeg_    = 75.0,
              double aspect_    = 4.0 / 3.0)
        : radius(radius_)
        , azimuth(azimuth_)
        , elevation(elevation_)
        , fovDeg(fovDeg_)
        , aspect(aspect_)
    {}

    // World-space position of the camera.
    HD Vec3 position() const {
        double el = elevation < 1e-4      ? 1e-4
                  : elevation > M_PI-1e-4 ? M_PI-1e-4
                  : elevation;
        return Vec3(
            radius * sin(el) * cos(azimuth),
            radius * cos(el),
            radius * sin(el) * sin(azimuth)
        );
    }

    // Build the orthonormal camera basis.
    HD void basis(Vec3& right, Vec3& up, Vec3& forward) const {
        Vec3 pos = position();
        forward  = (Vec3(0,0,0) - pos).normalize();   // look toward origin
        Vec3 worldUp(0, 1, 0);
        right    = forward.cross(worldUp).normalize();
        up       = right.cross(forward);
    }

    // Return a world-space ray direction for NDC pixel coords (u,v) in [-1,1].
    HD Vec3 generateRay(double u, double v) const {
        Vec3 right, up, forward;
        basis(right, up, forward);
        double tanHalf = tan((fovDeg * M_PI / 180.0) * 0.5);
        Vec3 dir = forward
                 + right * (u * aspect * tanHalf)
                 + up    * (v * tanHalf);
        return dir.normalize();
    }
};
