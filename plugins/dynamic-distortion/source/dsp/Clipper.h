// Craft Dynamic Distortion - anti-aliased hard clipper (spec r1 section 4.4, review decision D1).
//
// Memoryless symmetric hard limit at +-c, evaluated with first-order antiderivative anti-aliasing (Parker,
// Zavalishin, Le Bivic, "Reducing the aliasing of nonlinear waveshaping using continuous-time convolution",
// DAFx 2016) at the oversampled rate. ADAA averages the clipper output over one sample, i.e. it applies
// (1 + z^-1) / 2 to unclipped signals; a 3-tap symmetric FIR (-1/8, 5/4, -1/8) restores the magnitude
// (|error| < 0.05 dB up to 20 kHz at 4x oversampling). Delay of the pair: 1.5 oversampled samples.
#pragma once

#include <cmath>

namespace craft::dd
{

class AdaaClipper
{
public:
    void setCeiling (double ceiling) noexcept { c = ceiling; }
    double getCeiling() const noexcept { return c; }

    void reset() noexcept
    {
        xPrev = 0.0;
        fPrev = 0.0;
        h1 = h2 = 0.0;
    }

    double hardClip (double x) const noexcept { return x > c ? c : (x < -c ? -c : x); }

    double process (double x) noexcept
    {
        const double f = antiderivative (x);
        const double dx = x - xPrev;
        double y;
        if (std::abs (dx) > 1.0e-7)
            y = (f - fPrev) / dx;
        else
            y = hardClip (0.5 * (x + xPrev)); // ill-conditioned difference: midpoint value
        xPrev = x;
        fPrev = f;
        // magnitude compensation of the one-sample average
        const double out = -0.125 * y + 1.25 * h1 - 0.125 * h2;
        h2 = h1;
        h1 = y;
        return out;
    }

private:
    double antiderivative (double x) const noexcept
    {
        const double ax = std::abs (x);
        return ax <= c ? 0.5 * x * x : c * ax - 0.5 * c * c;
    }

    double c = 0.446;
    double xPrev = 0.0, fPrev = 0.0, h1 = 0.0, h2 = 0.0;
};

} // namespace craft::dd
