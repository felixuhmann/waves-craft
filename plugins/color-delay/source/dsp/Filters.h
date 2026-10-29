// Craft Colour Delay - filter building blocks and the third-order HP/LP pair (spec r1 section 4).
#pragma once

#include <cmath>
#include <complex>

namespace craft::cdl
{

inline constexpr double pi = 3.14159265358979323846;

// First-order section, transposed direct form II: H(z) = (b0 + b1 z^-1) / (1 + a1 z^-1)
struct FirstOrder
{
    double b0 = 1.0, b1 = 0.0, a1 = 0.0, s = 0.0;

    double process (double x) noexcept
    {
        const double y = b0 * x + s;
        s = b1 * x - a1 * y;
        return y;
    }
    void reset() noexcept { s = 0.0; }
    std::complex<double> response (double w) const noexcept
    {
        const auto z1 = std::polar (1.0, -w);
        return (b0 + b1 * z1) / (1.0 + a1 * z1);
    }
};

// Biquad, transposed direct form II: H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, s1 = 0.0, s2 = 0.0;

    double process (double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { s1 = s2 = 0.0; }
    void setCoefficients (double n0, double n1, double n2, double d0, double d1, double d2) noexcept
    {
        b0 = n0 / d0; b1 = n1 / d0; b2 = n2 / d0; a1 = d1 / d0; a2 = d2 / d0;
    }
    std::complex<double> response (double w) const noexcept
    {
        const auto z1 = std::polar (1.0, -w), z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }

    // Audio EQ Cookbook designs (bilinear transform, prewarped at f0).
    void setLowPass (double f0, double q, double fs) noexcept
    {
        const double w = 2.0 * pi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setCoefficients ((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + al, -2.0 * c, 1.0 - al);
    }
    void setHighPass (double f0, double q, double fs) noexcept
    {
        const double w = 2.0 * pi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setCoefficients ((1.0 + c) / 2.0, -(1.0 + c), (1.0 + c) / 2.0, 1.0 + al, -2.0 * c, 1.0 - al);
    }
    void setPeak (double f0, double q, double gainDb, double fs) noexcept
    {
        const double w = 2.0 * pi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);
        setCoefficients (1.0 + al * A, -2.0 * c, 1.0 - al * A, 1.0 + al / A, -2.0 * c, 1.0 - al / A);
    }
    void setAllPass (double f0, double q, double fs) noexcept
    {
        const double w = 2.0 * pi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setCoefficients (1.0 - al, -2.0 * c, 1.0 + al, 1.0 + al, -2.0 * c, 1.0 - al);
    }
};

// Bilinear transform with s = (1 - z^-1) / (k (1 + z^-1)), k = tan(pi fc / fs) (s normalised to the cutoff).
// First order: (p1 s + p0) / (s + 1)
inline void bilinearFirstOrder (FirstOrder& f, double p1, double p0, double k) noexcept
{
    const double d0 = 1.0 + k, d1 = k - 1.0;
    f.b0 = (p1 + p0 * k) / d0;
    f.b1 = (p0 * k - p1) / d0;
    f.a1 = d1 / d0;
}

// Second order: (p2 s^2 + p1 s + p0) / (s^2 + q1 s + 1)
inline void bilinearSecondOrder (Biquad& f, double p2, double p1, double p0, double q1, double k) noexcept
{
    const double k2 = k * k;
    f.setCoefficients (p2 + p1 * k + p0 * k2, -2.0 * p2 + 2.0 * p0 * k2, p2 - p1 * k + p0 * k2,
                       1.0 + q1 * k + k2, -2.0 + 2.0 * k2, 1.0 - q1 * k + k2);
}

// Third-order complementary HP/LP (spec section 4):
//   D(s) = (s + 1)(s^2 + (1 + a) s + 1),  LP = (1 - a s^2) / D,  HP = (s^3 - a s) / D,  a = 0.132316
// realised as a first-order section followed by a second-order section.
class CrossoverFilter
{
public:
    static constexpr double shapeA = 0.132316;

    void design (bool highPass, double fc, double fs) noexcept
    {
        const double k = std::tan (pi * fc / fs);
        if (highPass)
        {
            bilinearFirstOrder (first, 1.0, 0.0, k);                       // s / (s + 1)
            bilinearSecondOrder (second, 1.0, 0.0, -shapeA, 1.0 + shapeA, k); // (s^2 - a) / (s^2 + (1+a) s + 1)
        }
        else
        {
            bilinearFirstOrder (first, 0.0, 1.0, k);                       // 1 / (s + 1)
            bilinearSecondOrder (second, -shapeA, 0.0, 1.0, 1.0 + shapeA, k); // (1 - a s^2) / (...)
        }
    }
    double process (double x) noexcept { return second.process (first.process (x)); }
    void reset() noexcept { first.reset(); second.reset(); }
    std::complex<double> response (double w) const noexcept { return first.response (w) * second.response (w); }

private:
    FirstOrder first;
    Biquad second;
};

// Analog prototype of the pair, evaluated at the prewarped frequency (reference for tests).
inline std::complex<double> crossoverPrototype (bool highPass, double f, double fc, double fs) noexcept
{
    const double a = CrossoverFilter::shapeA;
    const std::complex<double> s (0.0, std::tan (pi * f / fs) / std::tan (pi * fc / fs));
    const auto d = (s + 1.0) * (s * s + (1.0 + a) * s + 1.0);
    return highPass ? (s * s * s - a * s) / d : (1.0 - a * s * s) / d;
}

} // namespace craft::cdl
