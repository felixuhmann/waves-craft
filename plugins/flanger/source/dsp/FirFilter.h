// Craft Flanger - 9-tap linear-phase wet filter (spec r1 section 6).
// Windowed-sinc design (Oppenheim & Schafer), Kaiser window beta = 4.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace craft::flanger::dsp
{

inline constexpr int firTaps = 9;
inline constexpr int firCentre = 4; // group delay in samples
using FirKernel = std::array<double, firTaps>;

// Zeroth-order modified Bessel function of the first kind (power series).
inline double besselI0 (double x) noexcept
{
    double sum = 1.0, term = 1.0;
    const double q = 0.25 * x * x;
    for (int k = 1; k < 64; ++k)
    {
        term *= q / (double (k) * double (k));
        sum += term;
        if (term < 1.0e-17 * sum)
            break;
    }
    return sum;
}

// Kaiser(9, beta) window, peak 1, index 0..8 <-> n = -4..4.
inline FirKernel kaiserWindow (double beta = 4.0) noexcept
{
    FirKernel w {};
    const double norm = besselI0 (beta);
    for (int i = 0; i < firTaps; ++i)
    {
        const double t = double (i - firCentre) / double (firCentre);
        w[size_t (i)] = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - t * t))) / norm;
    }
    return w;
}

inline double sinc (double x) noexcept
{
    if (x == 0.0)
        return 1.0;
    const double px = 3.14159265358979323846 * x;
    return std::sin (px) / px;
}

// h_raw[n] = (2 fc/fs) sinc(2 fc n / fs) w[n]
inline FirKernel rawKernel (double fc, double fs, const FirKernel& window) noexcept
{
    FirKernel h {};
    const double k = 2.0 * fc / fs;
    for (int i = 0; i < firTaps; ++i)
        h[size_t (i)] = k * sinc (k * double (i - firCentre)) * window[size_t (i)];
    return h;
}

// Low-pass: h_raw normalised to unity DC gain.
inline FirKernel lowPassKernel (double fc, double fs, const FirKernel& window) noexcept
{
    auto h = rawKernel (fc, fs, window);
    double sum = 0.0;
    for (double v : h)
        sum += v;
    for (auto& v : h)
        v /= sum;
    return h;
}

// High-pass: delta - h_raw (deliberately not renormalised, spec 6 / 11.5).
inline FirKernel highPassKernel (double fc, double fs, const FirKernel& window) noexcept
{
    auto h = rawKernel (fc, fs, window);
    for (auto& v : h)
        v = -v;
    h[firCentre] += 1.0;
    return h;
}

} // namespace craft::flanger::dsp
