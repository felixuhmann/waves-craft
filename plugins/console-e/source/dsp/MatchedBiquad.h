// Craft Console E - analogue-matched second-order sections (no bilinear frequency warping).
//
// Poles are mapped exactly (impulse invariance, z = exp(sT)); the numerator is chosen so that the digital
// magnitude equals the analogue prototype's magnitude at DC, at Nyquist and at one in-band frequency.
// This is the magnitude-matching scheme published by M. Vicanek, "Matched Second Order Digital Filters"
// (2016), generalised here to an arbitrary second-order analogue numerator.
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::consolee::dsp
{

inline constexpr double pi = 3.14159265358979323846;

// Analogue second-order section H(s) = (b2 s^2 + b1 s + b0) / (s^2 + a1 s + a0), s in rad/s.
struct AnalogSection
{
    double b2 = 1, b1 = 0, b0 = 1;
    double a1 = 0, a0 = 1;

    // |H(j w)|^2
    double magnitudeSquared (double w) const noexcept
    {
        const double nr = b0 - b2 * w * w, ni = b1 * w;
        const double dr = a0 - w * w, di = a1 * w;
        return (nr * nr + ni * ni) / (dr * dr + di * di);
    }
};

// Digital biquad coefficients, a0 normalised to 1: H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2).
struct BiquadCoeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    static BiquadCoeffs identity() noexcept { return {}; }

    // magnitude in dB at frequency f (Hz) for sample rate fs
    double magnitudeDb (double f, double fs) const noexcept
    {
        const double w = 2.0 * pi * f / fs;
        const double c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return 10.0 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di));
    }

    // 1/H (used for the reciprocal realisation of cuts); requires a minimum-phase numerator
    BiquadCoeffs inverted() const noexcept
    {
        BiquadCoeffs r;
        r.b0 = 1.0 / b0;
        r.b1 = a1 / b0;
        r.b2 = a2 / b0;
        r.a1 = b1 / b0;
        r.a2 = b2 / b0;
        return r;
    }
};

// Poles of s^2 + a1 s + a0 mapped by z = exp(sT); returns digital a1, a2.
inline void matchedPoles (double a1s, double a0s, double T, double& a1, double& a2) noexcept
{
    const double w0 = std::sqrt (std::max (a0s, 1e-30));
    const double zeta = a1s / (2.0 * w0);
    const double decay = std::exp (-zeta * w0 * T);
    if (zeta < 1.0)
    {
        const double wd = std::min (w0 * std::sqrt (1.0 - zeta * zeta) * T, pi);
        a1 = -2.0 * decay * std::cos (wd);
    }
    else
    {
        const double wd = w0 * std::sqrt (zeta * zeta - 1.0) * T;
        a1 = -2.0 * decay * std::cosh (wd);
    }
    a2 = decay * decay;
}

// Second-order analogue -> digital, magnitude matched at DC, Nyquist and wm (rad/s; <= 0 selects
// min(w0, fs * pi / 2)).
inline BiquadCoeffs designMatched (const AnalogSection& s, double fs, double wm = -1.0) noexcept
{
    const double T = 1.0 / fs;
    BiquadCoeffs c;
    matchedPoles (s.a1, s.a0, T, c.a1, c.a2);

    const double A0 = (1.0 + c.a1 + c.a2) * (1.0 + c.a1 + c.a2);
    const double A1 = (1.0 - c.a1 + c.a2) * (1.0 - c.a1 + c.a2);
    const double A2 = -4.0 * c.a2;

    const double wNyq = pi * fs;
    const double w0 = std::sqrt (std::max (s.a0, 1e-30));
    if (wm <= 0.0)
        wm = std::min (w0, 0.5 * wNyq);

    const double H0 = s.b0 == 0.0 ? 0.0 : (s.b0 * s.b0) / (s.a0 * s.a0);
    const double HN = s.magnitudeSquared (wNyq);
    const double Hm = s.magnitudeSquared (wm);

    const double sm = std::sin (0.5 * wm * T);
    const double phi1 = sm * sm, phi0 = 1.0 - phi1, phi2 = 4.0 * phi0 * phi1;

    const double B0 = H0 * A0;
    const double B1 = HN * A1;
    const double Am = A0 * phi0 + A1 * phi1 + A2 * phi2;
    const double B2 = (Hm * Am - B0 * phi0 - B1 * phi1) / phi2;

    const double sB0 = std::sqrt (std::max (B0, 0.0)), sB1 = std::sqrt (std::max (B1, 0.0));
    const double W = 0.5 * (sB0 + sB1);
    c.b0 = 0.5 * (W + std::sqrt (std::max (W * W + B2, 0.0)));
    c.b1 = 0.5 * (sB0 - sB1);
    c.b2 = c.b0 != 0.0 ? -B2 / (4.0 * c.b0) : 0.0;
    return c;
}

// First-order analogue (b1 s + b0) / (s + a0) -> digital, magnitude matched at DC and Nyquist.
inline BiquadCoeffs designMatchedFirstOrder (double b1s, double b0s, double a0s, double fs) noexcept
{
    BiquadCoeffs c;
    c.a1 = -std::exp (-a0s / fs);
    c.a2 = 0.0;
    const double A0 = (1.0 + c.a1) * (1.0 + c.a1);
    const double A1 = (1.0 - c.a1) * (1.0 - c.a1);
    const double wN = pi * fs;
    const double H0 = (b0s / a0s) * (b0s / a0s);
    const double HN = (b0s * b0s + b1s * b1s * wN * wN) / (a0s * a0s + wN * wN);
    const double sB0 = std::sqrt (H0 * A0), sB1 = std::sqrt (HN * A1);
    c.b0 = 0.5 * (sB0 + sB1);
    c.b1 = 0.5 * (sB0 - sB1);
    c.b2 = 0.0;
    return c;
}

// Transposed direct form II biquad, double precision state.
struct Biquad
{
    BiquadCoeffs c;
    double z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }

    double process (double x) noexcept
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

} // namespace craft::consolee::dsp
