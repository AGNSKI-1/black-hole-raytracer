#pragma once

#include <algorithm>
#include <cmath>

#include "relray/camera.h"
#include "relray/image.h"
#include "relray/scene.h"

namespace relray {

struct RenderConfig {
    int width = 160;
    int height = 120;
    double horizonStrength = 0.85;
};

inline Image renderPreview(const Camera& cam, const Scene& scene, const RenderConfig& config = RenderConfig{}) {
    Image img(config.width, config.height);
    const auto origin = cam.position();

    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            const double u = 2.0 * (x + 0.5) / config.width - 1.0;
            const double v = -(2.0 * (y + 0.5) / config.height - 1.0);
            const auto dir = cam.generateRay(u, v);

            const double sky = 0.15 + 0.85 * std::max(0.0, dir.y);
            const double glow = std::max(0.0, 1.0 - std::sqrt(dir.x * dir.x + dir.z * dir.z));

            double r = 0.15 + sky * 0.55;
            double g = 0.19 + sky * 0.68;
            double b = 0.35 + sky * 0.85;

            if (scene.objects.size() > 0) {
                const auto& obj = scene.objects.front();
                const Vec3 toCenter = obj.center - origin;
                const double centerDot = dir.dot(toCenter.normalize());
                const double body = std::max(0.0, 1.0 - std::abs(centerDot));
                r += body * 0.25;
                g += body * 0.18;
                b += body * 0.12;
            }

            const double accent = config.horizonStrength * glow;
            r += accent * 0.80;
            g += accent * 0.55;
            b += accent * 1.10;

            img.setPixel(x, y, Vec3(std::clamp(r, 0.0, 1.0), std::clamp(g, 0.0, 1.0), std::clamp(b, 0.0, 1.0)));
        }
    }

    return img;
}

} // namespace relray
