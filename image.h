#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "png_write.h"

// HDR float RGB image buffer with Reinhard tone mapping + gamma-2.2 PNG export.
struct Image {
    int width, height;
    std::vector<float> data;

    Image(int w, int h) : width(w), height(h), data(w * h * 3, 0.0f) {}

    void setPixel(int x, int y, float r, float g, float b) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        int idx = (y * width + x) * 3;
        data[idx + 0] = r;
        data[idx + 1] = g;
        data[idx + 2] = b;
    }

    void writePNG(const std::string& path) const {
        std::vector<uint8_t> buf((size_t)width * height * 3);
        for (int i = 0; i < width * height * 3; i += 3) {
            float r = data[i], g = data[i+1], b = data[i+2];
            r = std::pow(std::max(0.0f, r / (1.0f + r)), 1.0f / 2.2f);
            g = std::pow(std::max(0.0f, g / (1.0f + g)), 1.0f / 2.2f);
            b = std::pow(std::max(0.0f, b / (1.0f + b)), 1.0f / 2.2f);
            buf[i]   = (uint8_t)(std::min(1.0f, r) * 255.0f);
            buf[i+1] = (uint8_t)(std::min(1.0f, g) * 255.0f);
            buf[i+2] = (uint8_t)(std::min(1.0f, b) * 255.0f);
        }
        png_write_rgb8(path, buf.data(), width, height);
    }
};
