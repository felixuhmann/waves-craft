// Craft Enhancer - five-band phase-aligned crossover (spec r1 section 3).
//
// Each split k (crossover fc_k) is a pair of complementary filters built from one second-order low-pass prototype
// H_k(s) = 1 / (1 + d1 s + d2 s^2) (s normalised to the crossover frequency):
//     low  = H_k(s)^2                      = |H_k|^2 * A_k
//     high = A_k(s) - H_k(s)^2             = (1 - |H_k|^2) * A_k,      A_k(s) = D_k(-s) / D_k(s) (all-pass)
// so low + high = A_k exactly and both halves carry the same phase (zero-phase magnitudes |H_k|^2 and 1 - |H_k|^2).
// Topology (found by fitting the normative tables, see tools/fit/): the input is split at crossover 2 first; the low
// side is split at crossover 1 (bands A, B), the high side at crossover 3 (band C) and its high side at crossover 4
// (bands D, E). All-pass compensation gives every band the common phase A_1 A_2 A_3 A_4, so the five bands add up to
// that all-pass exactly (spec 3, complementarity) and the direct path has unit magnitude.
// The four prototypes (d1, d2 per split) are fitted to tables/xover_bands_*.csv and tables/direct_group_delay.csv
// (tools/fit/xover_fit.py). Digital filters: bilinear transform pre-warped at fc, designed at the true sample rate.
#pragma once

#include <array>
#include <cmath>

#include "Parameters.h"

namespace craft::enhancer::dsp
{

inline constexpr double pi = 3.14159265358979323846;

// Normalised prototype per split: |H(jx)|^2 = 1 / (1 + a x^2 + b x^4); d2 = sqrt(b), d1 = sqrt(a + 2 d2).
struct SplitPrototype
{
    double a, b;
    double d2() const noexcept { return std::sqrt (b); }
    double d1() const noexcept { return std::sqrt (a + 2.0 * std::sqrt (b)); }
};

inline constexpr SplitPrototype splitPrototypes[numCrossovers] = {
    { 0.5518515680949855, 0.6657686256985158 },
    { 0.7402901470506228, 0.45274847401540463 },
    { 0.7580166879475768, 0.43911436045998375 },
    { 0.49651293785410994, 0.8379263311905708 },
};

// Highest usable crossover frequency relative to the sample rate (keeps the bilinear design well conditioned).
inline constexpr double maxCrossoverRatio = 0.45;

struct BiquadCoeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

// Transposed direct form II biquad (double precision state).
struct Biquad
{
    double z1 = 0, z2 = 0;
    inline double process (double x, const BiquadCoeffs& c) noexcept
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

struct SplitCoeffs
{
    BiquadCoeffs lp; // H_k (applied twice)
    BiquadCoeffs ap; // A_k
};

inline SplitCoeffs designSplit (int k, double fc, double sampleRate) noexcept
{
    const auto& p = splitPrototypes[k];
    const double d1 = p.d1(), d2 = p.d2();
    fc = std::clamp (fc, 1.0, maxCrossoverRatio * sampleRate);
    const double K = std::tan (pi * fc / sampleRate);
    const double K2 = K * K;
    const double a0 = d2 + d1 * K + K2;
    const double a1 = (2.0 * K2 - 2.0 * d2) / a0;
    const double a2 = (d2 - d1 * K + K2) / a0;
    SplitCoeffs c;
    c.lp = { K2 / a0, 2.0 * K2 / a0, K2 / a0, a1, a2 };
    c.ap = { a2, a1, 1.0, a1, a2 };
    return c;
}

// Analytic magnitude/phase of the split pieces at frequency f (digital, for tests and the display).
struct Complex
{
    double re = 0, im = 0;
    Complex operator* (const Complex& o) const noexcept { return { re * o.re - im * o.im, re * o.im + im * o.re }; }
    Complex operator+ (const Complex& o) const noexcept { return { re + o.re, im + o.im }; }
    Complex operator- (const Complex& o) const noexcept { return { re - o.re, im - o.im }; }
    double abs() const noexcept { return std::hypot (re, im); }
    double arg() const noexcept { return std::atan2 (im, re); }
};

inline Complex biquadResponse (const BiquadCoeffs& c, double f, double sampleRate) noexcept
{
    const double w = 2.0 * pi * f / sampleRate;
    const Complex z1 { std::cos (w), -std::sin (w) }, z2 { std::cos (2 * w), -std::sin (2 * w) };
    const Complex num = Complex { c.b0, 0 } + Complex { c.b1, 0 } * z1 + Complex { c.b2, 0 } * z2;
    const Complex den = Complex { 1, 0 } + Complex { c.a1, 0 } * z1 + Complex { c.a2, 0 } * z2;
    const double d = den.re * den.re + den.im * den.im;
    return { (num.re * den.re + num.im * den.im) / d, (num.im * den.re - num.re * den.im) / d };
}

// Complex responses of the five bands for given crossovers (same structure as Crossover::process).
inline std::array<Complex, numBands> bandResponses (const std::array<double, numCrossovers>& fc, double f, double sampleRate) noexcept
{
    std::array<Complex, numCrossovers> L, A;
    for (int k = 0; k < numCrossovers; ++k)
    {
        const auto c = designSplit (k, fc[(size_t) k], sampleRate);
        const auto h = biquadResponse (c.lp, f, sampleRate);
        L[(size_t) k] = h * h;
        A[(size_t) k] = biquadResponse (c.ap, f, sampleRate);
    }
    const Complex lo2 = L[1], hi2 = A[1] - L[1];
    const Complex lowc = lo2 * A[2] * A[3];
    const Complex bandA = lowc * L[0];
    const Complex bandB = lowc * (A[0] - L[0]);
    const Complex highc = hi2 * A[0];
    const Complex c0 = highc * L[2];
    const Complex r = highc * (A[2] - L[2]);
    return { bandA, bandB, c0 * A[3], r * L[3], r * (A[3] - L[3]) };
}

// Per-channel five-band crossover.
class Crossover
{
public:
    void setCoefficients (const std::array<SplitCoeffs, numCrossovers>& c) noexcept { coeffs = c; }

    void reset() noexcept
    {
        for (auto* b : { &s2lp1, &s2lp2, &s2ap, &compLow3, &compLow4, &s1lp1, &s1lp2, &s1ap, &compHigh1, &s3lp1, &s3lp2,
                         &s3ap, &compC4, &s4lp1, &s4lp2, &s4ap })
            b->reset();
    }

    // Splits x into the five bands (out[0] = A ... out[4] = E).
    inline void process (double x, double* out) noexcept
    {
        const auto& c1 = coeffs[0];
        const auto& c2 = coeffs[1];
        const auto& c3 = coeffs[2];
        const auto& c4 = coeffs[3];
        // split 2
        const double lo = s2lp2.process (s2lp1.process (x, c2.lp), c2.lp);
        const double hi = s2ap.process (x, c2.ap) - lo;
        // low side: compensate A3 A4, split 1
        const double lc = compLow4.process (compLow3.process (lo, c3.ap), c4.ap);
        const double bA = s1lp2.process (s1lp1.process (lc, c1.lp), c1.lp);
        const double bB = s1ap.process (lc, c1.ap) - bA;
        // high side: compensate A1, split 3, split 4
        const double hc = compHigh1.process (hi, c1.ap);
        const double c0 = s3lp2.process (s3lp1.process (hc, c3.lp), c3.lp);
        const double r = s3ap.process (hc, c3.ap) - c0;
        const double bC = compC4.process (c0, c4.ap);
        const double bD = s4lp2.process (s4lp1.process (r, c4.lp), c4.lp);
        const double bE = s4ap.process (r, c4.ap) - bD;
        out[0] = bA;
        out[1] = bB;
        out[2] = bC;
        out[3] = bD;
        out[4] = bE;
    }

private:
    std::array<SplitCoeffs, numCrossovers> coeffs {};
    Biquad s2lp1, s2lp2, s2ap, compLow3, compLow4, s1lp1, s1lp2, s1ap, compHigh1, s3lp1, s3lp2, s3ap, compC4, s4lp1, s4lp2, s4ap;
};

} // namespace craft::enhancer::dsp
