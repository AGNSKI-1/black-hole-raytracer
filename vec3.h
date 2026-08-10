#pragma once
#include <cmath>

// HD expands to __host__ __device__ __forceinline__ under nvcc so every
// function below compiles for both the CPU and GPU renderers from one
// source file; under a plain C++ compiler it's just `inline`.
#ifdef __CUDACC__
#  define HD __host__ __device__ __forceinline__
#else
#  define HD inline
#endif

struct Vec3 {
    double x, y, z;

    HD Vec3() : x(0), y(0), z(0) {}
    HD Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

    HD Vec3 operator+(const Vec3& o) const { return Vec3(x+o.x, y+o.y, z+o.z); }
    HD Vec3 operator-(const Vec3& o) const { return Vec3(x-o.x, y-o.y, z-o.z); }
    HD Vec3 operator*(double t)      const { return Vec3(x*t,   y*t,   z*t);   }
    HD Vec3 operator/(double t)      const { return Vec3(x/t,   y/t,   z/t);   }
    HD Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
    HD Vec3& operator*=(double t)      { x*=t;   y*=t;   z*=t;   return *this; }

    HD double dot(const Vec3& o)   const { return x*o.x + y*o.y + z*o.z; }
    HD Vec3   cross(const Vec3& o) const {
        return Vec3(y*o.z - z*o.y,
                    z*o.x - x*o.z,
                    x*o.y - y*o.x);
    }
    HD double lengthSq() const { return x*x + y*y + z*z; }
    HD double length()   const { return sqrt(lengthSq()); }
    HD Vec3   normalize() const { return *this / length(); }
};

HD Vec3 operator*(double t, const Vec3& v) { return v * t; }
