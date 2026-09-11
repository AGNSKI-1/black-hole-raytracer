#pragma once

#include <cmath>

namespace relray {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    constexpr Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    constexpr Vec3 operator*(double scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    constexpr Vec3 operator/(double scalar) const { return {x / scalar, y / scalar, z / scalar}; }

    Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    Vec3& operator*=(double scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }

    double dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
    Vec3 cross(const Vec3& other) const {
        return {y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x};
    }
    double lengthSq() const { return x * x + y * y + z * z; }
    double length() const { return std::sqrt(lengthSq()); }
    Vec3 normalize() const {
        const double len = length();
        if (len <= 1e-12) return {0.0, 0.0, 0.0};
        return *this / len;
    }
};

inline Vec3 operator*(double scalar, const Vec3& v) { return v * scalar; }

} // namespace relray
