// Craft Motion Filter - 2x oversampling with linear-phase half-band FIR filters (Kaiser-windowed sinc).
// Reference: DAFX ch. "Sample-rate conversion"; Oppenheim/Schafer, "Discrete-Time Signal Processing" (Kaiser window).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace craft::mf
{

class HalfbandFir
{
public:
    static constexpr int numTaps = 63;                 // 4k + 3 taps: integer base-rate latency
    static constexpr int centre = (numTaps - 1) / 2;   // group delay at the high rate (31)

    HalfbandFir() { std::fill (state.begin(), state.end(), 0.0); }

    static const std::array<double, numTaps>& coefficients()
    {
        static const std::array<double, numTaps> h = design();
        return h;
    }

    void reset() noexcept
    {
        std::fill (state.begin(), state.end(), 0.0);
        pos = 0;
    }

    double process (double x) noexcept
    {
        const auto& h = coefficients();
        state[size_t (pos)] = x;
        state[size_t (pos + numTaps)] = x; // mirrored copy: contiguous window without modulo
        const double* w = &state[size_t (pos + 1)]; // oldest .. newest
        // half-band: every second tap is zero, and the filter is symmetric
        double y = h[size_t (centre)] * w[centre];
        for (int m = 1; m <= centre; m += 2)
            y += h[size_t (centre + m)] * (w[centre + m] + w[centre - m]);
        pos = (pos + 1) % numTaps;
        return y;
    }

private:
    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
        }
        return sum;
    }

    // Half-band low-pass (cut-off at a quarter of the high rate), Kaiser beta 8 (~80 dB stop band).
    static std::array<double, numTaps> design()
    {
        std::array<double, numTaps> h {};
        constexpr double beta = 8.0, pi = 3.14159265358979323846;
        double sum = 0.0;
        for (int n = 0; n < numTaps; ++n)
        {
            const int m = n - centre;
            const double sinc = m == 0 ? 0.5 : std::sin (pi * m / 2.0) / (pi * m);
            const double r = double (m) / centre;
            const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
            h[size_t (n)] = sinc * w;
            sum += h[size_t (n)];
        }
        for (auto& c : h)
            c /= sum; // unity DC gain
        return h;
    }

    std::array<double, 2 * numTaps> state {};
    int pos = 0;
};

// 2x up-sampler: one input sample -> two output samples.
struct Upsampler2x
{
    HalfbandFir fir;
    void reset() noexcept { fir.reset(); }
    void process (double x, double& y0, double& y1) noexcept
    {
        y0 = fir.process (2.0 * x);
        y1 = fir.process (0.0);
    }
};

// 2x down-sampler: two input samples -> one output sample.
struct Downsampler2x
{
    HalfbandFir fir;
    void reset() noexcept { fir.reset(); }
    double process (double x0, double x1) noexcept
    {
        const double y = fir.process (x0);
        fir.process (x1);
        return y;
    }
};

// Base-rate latency of an up/down pair.
constexpr int oversamplingLatency = HalfbandFir::centre; // 2 x 31 high-rate samples = 31 base samples

} // namespace craft::mf
