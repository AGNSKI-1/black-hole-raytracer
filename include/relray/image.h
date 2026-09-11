#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "relray/vec3.h"

namespace relray {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<float> data;

    Image() = default;
    Image(int w, int h) : width(w), height(h), data(static_cast<size_t>(w * h * 3), 0.0f) {}

    void setPixel(int x, int y, float r, float g, float b) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        const int idx = (y * width + x) * 3;
        data[idx + 0] = r;
        data[idx + 1] = g;
        data[idx + 2] = b;
    }

    void setPixel(int x, int y, const Vec3& color) {
        setPixel(x, y, static_cast<float>(color.x), static_cast<float>(color.y), static_cast<float>(color.z));
    }
};

} // namespace relray
