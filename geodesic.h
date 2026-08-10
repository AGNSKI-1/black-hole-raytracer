#pragma once
#include <cmath>
#include "vec3.h"

// Schwarzschild null-geodesic integration in spherical coordinates.
// Each ray carries its own conserved energy E and angular momentum L
// (equatorial-plane symmetry is not assumed — rays start in Cartesian
// space and are projected onto the local spherical basis), and is
// advanced with fixed-step RK4. Templated on T so the same code compiles
// for the double-precision CPU path and the float GPU kernel.

template<typename T>
struct GeoRay {
    T r, theta, phi;
    T dr, dtheta, dphi;
    T E, L;
    T x, y, z;

    HD GeoRay() : r(0), theta(0), phi(0), dr(0), dtheta(0), dphi(0),
                  E(0), L(0), x(0), y(0), z(0) {}
};

// Initialize a geodesic ray from a Cartesian origin/direction.
template<typename T>
HD GeoRay<T> initGeoRay(Vec3 pos, Vec3 dir, T r_s) {
    GeoRay<T> ray;

    ray.x = (T)pos.x; ray.y = (T)pos.y; ray.z = (T)pos.z;
    ray.r = sqrt(ray.x*ray.x + ray.y*ray.y + ray.z*ray.z);
    if (ray.r < (T)1e-6) ray.r = (T)1e-6;

    ray.theta = acos(fmax((T)-1.0, fmin((T)1.0, ray.y / ray.r)));
    ray.phi   = atan2(ray.z, ray.x);

    T sinT = sin(ray.theta);
    T cosT = cos(ray.theta);
    T sinP = sin(ray.phi);
    T cosP = cos(ray.phi);

    T dx = (T)dir.x, dy = (T)dir.y, dz = (T)dir.z;

    // Project Cartesian direction onto spherical basis.
    // e_r=(sinT*cosP, cosT, sinT*sinP), e_theta=(cosT*cosP, -sinT, cosT*sinP), e_phi=(-sinP, 0, cosP)
    ray.dr     =  sinT*cosP * dx  +  cosT        * dy  +  sinT*sinP * dz;
    ray.dtheta = (cosT*cosP * dx  -  sinT        * dy  +  cosT*sinP * dz) / ray.r;
    T st  = (fabs(sinT) < (T)1e-10) ? (T)1e-10 : sinT;
    ray.dphi   = (-sinP * dx + cosP * dz) / (ray.r * st);

    ray.L = ray.r * ray.r * sinT * ray.dphi;

    T f     = (T)1.0 - r_s / ray.r;
    T speed = sqrt(  ray.dr * ray.dr / f
                   + ray.r  * ray.r  * (ray.dtheta * ray.dtheta
                   + sinT   * sinT   *  ray.dphi   * ray.dphi));
    ray.E = f * speed;

    return ray;
}

// Right-hand side of the null-geodesic ODE system: dr/dLambda, dtheta/dLambda,
// dphi/dLambda (d1) and their second-derivative counterparts (d2).
template<typename T>
HD void geodesicRHS(const GeoRay<T>& ray, T r_s,
                    T d1[3], T d2[3])
{
    T r     = ray.r;
    T theta = ray.theta;
    T sinT  = sin(theta);
    T cosT  = cos(theta);
    if (fabs(sinT) < (T)1e-10) sinT = (sinT >= (T)0 ? (T)1e-10 : (T)-1e-10);

    T f  = (T)1.0 - r_s / r;
    T r2 = r * r;

    d1[0] = ray.dr;
    d1[1] = ray.dtheta;
    d1[2] = ray.dphi;

    d2[0] = -(r_s / ((T)2.0 * r2)) * (f * (ray.E/f) * (ray.E/f))
            + (r_s / ((T)2.0 * r2 * f)) * ray.dr * ray.dr
            + r * (ray.dtheta * ray.dtheta + sinT * sinT * ray.dphi * ray.dphi);

    d2[1] = -(T)2.0 * ray.dr * ray.dtheta / r
            + sinT * cosT * ray.dphi * ray.dphi;

    d2[2] = -(T)2.0 * ray.dr * ray.dphi / r
            - (T)2.0 * (cosT / sinT) * ray.dtheta * ray.dphi;
}

// Single fixed-step RK4 integration step.
template<typename T>
HD void rk4Step(GeoRay<T>& ray, T dL, T r_s) {
    T k1[3], l1[3];
    T k2[3], l2[3];
    T k3[3], l3[3];
    T k4[3], l4[3];

    // ---- k1 ----
    geodesicRHS(ray, r_s, k1, l1);

    // ---- k2 ----
    GeoRay<T> tmp = ray;
    tmp.r      += (T)0.5 * dL * k1[0];
    tmp.theta  += (T)0.5 * dL * k1[1];
    tmp.phi    += (T)0.5 * dL * k1[2];
    tmp.dr     += (T)0.5 * dL * l1[0];
    tmp.dtheta += (T)0.5 * dL * l1[1];
    tmp.dphi   += (T)0.5 * dL * l1[2];
    geodesicRHS(tmp, r_s, k2, l2);

    // ---- k3 ----
    tmp = ray;
    tmp.r      += (T)0.5 * dL * k2[0];
    tmp.theta  += (T)0.5 * dL * k2[1];
    tmp.phi    += (T)0.5 * dL * k2[2];
    tmp.dr     += (T)0.5 * dL * l2[0];
    tmp.dtheta += (T)0.5 * dL * l2[1];
    tmp.dphi   += (T)0.5 * dL * l2[2];
    geodesicRHS(tmp, r_s, k3, l3);

    // ---- k4 ----
    tmp = ray;
    tmp.r      += dL * k3[0];
    tmp.theta  += dL * k3[1];
    tmp.phi    += dL * k3[2];
    tmp.dr     += dL * l3[0];
    tmp.dtheta += dL * l3[1];
    tmp.dphi   += dL * l3[2];
    geodesicRHS(tmp, r_s, k4, l4);

    // Combine
    ray.r      += (dL / (T)6.0) * (k1[0] + (T)2.0*k2[0] + (T)2.0*k3[0] + k4[0]);
    ray.theta  += (dL / (T)6.0) * (k1[1] + (T)2.0*k2[1] + (T)2.0*k3[1] + k4[1]);
    ray.phi    += (dL / (T)6.0) * (k1[2] + (T)2.0*k2[2] + (T)2.0*k3[2] + k4[2]);
    ray.dr     += (dL / (T)6.0) * (l1[0] + (T)2.0*l2[0] + (T)2.0*l3[0] + l4[0]);
    ray.dtheta += (dL / (T)6.0) * (l1[1] + (T)2.0*l2[1] + (T)2.0*l3[1] + l4[1]);
    ray.dphi   += (dL / (T)6.0) * (l1[2] + (T)2.0*l2[2] + (T)2.0*l3[2] + l4[2]);

    // Rebuild Cartesian from spherical
    T sinT = sin(ray.theta);
    ray.x = ray.r * sinT * cos(ray.phi);
    ray.y = ray.r * cos(ray.theta);
    ray.z = ray.r * sinT * sin(ray.phi);
}
