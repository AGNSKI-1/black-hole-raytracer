#pragma once
#include "camera.h"
#include "scene.h"
#include "image.h"
#include "renderer.h"

// GPU render entry point (implemented in renderer.cu).
Image renderGPU(const Camera& cam, const Scene& scene, const RenderParams& params);
