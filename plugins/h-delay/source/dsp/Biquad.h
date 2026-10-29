// Craft Hybrid Delay - biquad sections.
// Coefficient formulas: R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients".
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>

namespace craft::hdelay
{

inline constexpr double pi = 3.14159265358979323846;

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    std::complex<double> response (double f, double fs) const noexcept
    {
        const auto z1 = std::polar (1.0, -2.0 * pi * f / fs);
        const auto z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }

    static BiquadCoeffs normalised (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    // --- RBJ designs from the digital corner w0 (radians/sample) ---------------------------------
    static BiquadCoeffs lowPass (double w0, double q) noexcept
    {
        const double c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        return normalised ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    static BiquadCoeffs highPass (double w0, double q) noexcept
    {
        const double c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        return normalised ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    // band-pass, 0 dB peak gain
    static BiquadCoeffs bandPass (double w0, double q) noexcept
    {
        const double c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        return normalised (al, 0.0, -al, 1 + al, -2 * c, 1 - al);
    }
    static BiquadCoeffs lowShelf (double w0, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        const double s = 2.0 * std::sqrt (A) * al;
        return normalised (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
                           (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    static BiquadCoeffs highShelf (double w0, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        const double s = 2.0 * std::sqrt (A) * al;
        return normalised (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
                           (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }

    // Standard RBJ corner w0 = 2 pi f / fs (analog-colour filters).
    static double cornerStandard (double f, double fs) noexcept { return 2.0 * pi * std::min (f, 0.49 * fs) / fs; }

    // Spec 6 corner: w0 = 2 atan(pi f / fs) (bilinear mapping without pre-warping).
    static double cornerSpec (double f, double fs) noexcept { return 2.0 * std::atan (pi * f / fs); }

    // Analog second-order low-pass section with optional pair of transmission zeros at fz,
    //   H(s) = (s^2 + wz^2) / (s^2 + (w0/Q) s + w0^2) * w0^2 / wz^2      (unity DC gain),
    // discretised with the bilinear transform; w0 and wz are each pre-warped so that the section's
    // resonance and notch land at the same frequencies in Hz at every sample rate.
    static BiquadCoeffs analogLowPassSection (double f0, double q, double fz, double fs) noexcept
    {
        const double K = 2.0 * fs;
        const double w0 = K * std::tan (pi * std::min (f0, 0.45 * fs) / fs);
        double n2 = 0.0, n0 = w0 * w0; // numerator n2 s^2 + n0
        if (fz > 0.0 && fz < 0.49 * fs)
        {
            const double wz = K * std::tan (pi * fz / fs);
            n2 = w0 * w0 / (wz * wz);
        }
        const double d2 = 1.0, d1 = w0 / q, d0 = w0 * w0;
        const double KK = K * K;
        return normalised (n2 * KK + n0, -2 * n2 * KK + 2 * n0, n2 * KK + n0,
                           d2 * KK + d1 * K + d0, -2 * d2 * KK + 2 * d0, d2 * KK - d1 * K + d0);
    }
};

// Transposed direct form II biquad (double precision).
class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& c) noexcept { k = c; }
    const BiquadCoeffs& coeffs() const noexcept { return k; }
    void reset() noexcept { z1 = z2 = 0.0; }

    double process (double x) noexcept
    {
        const double y = k.b0 * x + z1;
        z1 = k.b1 * x - k.a1 * y + z2;
        z2 = k.b2 * x - k.a2 * y;
        return y;
    }

private:
    BiquadCoeffs k;
    double z1 = 0.0, z2 = 0.0;
};

} // namespace craft::hdelay
