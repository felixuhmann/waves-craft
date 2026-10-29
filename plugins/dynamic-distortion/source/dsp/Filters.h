// Craft Dynamic Distortion - filters of the distortion path (spec r1 sections 4.5, 4.6).
//
// Every filter is an analog prototype (normalised to its corner, s = j f / f0) mapped to the processing rate
// with the bilinear transform pre-warped at f0 (RBJ audio-EQ cookbook). The distortion path runs oversampled
// (>= 176.4 kHz), so the digital responses follow the analog prototypes closely up to 20 kHz at every host rate.
// The prototype constants were fitted to the spec's normative response tables (tools/fit_filters.py).
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>

namespace craft::dd
{

constexpr double kPi = 3.14159265358979323846;

// Analog prototype b(s)/a(s) of order <= 2 in the normalised variable s = j f / f0.
struct Analog2
{
    double b2 = 0, b1 = 0, b0 = 1, a2 = 0, a1 = 0, a0 = 1;

    Analog2 inverse() const noexcept { return { a2, a1, a0, b2, b1, b0 }; }
    Analog2 scaled (double g) const noexcept { return { b2 * g, b1 * g, b0 * g, a2, a1, a0 }; }

    std::complex<double> at (double fNorm) const noexcept
    {
        const std::complex<double> s (0.0, fNorm);
        return (b2 * s * s + b1 * s + b0) / (a2 * s * s + a1 * s + a0);
    }
};

// Transposed direct form II biquad (double precision state).
class Biquad
{
public:
    // Bilinear transform of `proto` with its corner f0 (Hz) at sample rate fs.
    void design (const Analog2& p, double f0, double fs) noexcept
    {
        f0 = std::clamp (f0, 1.0e-3, 0.45 * fs);
        const double k = 1.0 / std::tan (kPi * f0 / fs), k2 = k * k;
        const double n0 = p.b2 * k2 + p.b1 * k + p.b0, n1 = 2.0 * (p.b0 - p.b2 * k2), n2 = p.b2 * k2 - p.b1 * k + p.b0;
        const double d0 = p.a2 * k2 + p.a1 * k + p.a0, d1 = 2.0 * (p.a0 - p.a2 * k2), d2 = p.a2 * k2 - p.a1 * k + p.a0;
        b0 = n0 / d0;
        b1 = n1 / d0;
        b2 = n2 / d0;
        a1 = d1 / d0;
        a2 = d2 / d0;
    }

    // First-order low-pass with the impulse-invariant pole and a zero chosen so that the magnitude equals the
    // analog 1 / (1 + s) at DC and at Nyquist ("matched" design; closer to the analog response near Nyquist
    // than the bilinear transform: < 0.3 dB error up to 20 kHz at 176.4 kHz).
    void designMatchedLowpass1 (double fc, double fs) noexcept
    {
        fc = std::clamp (fc, 1.0e-3, 0.45 * fs);
        const double p = std::exp (-2.0 * kPi * fc / fs);
        const double ny = 0.5 * fs / fc, hNyquist = 1.0 / std::sqrt (1.0 + ny * ny);
        const double sum = 1.0 - p, diff = hNyquist * (1.0 + p);
        b0 = 0.5 * (sum + diff);
        b1 = 0.5 * (sum - diff);
        b2 = 0.0;
        a1 = -p;
        a2 = 0.0;
    }

    void setIdentity() noexcept
    {
        b0 = 1.0;
        b1 = b2 = a1 = a2 = 0.0;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    // Takes over the coefficients of another section, keeping this section's state.
    void copyCoefficients (const Biquad& o) noexcept
    {
        b0 = o.b0;
        b1 = o.b1;
        b2 = o.b2;
        a1 = o.a1;
        a2 = o.a2;
    }

    double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    // Complex response at f (Hz) for sample rate fs.
    std::complex<double> response (double f, double fs) const noexcept
    {
        const auto z1c = std::polar (1.0, -2.0 * kPi * f / fs), z2c = z1c * z1c;
        return (b0 + b1 * z1c + b2 * z2c) / (1.0 + a1 * z1c + a2 * z2c);
    }

private:
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;
};

// ---------------------------------------------------------------------------------------------
// Tone EQ prototypes (section 4.5). A negative value v is a cut of depth 18 |v| / 50 dB; a positive value is
// the exact inverse of the cut at |v| times a broadband gain of -6 v / 50 dB (Bass, Treble) or -9 v / 50 dB (Mid).
namespace eq
{
    inline constexpr double maxCutDb = 18.0;

    // Bass: RBJ low shelf, mid-point (half depth in dB) at 100 Hz, Q 0.4 (fit: max error 0.01 dB).
    inline constexpr double bassF0 = 100.0, bassQ = 0.4;
    // Treble: bell with fixed zeros (numerator Q) and depth-dependent poles, centre 7.947 kHz, Qn 0.3239
    // (fit to treble -50 and the mirrored treble +25 jointly: max error 0.20 dB).
    inline constexpr double trebleF0 = 7947.0, trebleQn = 0.3239;
    // Mid: same bell family; centre ratio and Qn per Mid Freq anchor (fits: max error 0.01 / 0.15 / 0.33 dB),
    // interpolated linearly in log2(frequency) between the anchors.
    struct MidAnchor
    {
        double freq, centreRatio, qn;
    };
    inline constexpr MidAnchor midAnchors[] = { { 150.0, 0.99977, 1.9995 }, { 1252.0, 0.99397, 1.1840 },
                                                { 5000.0, 0.96627, 1.0198 } };

    inline double cutDepthDb (double value) noexcept { return maxCutDb * std::min (std::abs (value), 50.0) / 50.0; }

    // RBJ low shelf with gain G dB (normalised to its mid-point frequency).
    inline Analog2 lowShelf (double gainDb, double q) noexcept
    {
        const double a = std::pow (10.0, gainDb / 40.0), sa = std::sqrt (a);
        return { a, a * sa / q, a * a, a, sa / q, 1.0 };
    }

    // Bell with fixed zeros: (s^2 + s/Qn + 1) / (s^2 + s/(g Qn) + 1), g = centre gain (< 1 for a cut).
    inline Analog2 bellCut (double depthDb, double qn) noexcept
    {
        const double g = std::pow (10.0, -depthDb / 20.0);
        return { 1.0, 1.0 / qn, 1.0, 1.0, 1.0 / (g * qn), 1.0 };
    }

    // Cut for v <= 0, mirrored cut times the broadband gain for v > 0.
    inline Analog2 mirrored (const Analog2& cut, double value, double broadbandDbAt50) noexcept
    {
        if (value <= 0.0)
            return cut;
        return cut.inverse().scaled (std::pow (10.0, broadbandDbAt50 * std::min (value, 50.0) / 50.0 / 20.0));
    }

    inline Analog2 bass (double v) noexcept { return mirrored (lowShelf (-cutDepthDb (v), bassQ), v, -6.0); }
    inline Analog2 treble (double v) noexcept { return mirrored (bellCut (cutDepthDb (v), trebleQn), v, -6.0); }

    struct MidShape
    {
        double centreHz, qn;
    };
    inline MidShape midShape (double midFreq) noexcept
    {
        const auto& a = midAnchors;
        const double l = std::log2 (std::clamp (midFreq, a[0].freq, a[2].freq));
        const int i = l <= std::log2 (a[1].freq) ? 0 : 1;
        const double l0 = std::log2 (a[i].freq), l1 = std::log2 (a[i + 1].freq);
        const double t = std::clamp ((l - l0) / (l1 - l0), 0.0, 1.0);
        const double ratio = a[i].centreRatio + t * (a[i + 1].centreRatio - a[i].centreRatio);
        const double qn = a[i].qn + t * (a[i + 1].qn - a[i].qn);
        return { midFreq * ratio, qn };
    }
    inline Analog2 mid (double v, double qn) noexcept { return mirrored (bellCut (cutDepthDb (v), qn), v, -9.0); }
} // namespace eq

// ---------------------------------------------------------------------------------------------
// Post-clipper filters (section 4.6).
namespace post
{
    // HP (distortion path only): second-order section 24.437 Hz / Q 0.92207 times a first-order 25.357 Hz
    // section (fit to the section 4.6 table: max error 0.01 dB).
    inline constexpr double hp2F0 = 24.437, hp2Q = 0.92207, hp1F0 = 25.357;
    inline Analog2 hp2() noexcept { return { 1.0, 0.0, 0.0, 1.0, 1.0 / hp2Q, 1.0 }; }
    inline Analog2 hp1() noexcept { return { 0.0, 1.0, 0.0, 0.0, 1.0, 1.0 }; }
    // LP: two identical first-order sections 1 / (1 + s), -3.01 dB each (-6.02 dB together) at the set
    // frequency (Biquad::designMatchedLowpass1).

    // Analog magnitude helpers (display and gain bookkeeping).
    inline double hpMagnitude (double f) noexcept
    {
        return std::abs (hp2().at (f / hp2F0) * hp1().at (f / hp1F0));
    }
    inline double lpMagnitude (double f, double fc) noexcept { return 1.0 / (1.0 + (f / fc) * (f / fc)); }
} // namespace post

// ---------------------------------------------------------------------------------------------
// Clip emphasis pair: a first-order low-shelf boost before the clipper and its exact inverse after it. The pair
// is transparent for small signals; for clipped signals it sets the frequency dependence of the clip ceiling
// (spec section 6.1: fully clipped fundamentals -10.9 dBFS at 50 Hz .. -8.0 dBFS at 2 kHz) and makes long
// clipped half-cycles settle at the low-frequency clip level c of section 4.4.
namespace emphasis
{
    inline constexpr double zeroHz = 93.37, poleHz = 134.48; // post filter (s + wz) / (s + wp)
    // post, normalised to the pole: (s + z/p) / (s + 1)
    inline Analog2 postFilter() noexcept { return { 0.0, 1.0, zeroHz / poleHz, 0.0, 1.0, 1.0 }; }
    inline Analog2 preFilter() noexcept { return postFilter().inverse(); }
} // namespace emphasis

} // namespace craft::dd
