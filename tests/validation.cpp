#include <cmath>
#include <iostream>

#include "relray/metric.h"

int main() {
    relray::Schwarzschild metric(1.0);

    const double r = 3.0;
    const double theta = M_PI / 2.0;
    const auto g = metric.metric_diagonal(r, theta);
    const double photon_sphere = 3.0 * metric.mass();

    if (std::abs(g[0] + 1.0 / 3.0) > 1e-12) {
        std::cerr << "metric_diagonal mismatch\n";
        return 1;
    }

    if (std::abs(photon_sphere - 3.0) > 1e-12) {
        std::cerr << "photon sphere test failed\n";
        return 1;
    }

    if (!metric.is_horizon(2.0)) {
        std::cerr << "horizon check failed\n";
        return 1;
    }

    std::cout << "validation ok\n";
    return 0;
}
