#include <iostream>
#include <string>
#include <cstdlib>
#include <stdexcept>
#include <chrono>

#include "config.h"
#include "camera.h"
#include "scene.h"
#include "image.h"
#include "renderer.h"

#ifdef USE_CUDA
#include "renderer_gpu.h"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void usage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  -o <file>      Output file (.png)           (default: " CFG_OUTPUT_FILE ")\n"
        << "  -W <int>       Image width in pixels        (default: " << CFG_WIDTH        << ")\n"
        << "  -H <int>       Image height in pixels       (default: " << CFG_HEIGHT       << ")\n"
        << "  -s <int>       Max integration steps        (default: " << CFG_MAX_STEPS    << ")\n"
        << "  -d <float>     Affine step size dLambda     (default: " << CFG_D_LAMBDA     << ")\n"
        << "  -r <float>     Camera radius (metres)       (default: " << CFG_CAM_RADIUS    << ")\n"
        << "  -a <float>     Camera azimuth (degrees)     (default: " << CFG_CAM_AZIMUTH   << ")\n"
        << "  -e <float>     Camera elevation (degrees)   (default: " << CFG_CAM_ELEVATION << ")\n"
        << "  -f <float>     Vertical FOV (degrees)       (default: " << CFG_CAM_FOV       << ")\n"
        << "\n"
        << "Animation (video frame mode):\n"
        << "  -n <int>       Frame index (0-based).  Enables animation mode:\n"
        << "                   frames 0..(2/3)*N  — equatorial orbit  (elevation ~83 deg)\n"
        << "                   frames (2/3)*N..N  — rise over the pole (elevation 83->15 deg)\n"
        << "                 Azimuth and diskPhase are derived automatically.\n"
        << "                 When -o is at its default, output is named frame_XXXX.png.\n"
        << "                 Camera/azimuth/elevation flags are ignored in animation mode.\n"
        << "  -N <int>       Total frame count            (default: 1800 = 30 s @ 60 fps)\n"
        << "\n"
        << "  -q             Quiet mode (no stdout)\n"
        << "  -h             Print this help\n";
    std::exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {

    // Defaults from config.h
    std::string outputPath  = CFG_OUTPUT_FILE;
    RenderParams params;
    params.width    = CFG_WIDTH;
    params.height   = CFG_HEIGHT;
    params.maxSteps = CFG_MAX_STEPS;
    params.dLambda  = CFG_D_LAMBDA;

    double camRadius    = CFG_CAM_RADIUS;
    double camAzimuth   = CFG_CAM_AZIMUTH;
    double camElevation = CFG_CAM_ELEVATION;
    double camFov       = CFG_CAM_FOV;

    int frameIndex  = -1;
    int totalFrames = 1800; // 30 s @ 60 fps

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto nextStr = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << arg << "\n";
                usage(argv[0]);
            }
            return argv[++i];
        };
        if      (arg == "-o") outputPath       = nextStr();
        else if (arg == "-W") params.width      = std::atoi(nextStr());
        else if (arg == "-H") params.height     = std::atoi(nextStr());
        else if (arg == "-s") params.maxSteps   = std::atoi(nextStr());
        else if (arg == "-d") params.dLambda    = std::atof(nextStr());
        else if (arg == "-r") camRadius         = std::atof(nextStr());
        else if (arg == "-a") camAzimuth        = std::atof(nextStr());
        else if (arg == "-e") camElevation      = std::atof(nextStr());
        else if (arg == "-f") camFov            = std::atof(nextStr());
        else if (arg == "-n") frameIndex        = std::atoi(nextStr());
        else if (arg == "-N") totalFrames       = std::atoi(nextStr());
        else if (arg == "-q") params.quiet      = true;
        else if (arg == "-h") usage(argv[0]);
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            usage(argv[0]);
        }
    }

    // Animation mode: derive camera azimuth/elevation and disk rotation
    // phase from the frame index instead of the -r/-a/-e flags.
    if (frameIndex >= 0) {
        if (totalFrames < 1) { std::cerr << "Total frames must be >= 1\n"; return 1; }
        if (frameIndex >= totalFrames) {
            std::cerr << "Frame index " << frameIndex
                      << " out of range [0, " << totalFrames - 1 << "]\n";
            return 1;
        }

        const double PHASE_SPLIT   = 2.0 / 3.0; // fraction of the video spent orbiting
        const double N_DISK_ORBITS = 10.0;      // disk rotations over the full video

        double t = (double)frameIndex / (double)totalFrames; // in [0, 1)

        if (t < PHASE_SPLIT) {
            // Phase 1: equatorial orbit
            double t1    = t / PHASE_SPLIT;
            camAzimuth   = t1 * 360.0;
            camElevation = 83.0;
        } else {
            // Phase 2: rise over the pole
            double t2     = (t - PHASE_SPLIT) / (1.0 - PHASE_SPLIT);
            double smooth = t2 * t2 * (3.0 - 2.0 * t2);
            camAzimuth    = 360.0 + t2 * 180.0;
            camElevation  = 83.0 + smooth * (15.0 - 83.0);
        }

        params.diskPhase = t * 2.0 * M_PI * N_DISK_ORBITS;

        // Auto-name output when -o wasn't explicitly given.
        if (outputPath == CFG_OUTPUT_FILE) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "frame_%04d.png", frameIndex);
            outputPath = buf;
        }
    }

    if (params.width  <= 0) { std::cerr << "Width must be positive\n";  return 1; }
    if (params.height <= 0) { std::cerr << "Height must be positive\n"; return 1; }

    double aspect = (double)params.width / (double)params.height;

    Camera cam(
        camRadius,
        camAzimuth   * M_PI / 180.0,
        camElevation * M_PI / 180.0,
        camFov,
        aspect
    );

    Scene scene;

    if (!params.quiet) std::cout << "=== Black Hole Raytracer ===\n"
              << "  Renderer   : "
#ifdef USE_CUDA
              << "GPU (CUDA)\n"
#elif defined(_OPENMP)
              << "CPU (OpenMP)\n"
#else
              << "CPU (serial)\n"
#endif
              << "  Resolution : " << params.width << " x " << params.height << "\n"
              << "  Max steps  : " << params.maxSteps << "\n"
              << "  dLambda    : " << params.dLambda  << "\n"
              << "  Elevation  : " << camElevation << " deg\n"
              << "  Azimuth    : " << camAzimuth   << " deg\n"
              << "  FOV        : " << camFov << " deg\n"
              << "  Camera pos : ("
                  << cam.position().x << ", "
                  << cam.position().y << ", "
                  << cam.position().z << ")\n";
    if (frameIndex >= 0) {
        std::cout << "  Frame      : " << frameIndex << " / " << totalFrames - 1 << "\n"
                  << "  Disk phase : " << params.diskPhase << " rad\n";
    }
    std::cout << "  Output     : " << outputPath << "\n\n";

    auto t0 = std::chrono::high_resolution_clock::now();

#ifdef USE_CUDA
    Image img = renderGPU(cam, scene, params);
#else
    Image img = renderCPU(cam, scene, params);
#endif

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    if (!params.quiet)
        std::cout << "  Render time : " << elapsed << " s\n"
                  << "  Throughput  : "
                  << (double)(params.width * params.height) / elapsed / 1.0e6
                  << " Mrays/s\n";

    try {
        img.writePNG(outputPath);
        if (!params.quiet) std::cout << "  Wrote " << outputPath << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error writing file: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
