// Craft Opto Leveler - small IIR building blocks (bilinear transform with pre-warping). Host independent.
#pragma once

#include <cmath>

namespace craft::opto_leveler::dsp
{

inline constexpr double pi = 3.14159265358979323846;

// First-order section y = (b0 + b1 z^-1) / (1 + a1 z^-1) x, transposed direct form II.
struct FirstOrder
{
    double b0 = 1.0, b1 = 0.0, a1 = 0.0;
    double s = 0.0;

    void reset() noexcept { s = 0.0; }
    double process (double x) noexcept
    {
        const double y = b0 * x + s;
        s = b1 * x - a1 * y;
        return y;
    }

    // 1st-order high-pass at fc (Hz)
    static FirstOrder highPass (double fc, double fs)
    {
        const double k = std::tan (pi * fc / fs);
        FirstOrder f;
        f.b0 = 1.0 / (1.0 + k);
        f.b1 = -f.b0;
        f.a1 = (k - 1.0) / (k + 1.0);
        return f;
    }

    // 1st-order high shelf H(s) = (G s + w) / (s + w): 0 dB below, gainDb above the corner fc.
    static FirstOrder highShelf (double fc, double gainDb, double fs)
    {
        const double g = std::pow (10.0, gainDb / 20.0);
        const double k = std::tan (pi * fc / fs);
        FirstOrder f;
        f.b0 = (g + k) / (1.0 + k);
        f.b1 = (k - g) / (1.0 + k);
        f.a1 = (k - 1.0) / (k + 1.0);
        return f;
    }

    double magnitude (double f, double fs) const
    {
        const double w = 2.0 * pi * f / fs;
        const double nr = b0 + b1 * std::cos (w), ni = -b1 * std::sin (w);
        const double dr = 1.0 + a1 * std::cos (w), di = -a1 * std::sin (w);
        return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
    }
};

// Biquad, transposed direct form II.
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double s1 = 0.0, s2 = 0.0;

    void reset() noexcept { s1 = s2 = 0.0; }
    double process (double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
    void setCoefficients (const Biquad& c) noexcept
    {
        b0 = c.b0;
        b1 = c.b1;
        b2 = c.b2;
        a1 = c.a1;
        a2 = c.a2;
    }

    // RBJ peaking equaliser (bandwidth by Q, pre-warped centre).
    static Biquad peaking (double f0, double q, double gainDb, double fs)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * f0 / fs;
        const double al = std::sin (w0) / (2.0 * q);
        const double c = std::cos (w0);
        const double a0 = 1.0 + al / A;
        Biquad b;
        b.b0 = (1.0 + al * A) / a0;
        b.b1 = -2.0 * c / a0;
        b.b2 = (1.0 - al * A) / a0;
        b.a1 = -2.0 * c / a0;
        b.a2 = (1.0 - al / A) / a0;
        return b;
    }

    // RBJ shelves (shelf slope S = 1).
    static Biquad shelf (bool high, double f0, double gainDb, double fs)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * f0 / fs;
        const double c = std::cos (w0), sn = std::sin (w0);
        const double al = sn / 2.0 * std::sqrt (2.0);
        const double sq = 2.0 * std::sqrt (A) * al;
        double b0, b1, b2, a0, a1, a2;
        if (high)
        {
            b0 = A * ((A + 1) + (A - 1) * c + sq);
            b1 = -2 * A * ((A - 1) + (A + 1) * c);
            b2 = A * ((A + 1) + (A - 1) * c - sq);
            a0 = (A + 1) - (A - 1) * c + sq;
            a1 = 2 * ((A - 1) - (A + 1) * c);
            a2 = (A + 1) - (A - 1) * c - sq;
        }
        else
        {
            b0 = A * ((A + 1) - (A - 1) * c + sq);
            b1 = 2 * A * ((A - 1) - (A + 1) * c);
            b2 = A * ((A + 1) - (A - 1) * c - sq);
            a0 = (A + 1) + (A - 1) * c + sq;
            a1 = -2 * ((A - 1) + (A + 1) * c);
            a2 = (A + 1) + (A - 1) * c - sq;
        }
        Biquad b;
        b.b0 = b0 / a0;
        b.b1 = b1 / a0;
        b.b2 = b2 / a0;
        b.a1 = a1 / a0;
        b.a2 = a2 / a0;
        return b;
    }

    double magnitude (double f, double fs) const
    {
        const double w = 2.0 * pi * f / fs;
        const double c1 = std::cos (w), s1_ = std::sin (w), c2 = std::cos (2 * w), s2_ = std::sin (2 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1_ + b2 * s2_);
        const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1_ + a2 * s2_);
        return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
    }
};

} // namespace craft::opto_leveler::dsp
