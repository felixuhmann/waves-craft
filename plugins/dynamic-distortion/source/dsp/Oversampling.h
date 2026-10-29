// Craft Dynamic Distortion - 2x / 4x oversampling (shared design with the Craft Motion Filter) with linear-phase half-band FIR filters (Kaiser-windowed sinc).
// Reference: DAFX ch. "Sample-rate conversion"; Oppenheim/Schafer, "Discrete-Time Signal Processing" (Kaiser window).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace craft::dd
{

// Half-band low-pass (cut-off at a quarter of its own rate) with N = 4k + 3 taps.
template <int N>
class HalfbandFir
{
public:
    static constexpr int numTaps = N;
    static constexpr int centre = (N - 1) / 2; // group delay in samples of the filter's rate
    static_assert ((N - 3) % 4 == 0, "half-band length must be 4k + 3");

    static const std::array<double, N>& coefficients()
    {
        static const std::array<double, N> h = design();
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
        state[size_t (pos + N)] = x; // mirrored copy: contiguous window without modulo
        const double* w = &state[size_t (pos + 1)]; // oldest .. newest
        // every second tap is zero and the filter is symmetric
        double y = h[size_t (centre)] * w[centre];
        for (int m = 1; m <= centre; m += 2)
            y += h[size_t (centre + m)] * (w[centre + m] + w[centre - m]);
        pos = (pos + 1) % N;
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

    static std::array<double, N> design()
    {
        std::array<double, N> h {};
        constexpr double beta = 8.0, kPi = 3.14159265358979323846; // ~80 dB stop band
        double sum = 0.0;
        for (int n = 0; n < N; ++n)
        {
            const int m = n - centre;
            const double sinc = m == 0 ? 0.5 : std::sin (kPi * m / 2.0) / (kPi * m);
            const double r = double (m) / centre;
            h[size_t (n)] = sinc * besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
            sum += h[size_t (n)];
        }
        for (auto& c : h)
            c /= sum; // unity DC gain
        return h;
    }

    std::array<double, 2 * N> state {};
    int pos = 0;
};

// "Root half-band" low-pass: magnitude = sqrt of a Kaiser half-band's response, linear phase, N taps.
// Used for both the up- and the down-sampling filter of the outer stage, the round trip of a linear signal
// is |H(f)|^2 + |H(fs - f)|^2 = 1, i.e. flat up to the base Nyquist (a plain half-band pair loses ~6 dB there,
// e.g. at 22 kHz for a 44.1 kHz host). Image/alias rejection is ~50 dB from 0.62 fs on.
template <int N>
class RootHalfbandFir
{
public:
    static constexpr int numTaps = N;
    static constexpr int centre = (N - 1) / 2;

    static const std::array<double, N>& coefficients()
    {
        static const std::array<double, N> h = design();
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
        state[size_t (pos + N)] = x;
        const double* w = &state[size_t (pos + 1)];
        double y = h[size_t (centre)] * w[centre];
        for (int m = 1; m <= centre; ++m)
            y += h[size_t (centre + m)] * (w[centre + m] + w[centre - m]);
        pos = (pos + 1) % N;
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

    static std::array<double, N> design()
    {
        constexpr double kPi = 3.14159265358979323846;
        const auto& hb = HalfbandFir<N>::coefficients();
        // zero-phase amplitude of the half-band on a dense grid, its square root, inverse cosine transform
        constexpr int K = 4096;
        std::array<double, N> h {};
        std::vector<double> amp (K + 1);
        for (int i = 0; i <= K; ++i)
        {
            const double w = kPi * i / K;
            double a = hb[size_t (centre)];
            for (int m = 1; m <= centre; ++m)
                a += 2.0 * hb[size_t (centre + m)] * std::cos (w * m);
            amp[size_t (i)] = std::sqrt (std::max (0.0, a));
        }
        double sum = 0.0;
        for (int n = 0; n < N; ++n)
        {
            const int m = n - centre;
            double acc = 0.0;
            for (int i = 0; i <= K; ++i) // trapezoidal rule
                acc += (i == 0 || i == K ? 0.5 : 1.0) * amp[size_t (i)] * std::cos (kPi * i / K * m);
            const double r = double (m) / centre;
            const double win = besselI0 (3.0 * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (3.0);
            h[size_t (n)] = acc / K * win;
            sum += h[size_t (n)];
        }
        for (auto& c : h)
            c /= sum;
        return h;
    }

    std::array<double, 2 * N> state {};
    int pos = 0;
};

// One 2x stage: up (1 -> 2 samples) and down (2 -> 1 samples) for one channel.
template <typename Fir>
struct Stage2x
{
    Fir upFir, downFir;
    void reset() noexcept { upFir.reset(); downFir.reset(); }
    void up (double x, double& y0, double& y1) noexcept
    {
        y0 = upFir.process (2.0 * x);
        y1 = upFir.process (0.0);
    }
    double down (double x0, double x1) noexcept
    {
        const double y = downFir.process (x0);
        downFir.process (x1);
        return y;
    }
};

// Up/down sampler by 1, 2 or 4 with an integer base-rate latency:
//  2x: 63-tap root-half-band stage, 2 x 31 high-rate samples = 31 base samples.
//  4x: outer 63-tap stage (31 base samples) + inner 23-tap stage (2 x 11 + 2 padding = 24 4x-samples = 6 base).
class Oversampler
{
public:
    void setFactor (int f) noexcept { factor = (f >= 4) ? 4 : (f >= 2 ? 2 : 1); }
    int getFactor() const noexcept { return factor; }
    int latency() const noexcept { return factor == 1 ? 0 : (factor == 2 ? outerLatency : outerLatency + innerLatency); }

    void reset() noexcept
    {
        outer.reset();
        inner0.reset();
        inner1.reset();
        pad.fill (0.0);
        padPos = 0;
    }

    // base sample -> factor samples in out[]
    void up (double x, double* out) noexcept
    {
        if (factor == 1) { out[0] = x; return; }
        double a, b;
        outer.up (x, a, b);
        if (factor == 2) { out[0] = a; out[1] = b; return; }
        inner0.up (a, out[0], out[1]);
        inner0.up (b, out[2], out[3]); // inner0 handles the up path, inner1 the down path
    }

    // factor samples -> base sample
    double down (const double* in) noexcept
    {
        if (factor == 1) return in[0];
        if (factor == 2) return outer.down (in[0], in[1]);
        // 2 samples of padding at 4x keep the total latency an integer number of base samples
        double s[4];
        for (int k = 0; k < 4; ++k)
        {
            s[k] = pad[size_t (padPos)];
            pad[size_t (padPos)] = in[k];
            padPos = (padPos + 1) % 2;
        }
        const double a = inner1.down (s[0], s[1]);
        const double b = inner1.down (s[2], s[3]);
        return outer.down (a, b);
    }

private:
    static constexpr int outerLatency = 31, innerLatency = 6;
    int factor = 2;
    Stage2x<RootHalfbandFir<63>> outer;
    Stage2x<HalfbandFir<23>> inner0, inner1;
    std::array<double, 2> pad {};
    int padPos = 0;
};

} // namespace craft::dd
