# Relativistic Ray Tracer

A compact C++ package for rendering Schwarzschild spacetime with null-geodesic integration. The physics in this repository is kept from the original teaching/HPC ray tracer, while the project layout, build flow, and API surface are organized to behave like a reusable GitHub package rather than a single-file demo.

This is not a production astrophysics pipeline or a GRMHD codebase; it is a small general-relativity visualization package for teaching, exploration, and metric prototyping.

## Quick start

```bash
git clone <repo-url>
cd black-hole-raytracer
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/Release/relray_render_example.exe
```

## Example usage

```cpp
#include "relray/relray.h"

int main() {
    relray::Camera cam(1.2693e12, 0.0, 83.0 * M_PI / 180.0, 12.0, 4.0 / 3.0);
    relray::Scene scene;
    relray::RenderConfig config{160, 120, 0.85};
    relray::Image img = relray::renderPreview(cam, scene, config);
    (void)img;
}
```

## Conventions

- Metric signature: `(-,+,+,+)`
- Units: geometrized units with `G = c = 1`; `M` sets the length scale
- Christoffel indexing follows the standard coordinate basis for the metric `diag(-f, 1/f, r^2, r^2 sin^2 theta)`
- The affine parameter is integrated along the ray path in the direction away from the observer

## Project layout

```text
include/relray/     Public headers and package API
examples/           Demonstration executables
tests/              Validation checks
CMakeLists.txt      Build, install, and export configuration
README.md           Project docs
LICENSE             Repository license
```

## Validation

The package includes a metric seam in [include/relray/metric.h](include/relray/metric.h) and a validation check for canonical Schwarzschild behavior, including the horizon and the photon-sphere scale:

- horizon at `r = 2M`
- photon sphere at `r = 3M`
- consistency with the expected metric diagonal and sign conventions

Run the validation target with:

```bash
ctest --test-dir build -C Release --output-on-failure
```

## Limitations

- Schwarzschild only; no Kerr or additional metrics yet
- Offline rendering only; not a real-time viewer
- The coordinate singularity at `r = 2M` is still a documented limitation in Schwarzschild coordinates
- This is a learning and prototyping package, not a production physics engine

## Extending the metric

Adding a new metric means implementing the interface in [include/relray/metric.h](include/relray/metric.h) and reusing the same geodesic conventions. The project is intentionally small enough for a second metric to be a straightforward additive change rather than a rewrite.
