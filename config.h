#pragma once

// config.h — default tunables. All of these can be overridden at runtime
// via command-line flags (see main.cpp -h).

// Default image resolution
#define CFG_WIDTH  400
#define CFG_HEIGHT 300

// Default geodesic integration step size and step budget
#define CFG_MAX_STEPS 100000
#define CFG_D_LAMBDA  4e7

// Default camera placement
#define CFG_CAM_RADIUS    1.2693e12
#define CFG_CAM_AZIMUTH   0.0
#define CFG_CAM_ELEVATION 83.0
#define CFG_CAM_FOV       12.0

// Default output file
#define CFG_OUTPUT_FILE "output.png"
