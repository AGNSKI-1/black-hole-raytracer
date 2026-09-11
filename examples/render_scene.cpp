#include <iostream>

#include "relray/relray.h"

int main() {
    relray::Camera cam(1.2693e12, 0.0, 83.0 * M_PI / 180.0, 12.0, 4.0 / 3.0);
    relray::Scene scene;

    const relray::RenderConfig config{160, 120, 0.85};
    const relray::Image image = relray::renderPreview(cam, scene, config);

    std::cout << "Example render generated for " << image.width << "x" << image.height << "\n";
    return 0;
}
