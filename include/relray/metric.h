#pragma once

#include <array>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace relray {

struct MetricPoint {
    double t = 0.0;
    double r = 0.0;
    double theta = 0.0;
    double phi = 0.0;
};

class Metric {
public:
    virtual ~Metric() = default;
    virtual double mass() const noexcept = 0;
    virtual double horizon_radius() const noexcept = 0;
    virtual std::array<double, 4> metric_diagonal(double r, double theta) const = 0;
    virtual std::array<std::array<std::array<double, 4>, 4>, 4> christoffel(double r, double theta) const = 0;
    virtual bool is_horizon(double r) const noexcept = 0;
};

class Schwarzschild final : public Metric {
public:
    explicit Schwarzschild(double mass = 1.0) noexcept : mass_(mass), horizon_(2.0 * mass) {}

    double mass() const noexcept override { return mass_; }
    double horizon_radius() const noexcept override { return horizon_; }

    std::array<double, 4> metric_diagonal(double r, double theta) const override {
        const double s = std::sin(theta);
        const double f = 1.0 - horizon_ / r;
        return {-f, 1.0 / f, r * r, r * r * s * s};
    }

    std::array<std::array<std::array<double, 4>, 4>, 4> christoffel(double r, double theta) const override {
        std::array<std::array<std::array<double, 4>, 4>, 4> gamma{};
        const double s = std::sin(theta);
        const double c = std::cos(theta);
        const double f = 1.0 - horizon_ / r;

        gamma[0][0][1] = 0.5 * (horizon_ / (r * r)) * (1.0 - horizon_ / r);
        gamma[1][0][0] = gamma[0][0][1] / f;
        gamma[1][1][1] = -0.5 * (horizon_ / (r * (r - horizon_)));
        gamma[1][2][2] = -(r - horizon_);
        gamma[1][3][3] = -(r - horizon_) * s * s;
        gamma[2][1][2] = gamma[2][2][1] = 1.0 / r;
        gamma[2][3][3] = -s * c;
        gamma[3][1][3] = gamma[3][3][1] = 1.0 / r;
        gamma[3][2][3] = gamma[3][3][2] = c / s;

        gamma[0][1][0] = gamma[0][0][1];
        gamma[0][0][1] = gamma[1][0][0];
        gamma[1][1][0] = gamma[1][0][0];
        gamma[1][0][1] = gamma[1][1][0];
        gamma[2][2][1] = gamma[1][2][2];
        gamma[3][3][1] = gamma[1][3][3];
        gamma[3][3][2] = gamma[2][3][3];

        return gamma;
    }

    bool is_horizon(double r) const noexcept override { return r <= horizon_; }

private:
    double mass_;
    double horizon_;
};

} // namespace relray
