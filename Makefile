# Makefile — Schwarzschild black-hole raytracer
#
# Usage:
#   make gpu     # CUDA build (raytracer_gpu)
#   make cpu     # CPU-only build, no CUDA required (raytracer_cpu)
#
# GPU_ARCH selects the target compute capability. Common values:
#   sm_60  Pascal   (e.g. P100)
#   sm_70  Volta    (e.g. V100)
#   sm_75  Turing   (e.g. RTX 20xx / T4)      <- default
#   sm_80  Ampere   (e.g. A100)
#   sm_86  Ampere   (e.g. RTX 30xx)
#   sm_89  Ada      (e.g. RTX 40xx)
GPU_ARCH ?= sm_75

# Set to your host compiler if nvcc needs an older/newer g++ than default,
# e.g. CCBIN=g++-9. Leave empty to use nvcc's default.
CCBIN ?=

NVCC = nvcc
CXX  = g++

CCBIN_FLAG = $(if $(CCBIN),-ccbin $(CCBIN),)

NVCC_FLAGS = -O3 -arch=$(GPU_ARCH) -std=c++11 \
             --expt-relaxed-constexpr \
             --use_fast_math \
             --ptxas-options=-v \
             -Xcompiler -fopenmp \
             $(CCBIN_FLAG)

CPU_FLAGS  = -O3 -std=c++11 -fopenmp -march=native

HEADERS = config.h vec3.h camera.h scene.h geodesic.h disk_noise.h image.h png_write.h renderer.h

.PHONY: all gpu cpu clean run_gpu run_cpu

all: gpu

# GPU build — nvcc compiles main.cpp + renderer.cu together
gpu: main.cpp renderer.cu renderer_gpu.h $(HEADERS)
	$(NVCC) $(NVCC_FLAGS) -DUSE_CUDA -o raytracer_gpu main.cpp renderer.cu
	@echo "Built: raytracer_gpu  (arch=$(GPU_ARCH))"

# CPU-only build
cpu: main.cpp $(HEADERS)
	$(CXX) $(CPU_FLAGS) -o raytracer_cpu main.cpp
	@echo "Built: raytracer_cpu"

run_gpu: gpu
	./raytracer_gpu

run_cpu: cpu
	./raytracer_cpu

clean:
	rm -f raytracer_gpu raytracer_cpu output.png *.o
