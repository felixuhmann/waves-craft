// Craft De-Esser - sidechain filter (spec r2 section 4) and split band filter (section 6).
// Host-independent: no JUCE dependency. All arithmetic in double precision.
#pragma once

#include <cmath>

namespace craft::deesser::dsp
{

inline constexpr double pi = 3.14159265358979323846;

// ---------------------------------------------------------------------------------------------
// Biquad coefficients normalised by a0: H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients": HPF.
inline BiquadCoeffs cookbookHighPass (double f0, double q, double fs) noexcept
{
    const double w0 = 2.0 * pi * f0 / fs;
    const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    return { (1.0 + cw) / 2.0 / a0, -(1.0 + cw) / a0, (1.0 + cw) / 2.0 / a0, -2.0 * cw / a0, (1.0 - alpha) / a0 };
}

// Cookbook LPF.
inline BiquadCoeffs cookbookLowPass (double f0, double q, double fs) noexcept
{
    const double w0 = 2.0 * pi * f0 / fs;
    const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    return { (1.0 - cw) / 2.0 / a0, (1.0 - cw) / a0, (1.0 - cw) / 2.0 / a0, -2.0 * cw / a0, (1.0 - alpha) / a0 };
}

// Transposed direct form II biquad.
struct Biquad
{
    double s1 = 0.0, s2 = 0.0;

    void reset() noexcept { s1 = s2 = 0.0; }

    double process (const BiquadCoeffs& c, double x) noexcept
    {
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

// ---------------------------------------------------------------------------------------------
// Spec section 4: three identical cookbook high-pass sections at 0.84 Freq (Q = 1/sqrt 2), and for the
// band-pass mode one cookbook low-pass at 1.457107 Freq (clamped just below fs/2).
struct SidechainCoeffs
{
    static constexpr double hpFactor = 0.84;
    static constexpr double lpFactor = 1.457107;
    static constexpr double butterworthQ = 0.70710678118654752440;
    static constexpr double maxLowPassFraction = 0.4999; // of fs: "just below fs/2"

    BiquadCoeffs hp, lp;

    static SidechainCoeffs design (double freqHz, double fs) noexcept
    {
        SidechainCoeffs c;
        c.hp = cookbookHighPass (hpFactor * freqHz, butterworthQ, fs);
        c.lp = cookbookLowPass (std::fmin (lpFactor * freqHz, maxLowPassFraction * fs), butterworthQ, fs);
        return c;
    }
};

// Runs the high-pass cascade once and derives both the HighPass and the BandPass outputs, so a
// mode switch can crossfade between them without restarting a filter.
class SidechainFilter
{
public:
    void reset() noexcept
    {
        for (auto& s : hp)
            s.reset();
        lp.reset();
    }

    struct Output
    {
        double highPass, bandPass;
    };

    Output process (const SidechainCoeffs& c, double x) noexcept
    {
        double y = x;
        for (auto& s : hp)
            y = s.process (c.hp, y);
        return { y, lp.process (c.lp, y) };
    }

private:
    Biquad hp[3];
    Biquad lp;
};

// ---------------------------------------------------------------------------------------------
// Spec section 6: complementary first-order split, k = tan(pi Freq / fs) / sqrt 10,
//   HP1(z) = (1 - z^-1) / ((1 + k) + (k - 1) z^-1),  LP1 = 1 - HP1.
struct SplitCoeffs
{
    static constexpr double sqrt10 = 3.16227766016837933200;

    double k = 0.0;
    double b = 0.0;  // 1 / (1 + k)
    double a1 = 0.0; // (k - 1) / (1 + k)

    static double cornerK (double freqHz, double fs) noexcept
    {
        // Freq <= 16 kHz; keep the tangent finite at very low sample rates as well.
        const double w = std::fmin (pi * freqHz / fs, 0.4999 * pi);
        return std::tan (w) / sqrt10;
    }

    static SplitCoeffs design (double freqHz, double fs) noexcept
    {
        SplitCoeffs c;
        c.k = cornerK (freqHz, fs);
        c.b = 1.0 / (1.0 + c.k);
        c.a1 = (c.k - 1.0) / (1.0 + c.k);
        return c;
    }
};

// High band of the split (first-order high-pass, direct form I state).
class SplitHighBand
{
public:
    void reset() noexcept { x1 = y1 = 0.0; }

    double process (const SplitCoeffs& c, double x) noexcept
    {
        const double y = c.b * (x - x1) - c.a1 * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    double x1 = 0.0, y1 = 0.0;
};

} // namespace craft::deesser::dsp
