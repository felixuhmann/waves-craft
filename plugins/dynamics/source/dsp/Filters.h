// Craft Dynamics - filter module (spec r1 section 7). Host-independent.
//
// H(s) is a textbook second-order section (Q = the Q control) discretised by the bilinear transform prewarped at
// the Freq control. From it:
//   sidechain (detection) filter  = H^3            (three cascaded sections, |H|^3)
//   active band                   A = H^2           (two cascaded sections)
//   allpass                       AP = +-D(-s)/D(s) (sign chosen so that AP is in phase with A)
//   passive band                  P = AP - A        (|P| = 1 - |A|, A + P = AP exactly)
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>

namespace craft::dynamics::dsp
{

enum class FilterType
{
    LowPass = 0,
    BandPass = 1,
    HighPass = 2,
    BandReject = 3
};

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    std::complex<double> response (double omega) const noexcept // omega in rad/sample
    {
        const std::complex<double> z1 = std::polar (1.0, -omega), z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

// Transposed direct form II, double precision.
struct Biquad
{
    double s1 = 0.0, s2 = 0.0;

    double process (const BiquadCoeffs& c, double x) noexcept
    {
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void reset() noexcept { s1 = s2 = 0.0; }
};

struct FilterDesign
{
    BiquadCoeffs h;  // H(z)
    BiquadCoeffs ap; // allpass in phase with H^2
};

namespace filterdesign
{
    inline constexpr double pi = 3.14159265358979323846;

    // u = w/w0 at which |1 / ((s/w0)^2 + s/(Q w0) + 1)|^2 = 1/2:  u^4 + (1/Q^2 - 2) u^2 - 1 = 0
    inline double halfPowerRatio (double q) noexcept
    {
        const double b = 1.0 / (q * q) - 2.0;
        return std::sqrt ((-b + std::sqrt (b * b + 4.0)) * 0.5);
    }

    // Analog section N(s)/D(s) with polynomials in s: n2 s^2 + n1 s + n0 over d2 s^2 + d1 s + d0.
    struct Analog
    {
        double n2, n1, n0, d2, d1, d0;
    };

    inline Analog analog (FilterType type, double freqHz, double q) noexcept
    {
        const double w = 2.0 * pi * freqHz;
        double w0 = w;
        if (type == FilterType::LowPass)
            w0 = w / halfPowerRatio (q);
        else if (type == FilterType::HighPass)
            w0 = w * halfPowerRatio (q);
        const double d2 = 1.0 / (w0 * w0), d1 = 1.0 / (q * w0), d0 = 1.0;
        switch (type)
        {
            case FilterType::LowPass:    return { 0.0, 0.0, 1.0, d2, d1, d0 };
            case FilterType::HighPass:   return { d2, 0.0, 0.0, d2, d1, d0 };
            case FilterType::BandPass:   return { 0.0, d1, 0.0, d2, d1, d0 };
            case FilterType::BandReject: return { d2, 0.0, 1.0, d2, d1, d0 };
        }
        return { 0.0, 0.0, 1.0, d2, d1, d0 };
    }

    // Bilinear transform s = K (1 - z^-1) / (1 + z^-1).
    inline BiquadCoeffs bilinear (double n2, double n1, double n0, double d2, double d1, double d0, double k) noexcept
    {
        const double k2 = k * k;
        const double a0 = d2 * k2 + d1 * k + d0;
        BiquadCoeffs c;
        c.b0 = (n2 * k2 + n1 * k + n0) / a0;
        c.b1 = 2.0 * (n0 - n2 * k2) / a0;
        c.b2 = (n2 * k2 - n1 * k + n0) / a0;
        c.a1 = 2.0 * (d0 - d2 * k2) / a0;
        c.a2 = (d2 * k2 - d1 * k + d0) / a0;
        return c;
    }

    inline FilterDesign design (FilterType type, double freqHz, double q, double sampleRate) noexcept
    {
        // keep the prewarp frequency below Nyquist
        const double f = std::min (freqHz, 0.495 * sampleRate);
        const auto a = analog (type, f, q);
        const double w = 2.0 * pi * f;
        const double k = w / std::tan (w / (2.0 * sampleRate));
        FilterDesign d;
        d.h = bilinear (a.n2, a.n1, a.n0, a.d2, a.d1, a.d0, k);
        // AP = D(-s)/D(s) (LP, HP, BR) or -D(-s)/D(s) (BP): in phase with H^2 (spec 7)
        const double sgn = type == FilterType::BandPass ? -1.0 : 1.0;
        d.ap = bilinear (sgn * a.d2, -sgn * a.d1, sgn * a.d0, a.d2, a.d1, a.d0, k);
        return d;
    }
} // namespace filterdesign

// Per-channel filter states for the three paths.
struct FilterChannel
{
    Biquad sc[3]; // sidechain H^3
    Biquad act[2]; // active H^2
    Biquad ap;

    double sidechain (const FilterDesign& d, double x) noexcept
    {
        return sc[2].process (d.h, sc[1].process (d.h, sc[0].process (d.h, x)));
    }
    // returns A(x); writes AP(x)
    double bands (const FilterDesign& d, double x, double& allpassOut) noexcept
    {
        allpassOut = ap.process (d.ap, x);
        return act[1].process (d.h, act[0].process (d.h, x));
    }
    void reset() noexcept
    {
        for (auto& b : sc) b.reset();
        for (auto& b : act) b.reset();
        ap.reset();
    }
};

} // namespace craft::dynamics::dsp
