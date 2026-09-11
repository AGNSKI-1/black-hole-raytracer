#pragma once

#include <cmath>

#include "relray/vec3.h"

namespace relray {

template <typename T>
struct GeoRay {
    T r = 0;
    T theta = 0;
    T phi = 0;
    T dr = 0;
    T dtheta = 0;
    T dphi = 0;
    T E = 0;
    T L = 0;
    T x = 0;
    T y = 0;
    T z = 0;
};

template <typename T>
GeoRay<T> initGeoRay(Vec3 pos, Vec3 dir, T r_s) {
    GeoRay<T> ray;

    ray.x = static_cast<T>(pos.x); ray.y = static_cast<T>(pos.y); ray.z = static_cast<T>(pos.z);
    ray.r = std::sqrt(ray.x * ray.x + ray.y * ray.y + ray.z * ray.z);
    if (ray.r < static_cast<T>(1e-6)) ray.r = static_cast<T>(1e-6);

    ray.theta = std::acos(std::max(static_cast<T>(-1.0), std::min(static_cast<T>(1.0), ray.y / ray.r)));
    ray.phi = std::atan2(ray.z, ray.x);

    const T sinT = std::sin(ray.theta);
    const T cosT = std::cos(ray.theta);
    const T sinP = std::sin(ray.phi);
    const T cosP = std::cos(ray.phi);

    const T dx = static_cast<T>(dir.x);
    const T dy = static_cast<T>(dir.y);
    const T dz = static_cast<T>(dir.z);

    ray.dr = sinT * cosP * dx + cosT * dy + sinT * sinP * dz;
    ray.dtheta = (cosT * cosP * dx - sinT * dy + cosT * sinP * dz) / ray.r;
    const T st = std::fabs(sinT) < static_cast<T>(1e-10) ? static_cast<T>(1e-10) : sinT;
    ray.dphi = (-sinP * dx + cosP * dz) / (ray.r * st);

    ray.L = ray.r * ray.r * sinT * ray.dphi;
    const T f = static_cast<T>(1.0) - r_s / ray.r;
    const T speed = std::sqrt(ray.dr * ray.dr / f + ray.r * ray.r * (ray.dtheta * ray.dtheta + sinT * sinT * ray.dphi * ray.dphi));
    ray.E = f * speed;
    return ray;
}

template <typename T>
void geodesicRHS(const GeoRay<T>& ray, T r_s, T d1[3], T d2[3]) {
    const T r = ray.r;
    const T theta = ray.theta;
    T sinT = std::sin(theta);
    const T cosT = std::cos(theta);
    if (std::fabs(sinT) < static_cast<T>(1e-10)) sinT = (sinT >= static_cast<T>(0) ? static_cast<T>(1e-10) : static_cast<T>(-1e-10));

    const T f = static_cast<T>(1.0) - r_s / r;
    const T r2 = r * r;

    d1[0] = ray.dr;
    d1[1] = ray.dtheta;
    d1[2] = ray.dphi;

    d2[0] = -(r_s / ((T)2.0 * r2)) * (f * (ray.E / f) * (ray.E / f))
            + (r_s / ((T)2.0 * r2 * f)) * ray.dr * ray.dr
            + r * (ray.dtheta * ray.dtheta + sinT * sinT * ray.dphi * ray.dphi);

    d2[1] = -(T)2.0 * ray.dr * ray.dtheta / r + sinT * cosT * ray.dphi * ray.dphi;
    d2[2] = -(T)2.0 * ray.dr * ray.dphi / r - (T)2.0 * (cosT / sinT) * ray.dtheta * ray.dphi;
}

template <typename T>
void rk4Step(GeoRay<T>& ray, T dL, T r_s) {
    T k1[3], l1[3];
    T k2[3], l2[3];
    T k3[3], l3[3];
    T k4[3], l4[3];

    geodesicRHS(ray, r_s, k1, l1);

    GeoRay<T> tmp = ray;
    tmp.r += static_cast<T>(0.5) * dL * k1[0];
    tmp.theta += static_cast<T>(0.5) * dL * k1[1];
    tmp.phi += static_cast<T>(0.5) * dL * k1[2];
    tmp.dr += static_cast<T>(0.5) * dL * l1[0];
    tmp.dtheta += static_cast<T>(0.5) * dL * l1[1];
    tmp.dphi += static_cast<T>(0.5) * dL * l1[2];
    geodesicRHS(tmp, r_s, k2, l2);

    tmp = ray;
    tmp.r += static_cast<T>(0.5) * dL * k2[0];
    tmp.theta += static_cast<T>(0.5) * dL * k2[1];
    tmp.phi += static_cast<T>(0.5) * dL * k2[2];
    tmp.dr += static_cast<T>(0.5) * dL * l2[0];
    tmp.dtheta += static_cast<T>(0.5) * dL * l2[1];
    tmp.dphi += static_cast<T>(0.5) * dL * l2[2];
    geodesicRHS(tmp, r_s, k3, l3);

    tmp = ray;
    tmp.r += dL * k3[0];
    tmp.theta += dL * k3[1];
    tmp.phi += dL * k3[2];
    tmp.dr += dL * l3[0];
    tmp.dtheta += dL * l3[1];
    tmp.dphi += dL * l3[2];
    geodesicRHS(tmp, r_s, k4, l4);

    ray.r += (dL / static_cast<T>(6.0)) * (k1[0] + static_cast<T>(2.0) * k2[0] + static_cast<T>(2.0) * k3[0] + k4[0]);
    ray.theta += (dL / static_cast<T>(6.0)) * (k1[1] + static_cast<T>(2.0) * k2[1] + static_cast<T>(2.0) * k3[1] + k4[1]);
    ray.phi += (dL / static_cast<T>(6.0)) * (k1[2] + static_cast<T>(2.0) * k2[2] + static_cast<T>(2.0) * k3[2] + k4[2]);
    ray.dr += (dL / static_cast<T>(6.0)) * (l1[0] + static_cast<T>(2.0) * l2[0] + static_cast<T>(2.0) * l3[0] + l4[0]);
    ray.dtheta += (dL / static_cast<T>(6.0)) * (l1[1] + static_cast<T>(2.0) * l2[1] + static_cast<T>(2.0) * l3[1] + l4[1]);
    ray.dphi += (dL / static_cast<T>(6.0)) * (l1[2] + static_cast<T>(2.0) * l2[2] + static_cast<T>(2.0) * l3[2] + l4[2]);

    const T sinT = std::sin(ray.theta);
    ray.x = ray.r * sinT * std::cos(ray.phi);
    ray.y = ray.r * std::cos(ray.theta);
    ray.z = ray.r * sinT * std::sin(ray.phi);
}

} // namespace relray
