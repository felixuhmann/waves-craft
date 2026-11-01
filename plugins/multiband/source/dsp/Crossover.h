// Craft Multiband - allpass-complementary 4-band crossover (spec r2 section 3).
// Host-independent: no JUCE dependency.
//
// Each crossover k uses one second-order denominator D_k(s) = s^2 + s w0/Q + w0^2 (analog prototype normalised to
// f_k, bilinear transform pre-warped at f_k):
//     LP_k = (w0^2 / D_k)^2,   AP_k = (s^2 - s w0/Q + w0^2) / D_k,   HP_k = AP_k - LP_k
// Bands: B1 = LP1 AP2 AP3, B2 = HP1 LP2 AP3, B3 = HP1 HP2 LP3, B4 = HP1 HP2 HP3, so B1+B2+B3+B4 = AP1 AP2 AP3.
#pragma once

#include <array>
#include <cmath>
#include <complex>

#include "Parameters.h"

namespace craft::multiband
{

inline constexpr double pi = 3.14159265358979323846;

// w0(Q): the w0 > 0 for which the second-order lowpass w0^2 / D is -3.01 dB (|.| = 1/sqrt 2) at s = j.
// With u = w0^2:  2u^2 = (u - 1)^2 + u/Q^2  =>  u^2 + (2 - 1/Q^2) u - 1 = 0.
inline double crossoverW0 (double q) noexcept
{
    const double b = 2.0 - 1.0 / (q * q);
    const double u = 0.5 * (-b + std::sqrt (b * b + 4.0));
    return std::sqrt (u);
}

// Digital biquad coefficients (a0 normalised to 1).
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    // Bilinear transform of (n0 s^2 + n1 s + n2) / (s^2 + d1 s + d2) with s = (1/K) (1 - z^-1)/(1 + z^-1).
    static BiquadCoeffs fromAnalog (double n0, double n1, double n2, double d1, double d2, double K) noexcept
    {
        const double K2 = K * K;
        const double a0 = 1.0 + d1 * K + d2 * K2;
        BiquadCoeffs c;
        c.b0 = (n0 + n1 * K + n2 * K2) / a0;
        c.b1 = (-2.0 * n0 + 2.0 * n2 * K2) / a0;
        c.b2 = (n0 - n1 * K + n2 * K2) / a0;
        c.a1 = (-2.0 + 2.0 * d2 * K2) / a0;
        c.a2 = (1.0 - d1 * K + d2 * K2) / a0;
        return c;
    }

    std::complex<double> response (double freqHz, double fs) const noexcept
    {
        const auto z1 = std::polar (1.0, -2.0 * pi * freqHz / fs); // z^-1
        const auto z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

// Transposed direct form II, double precision.
struct Biquad
{
    BiquadCoeffs c;
    double s1 = 0.0, s2 = 0.0;

    void reset() noexcept { s1 = s2 = 0.0; }

    double process (double x) noexcept
    {
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

// Coefficients of one crossover section.
struct CrossoverSection
{
    BiquadCoeffs lp; // w0^2 / D (applied twice -> LP_k)
    BiquadCoeffs ap; // (s^2 - s w0/Q + w0^2) / D
    double g = 0.1, k = 1.0; // the same section as a trapezoidal state-variable filter: g = w0 tan(pi f_k/fs), k = 1/Q

    static CrossoverSection design (double freqHz, double q, double fs) noexcept
    {
        const double fMax = 0.49 * fs;
        const double f = freqHz < 1.0 ? 1.0 : (freqHz > fMax ? fMax : freqHz);
        const double K = std::tan (pi * f / fs);
        const double w0 = crossoverW0 (q);
        const double d1 = w0 / q, d2 = w0 * w0;
        CrossoverSection s;
        s.lp = BiquadCoeffs::fromAnalog (0.0, 0.0, d2, d1, d2, K);
        s.ap = BiquadCoeffs::fromAnalog (1.0, -d1, d2, d1, d2, K);
        s.g = w0 * K;
        s.k = 1.0 / q;
        return s;
    }

    std::complex<double> lowpass (double f, double fs) const noexcept { const auto h = lp.response (f, fs); return h * h; }
    std::complex<double> allpass (double f, double fs) const noexcept { return ap.response (f, fs); }
    std::complex<double> highpass (double f, double fs) const noexcept { return allpass (f, fs) - lowpass (f, fs); }
};

// Complex responses of the four bands for a set of crossover sections (the exact digital transfer functions).
inline std::array<std::complex<double>, numBands> bandResponses (const std::array<CrossoverSection, numCrossovers>& x,
                                                                 double f, double fs) noexcept
{
    const auto lp1 = x[0].lowpass (f, fs), lp2 = x[1].lowpass (f, fs), lp3 = x[2].lowpass (f, fs);
    const auto ap1 = x[0].allpass (f, fs), ap2 = x[1].allpass (f, fs), ap3 = x[2].allpass (f, fs);
    const auto hp1 = ap1 - lp1, hp2 = ap2 - lp2, hp3 = ap3 - lp3;
    return { lp1 * ap2 * ap3, hp1 * lp2 * ap3, hp1 * hp2 * lp3, hp1 * hp2 * hp3 };
}

// Trapezoidal-integrator state-variable filter (topology-preserving bilinear transform; A. Simper, "Linear
// trapezoidal integrated SVF", Cytomic technical paper, 2013). With g = w0 tan(pi f_k/fs) and k = 1/Q it realises
// exactly the bilinear-transformed (pre-warped at f_k) w0^2/D and (s^2 - s w0/Q + w0^2)/D of a crossover section,
// like the direct-form coefficients above, but its states stay well-behaved while the coefficients glide (crossover
// / Q automation), so a coefficient change cannot produce a large transient.
struct Svf
{
    double a1 = 1.0, a2 = 0.0, a3 = 0.0, k = 1.0;
    double ic1 = 0.0, ic2 = 0.0;

    void set (double g, double kk) noexcept
    {
        k = kk;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() noexcept { ic1 = ic2 = 0.0; }

    // lowpass w0^2/D and allpass (D - 2 k s w0)/D of the same input
    void process (double v0, double& lp, double& ap) noexcept
    {
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        lp = v2;
        ap = v0 - 2.0 * k * v1;
    }
    double lowpass (double v0) noexcept { double lp, ap; process (v0, lp, ap); return lp; }
    double allpass (double v0) noexcept { double lp, ap; process (v0, lp, ap); return ap; }
};

// One split: x -> (LP_k x, HP_k x). Three biquads: two lowpass sections in series and one allpass.
class Split
{
public:
    void setCoeffs (const CrossoverSection& s) noexcept
    {
        first.set (s.g, s.k);
        second.set (s.g, s.k);
    }
    void reset() noexcept
    {
        first.reset();
        second.reset();
    }
    // The first section gives w0^2/D x and AP_k x; the second turns the former into LP_k x.
    void process (double x, double& low, double& high) noexcept
    {
        double lp1, ap;
        first.process (x, lp1, ap);
        low = second.lowpass (lp1);
        high = ap - low;
    }

private:
    Svf first, second;
};

// Four-band splitter for one channel (9 SVF sections; each split shares one section for its lowpass and allpass):
//   split1(x) -> L1, H1;  split2(H1) -> HP1 LP2, HP1 HP2;  split3(HP1 HP2) -> B3, B4;
//   B2 = AP3(HP1 LP2);  B1 = AP3(AP2(L1)).
class FourBandSplitter
{
public:
    void setSections (const std::array<CrossoverSection, numCrossovers>& s) noexcept
    {
        for (int k = 0; k < numCrossovers; ++k)
            split[size_t (k)].setCoeffs (s[size_t (k)]);
        ap2OnB1.set (s[1].g, s[1].k);
        ap3OnB1.set (s[2].g, s[2].k);
        ap3OnB2.set (s[2].g, s[2].k);
    }

    void reset() noexcept
    {
        for (auto& sp : split)
            sp.reset();
        ap2OnB1.reset();
        ap3OnB1.reset();
        ap3OnB2.reset();
    }

    void process (double x, std::array<double, numBands>& bands) noexcept
    {
        double l1, h1, l2, h2, l3, h3;
        split[0].process (x, l1, h1);
        split[1].process (h1, l2, h2);
        split[2].process (h2, l3, h3);
        bands[0] = ap3OnB1.allpass (ap2OnB1.allpass (l1));
        bands[1] = ap3OnB2.allpass (l2);
        bands[2] = l3;
        bands[3] = h3;
    }

private:
    std::array<Split, numCrossovers> split;
    Svf ap2OnB1, ap3OnB1, ap3OnB2;
};

} // namespace craft::multiband
