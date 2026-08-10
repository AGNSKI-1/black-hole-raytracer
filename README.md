# black-hole-raytracer

A GPU-accelerated relativistic raytracer that renders a Schwarzschild black
hole with a volumetric accretion disk by numerically integrating null
geodesics. Each pixel is one independent photon path traced backward from
the camera through curved spacetime — embarrassingly parallel, and a good
fit for a GPU. Includes both a CUDA renderer and an OpenMP CPU reference
implementation that produce matching output.

## Features

- Schwarzschild (non-rotating) black hole geodesics integrated with
  fixed-step RK4 in spherical coordinates, conserving each photon's energy
  and angular momentum.
- Volumetric accretion disk with relativistic Doppler beaming and
  gravitational redshift, procedural fBm turbulence, and Henyey-Greenstein
  scattering.
- Optional foreground spheres (e.g. a companion star) to demonstrate
  gravitational lensing near the photon ring.
- CUDA kernel (one thread per pixel) with scene data in `__constant__`
  memory, structure-of-arrays output for coalesced writes, and warp-level
  termination masking via `__ballot_sync` to reduce divergence.
- OpenMP CPU renderer sharing the same geodesic/shading code, used to
  validate the GPU output pixel-for-pixel.
- Self-contained PNG writer — no external image or graphics libraries.
- Simple CLI for resolution, integration parameters, camera placement, and
  a scripted orbit/animation mode for rendering video frames.

## Physics background

Photon trajectories are computed as null geodesics of the Schwarzschild
metric,

```
ds^2 = -(1 - r_s/r) c^2 dt^2 + dr^2/(1 - r_s/r) + r^2 (dtheta^2 + sin^2(theta) dphi^2)
```

where `r_s = 2GM/c^2` is the Schwarzschild radius. Each ray is initialized
in Cartesian space from the camera, projected onto a local spherical basis,
and advanced in the affine parameter with RK4 while conserving the
geodesic's energy `E` and angular momentum `L`. See
[`geodesic.h`](geodesic.h) for the integrator and
[`renderer.h`](renderer.h) / [`renderer.cu`](renderer.cu) for how hits
against the event horizon, accretion disk, and scene objects are resolved
along the path.

## Building

Requires a C++11 compiler; the GPU build additionally requires the CUDA
toolkit (`nvcc`).

```bash
make cpu          # OpenMP CPU build -> raytracer_cpu, no CUDA required
make gpu          # CUDA build       -> raytracer_gpu
make gpu GPU_ARCH=sm_80   # target a specific compute capability (see Makefile)
```

## Running

```bash
./raytracer_gpu                       # renders output.png with config.h defaults
./raytracer_gpu -W 1920 -H 1080 -o render.png
./raytracer_cpu -W 400 -H 300         # fast low-res CPU sanity check
```

Key flags (see `-h` for the full list):

| Flag | Meaning | Default |
|------|---------|---------|
| `-W`, `-H` | image resolution | 400 x 300 |
| `-s` | max RK4 integration steps per ray | 100000 |
| `-d` | affine step size (dLambda) | 4e7 |
| `-r`, `-a`, `-e`, `-f` | camera radius / azimuth / elevation / FOV | see `config.h` |
| `-n`, `-N` | render frame `n` of `N` in the built-in orbit animation | off |
| `-o` | output PNG path | `output.png` |

Edit [`config.h`](config.h) to change the defaults, or
[`scene.h`](scene.h) to change the black hole mass, disk geometry, or
foreground objects.

### Animation mode

Passing `-n <frame> -N <total>` drives the camera through a scripted orbit
(an equatorial pass followed by a rise over the pole) and rotates the
accretion disk accordingly, naming output frames `frame_0000.png`,
`frame_0001.png`, etc. Stitch frames into a video with `ffmpeg`:

```bash
ffmpeg -framerate 60 -i frames/frame_%04d.png -c:v libx264 -pix_fmt yuv420p -crf 18 out.mp4
```

## Validating the GPU output

Both renderers share the same physics and shading code, so a CPU and GPU
render of the same scene/camera should be visually identical (the GPU path
runs in single precision, so expect small numerical differences). This is
useful both as a correctness check and as a way to measure GPU speedup —
the GPU build is typically tens to a few hundred times faster than the CPU
build at the same resolution, depending on hardware and scene complexity.

## Project layout

```
config.h        Tunable defaults: resolution, integration params, camera, output path
main.cpp        CLI parsing, scene/camera setup, dispatch to CPU or GPU renderer
vec3.h          3D vector math (host + device)
camera.h        Spherical-orbit camera, ray generation
scene.h         BlackHole, AccretionDisk, SphereObject definitions
geodesic.h      Schwarzschild RK4 null-geodesic integrator (templated, host + device)
disk_noise.h    fBm value noise + Henyey-Greenstein phase function
renderer.h      CPU (OpenMP) renderer + shared ray/disk/sphere shading logic
renderer_gpu.h  GPU renderer declaration
renderer.cu     CUDA kernel + host wrapper
image.h         HDR float image buffer, Reinhard tone mapping
png_write.h     Minimal dependency-free PNG encoder
Makefile        make cpu / make gpu
```

## License

MIT — see [LICENSE](LICENSE).
