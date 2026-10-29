// Craft Console G - second-order sections, bilinear transform (no prewarping) and magnitude-matched design.
#pragma once

#include <array>
#include <cmath>
#include <complex>

#include "MathUtil.h"

namespace craft::consoleg::dsp
{

// H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    std::complex<double> response (double freqHz, double fs) const noexcept
    {
        const auto z1 = std::polar (1.0, -2.0 * pi * freqHz / fs);
        const auto z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
    double magnitudeDb (double freqHz, double fs) const noexcept { return gainToDb (std::abs (response (freqHz, fs))); }

    static BiquadCoeffs lerp (const BiquadCoeffs& a, const BiquadCoeffs& b, double t) noexcept
    {
        return { a.b0 + t * (b.b0 - a.b0), a.b1 + t * (b.b1 - a.b1), a.b2 + t * (b.b2 - a.b2), a.a1 + t * (b.a1 - a.a1),
                 a.a2 + t * (b.a2 - a.a2) };
    }
};

// Transposed direct form II, double precision state.
struct Biquad
{
    BiquadCoeffs c;
    double z1 = 0.0, z2 = 0.0;

    double process (double x) noexcept
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0; }
    void sanitise() noexcept
    {
        if (! std::isfinite (z1) || ! std::isfinite (z2) || std::abs (z1) > 1.0e6 || std::abs (z2) > 1.0e6)
            reset();
    }
};

// Bilinear transform s = 2 fs (1 - z^-1) / (1 + z^-1), deliberately WITHOUT frequency prewarping (spec 3.1, 4, 8):
// H(s) = (B2 s^2 + B1 s + B0) / (A2 s^2 + A1 s + A0), s in rad/s.
inline BiquadCoeffs bilinear2 (double B2, double B1, double B0, double A2, double A1, double A0, double fs) noexcept
{
    const double K = 2.0 * fs, K2 = K * K;
    const double n0 = B2 * K2 + B1 * K + B0, n1 = 2.0 * (B0 - B2 * K2), n2 = B2 * K2 - B1 * K + B0;
    const double d0 = A2 * K2 + A1 * K + A0, d1 = 2.0 * (A0 - A2 * K2), d2 = A2 * K2 - A1 * K + A0;
    return { n0 / d0, n1 / d0, n2 / d0, d1 / d0, d2 / d0 };
}

// First order: H(s) = (B1 s + B0) / (A1 s + A0) (b2 = a2 = 0).
inline BiquadCoeffs bilinear1 (double B1, double B0, double A1, double A0, double fs) noexcept
{
    const double K = 2.0 * fs;
    const double d0 = A1 * K + A0;
    return { (B1 * K + B0) / d0, (B0 - B1 * K) / d0, 0.0, (A0 - A1 * K) / d0, 0.0 };
}

// ---------------------------------------------------------------------------------------------
// Magnitude-matched design (after M. Vicanek, "Matched Second Order Digital Filters", 2016): with
// phi0 = cos^2(w/2), phi1 = sin^2(w/2), phi2 = 4 phi0 phi1 the squared magnitude of a biquad is
//   |H|^2 = (B0 phi0 + B1 phi1 + B2 phi2) / (A0 phi0 + A1 phi1 + A2 phi2),
//   B0 = (b0+b1+b2)^2, B1 = (b0-b1+b2)^2, B2 = -4 b0 b2 (A likewise with 1, a1, a2).
// The poles are given; the numerator is the weighted least-squares fit of the target magnitude (relative error),
// which is linear in (B0, B1, B2), followed by the factorisation back to (b0, b1, b2).
struct MagnitudeFit
{
    static std::array<double, 3> phis (double w) noexcept
    {
        const double s = std::sin (0.5 * w), p1 = s * s, p0 = 1.0 - p1;
        return { p0, p1, 4.0 * p0 * p1 };
    }

    // Poles of an analog pair s^2 + (w0/Q) s + w0^2 mapped by z = exp(s T) (impulse-invariant / "matched" poles).
    static void matchedPoles (double f0, double q, double fs, double& a1, double& a2) noexcept
    {
        const double w = 2.0 * pi * f0 / fs;
        const double zeta = 1.0 / (2.0 * q);
        const double e = std::exp (-zeta * w);
        if (zeta <= 1.0)
            a1 = -2.0 * e * std::cos (w * std::sqrt (1.0 - zeta * zeta));
        else
            a1 = -2.0 * e * std::cosh (w * std::sqrt (zeta * zeta - 1.0));
        a2 = e * e;
    }

    // n points: digital frequencies w[i] (rad/sample), target linear magnitude mag[i], weight wt[i].
    // Returns false if the system is singular.
    static bool fitNumerator (double a1, double a2, const double* w, const double* mag, const double* wt, int n,
                              BiquadCoeffs& out) noexcept
    {
        const double A0 = (1 + a1 + a2) * (1 + a1 + a2), A1 = (1 - a1 + a2) * (1 - a1 + a2), A2 = -4.0 * a2;
        double M[3][3] = {}, v[3] = {};
        for (int i = 0; i < n; ++i)
        {
            const auto p = phis (w[i]);
            const double den = A0 * p[0] + A1 * p[1] + A2 * p[2];
            const double t = mag[i] * mag[i] * den;
            if (! (t > 0.0))
                continue;
            // relative residual: (B . phi) / t - 1
            const double r[3] = { p[0] / t, p[1] / t, p[2] / t };
            for (int a = 0; a < 3; ++a)
            {
                for (int b = 0; b < 3; ++b)
                    M[a][b] += wt[i] * r[a] * r[b];
                v[a] += wt[i] * r[a];
            }
        }
        double B[3];
        if (! solve3 (M, v, B))
            return false;
        out = factor (B[0], B[1], B[2]);
        out.a1 = a1;
        out.a2 = a2;
        return true;
    }

    // (B0, B1, B2) -> (b0, b1, b2), projecting onto the realisable set when the fit left it.
    static BiquadCoeffs factor (double B0, double B1, double B2) noexcept
    {
        B0 = std::max (B0, 0.0);
        B1 = std::max (B1, 0.0);
        const double s0 = std::sqrt (B0), s1 = std::sqrt (B1);
        const double W = 0.5 * (s0 + s1); // b0 + b2
        const double b1 = 0.5 * (s0 - s1);
        const double disc = std::max (W * W + B2, 0.0);
        const double b0 = 0.5 * (W + std::sqrt (disc));
        const double b2 = W - b0;
        return { b0, b1, b2, 0.0, 0.0 };
    }

    static bool solve3 (double M[3][3], double v[3], double x[3]) noexcept
    {
        double A[3][4];
        for (int r = 0; r < 3; ++r)
        {
            for (int c = 0; c < 3; ++c)
                A[r][c] = M[r][c];
            A[r][3] = v[r];
        }
        for (int c = 0; c < 3; ++c)
        {
            int piv = c;
            for (int r = c + 1; r < 3; ++r)
                if (std::abs (A[r][c]) > std::abs (A[piv][c]))
                    piv = r;
            if (std::abs (A[piv][c]) < 1.0e-300)
                return false;
            if (piv != c)
                for (int k = 0; k < 4; ++k)
                    std::swap (A[c][k], A[piv][k]);
            for (int r = 0; r < 3; ++r)
            {
                if (r == c)
                    continue;
                const double f = A[r][c] / A[c][c];
                for (int k = c; k < 4; ++k)
                    A[r][k] -= f * A[c][k];
            }
        }
        for (int r = 0; r < 3; ++r)
            x[r] = A[r][3] / A[r][r];
        return std::isfinite (x[0]) && std::isfinite (x[1]) && std::isfinite (x[2]);
    }
};

} // namespace craft::consoleg::dsp
