// Craft Audio doubler - voice EQ shelves (spec r1 section 8) and the feedback high-pass (section 9).
// RBJ cookbook shelves (public: R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients").
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>

#include "Parameters.h"

namespace craft::doubler
{

struct BiquadCoeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; // normalised by a0

    std::complex<double> response (double f, double fs) const
    {
        const auto z1 = std::polar (1.0, -2.0 * pi * f / fs);
        const auto z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

enum class ShelfType { Low, High };

// Analog shelf corner of spec section 8 (before the bilinear pre-mapping).
inline double shelfAnalogFrequency (ShelfType t, double fSet, double gainDb) noexcept
{
    return t == ShelfType::Low ? fSet * std::pow (10.0, gainDb / 80.0) / 1.5414
                               : fSet * 1.5694 * std::pow (10.0, -gainDb / 80.0);
}

inline BiquadCoeffs designShelf (ShelfType t, double fSet, double gainDb, double fs)
{
    const double fa = shelfAnalogFrequency (t, fSet, gainDb);
    const double f0 = (fs / pi) * std::atan (pi * fa / fs); // digital design frequency
    const double w0 = 2.0 * pi * f0 / fs;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double Q = 0.5;
    const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * Q), sa = 2.0 * std::sqrt (A) * alpha;

    double b0, b1, b2, a0, a1, a2;
    if (t == ShelfType::Low)
    {
        b0 = A * ((A + 1) - (A - 1) * cw + sa);
        b1 = 2 * A * ((A - 1) - (A + 1) * cw);
        b2 = A * ((A + 1) - (A - 1) * cw - sa);
        a0 = (A + 1) + (A - 1) * cw + sa;
        a1 = -2 * ((A - 1) + (A + 1) * cw);
        a2 = (A + 1) + (A - 1) * cw - sa;
    }
    else
    {
        b0 = A * ((A + 1) + (A - 1) * cw + sa);
        b1 = -2 * A * ((A - 1) + (A + 1) * cw);
        b2 = A * ((A + 1) + (A - 1) * cw - sa);
        a0 = (A + 1) - (A - 1) * cw + sa;
        a1 = 2 * ((A - 1) - (A + 1) * cw);
        a2 = (A + 1) - (A - 1) * cw - sa;
    }
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

// Transposed direct form II biquad.
struct Biquad
{
    BiquadCoeffs c;
    double s1 = 0, s2 = 0;

    void reset() noexcept { s1 = s2 = 0; }
    double process (double x) noexcept
    {
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

// First-order high-pass of spec section 9: H(z) = (1 - z^-1) / ((1 + k) + (k - 1) z^-1), k = tan(pi F / fs).
struct FeedbackHighPass
{
    double k = 0.0, x1 = 0.0, y1 = 0.0;

    static double coefficient (double fHz, double fs) noexcept
    {
        const double f = std::min (fHz, 0.49 * fs); // above Nyquist the bilinear map is undefined
        return std::tan (pi * f / fs);
    }
    void setFrequency (double fHz, double fs) noexcept { k = coefficient (fHz, fs); }
    void reset() noexcept { x1 = y1 = 0.0; }
    double process (double x) noexcept
    {
        const double y = (x - x1 - (k - 1.0) * y1) / (1.0 + k);
        x1 = x;
        y1 = y;
        return y;
    }
    static std::complex<double> response (double fHz, double f, double fs)
    {
        const double k = coefficient (fHz, fs);
        const auto z1 = std::polar (1.0, -2.0 * pi * f / fs);
        return (1.0 - z1) / ((1.0 + k) + (k - 1.0) * z1);
    }
};

} // namespace craft::doubler
