// Craft Console E - coefficient design for the filters and EQ bands (spec r1 sections 4 and 5, Appendix A).
//
// Each band's response for a control setting is defined by the tables in EqTables.h: a grid over (frequency
// control r, Q control r, gain label) whose nodes reproduce every Appendix A row; between nodes the section
// parameters are interpolated multilinearly. The parameters are converted to analogue second-order prototypes
// and realised with the analogue-matched design of MatchedBiquad.h, so the response is the same at every
// sample rate (no warping toward Nyquist).
#pragma once

#include <array>

#include "EqTables.h"
#include "MatchedBiquad.h"

namespace craft::consolee::dsp
{

enum class BandKind { bell, lowShelf, highShelf };

inline constexpr double lowShelfCompanionRatio = 15.0; // companion bell of the LF shelf: frequency ratio ...
inline constexpr double lowShelfCompanionQ = 0.4;       // ... and Q

// Up to two digital sections per band.
struct BandCoeffs
{
    std::array<BiquadCoeffs, 2> s { BiquadCoeffs::identity(), BiquadCoeffs::identity() };
    int count = 0;

    double magnitudeDb (double f, double fs) const noexcept
    {
        double db = 0;
        for (int i = 0; i < count; ++i)
            db += s[size_t (i)].magnitudeDb (f, fs);
        return db;
    }
};

namespace detail
{
inline void locate (const double* axis, int n, double x, int& i, double& w) noexcept
{
    if (n <= 1) { i = 0; w = 0; return; }
    if (x <= axis[0]) { i = 0; w = 0; return; }
    if (x >= axis[n - 1]) { i = n - 2; w = 1; return; }
    i = 0;
    while (i < n - 2 && axis[i + 1] <= x)
        ++i;
    w = (x - axis[i]) / (axis[i + 1] - axis[i]);
}
} // namespace detail

// Multilinear interpolation of the four node parameters.
inline std::array<double, 4> lookup (const tables::BandGrid& grid, double fr, double qr, double gainDb) noexcept
{
    int i, j, k;
    double wf, wq, wg;
    detail::locate (grid.f, grid.nf, fr, i, wf);
    detail::locate (grid.q, grid.nq, qr, j, wq);
    detail::locate (grid.g, grid.ng, gainDb, k, wg);
    std::array<double, 4> out {};
    const int nqSpan = grid.nq > 1 ? 2 : 1;
    for (int a = 0; a < 2; ++a)
        for (int b = 0; b < nqSpan; ++b)
            for (int c = 0; c < 2; ++c)
            {
                const double w = (a ? wf : 1 - wf) * (grid.nq > 1 ? (b ? wq : 1 - wq) : 1.0) * (c ? wg : 1 - wg);
                if (w == 0.0)
                    continue;
                const double* v = grid.v + (((i + a) * grid.nq + (j + b)) * grid.ng + (k + c)) * 4;
                for (int p = 0; p < 4; ++p)
                    out[size_t (p)] += w * v[p];
            }
    return out;
}

// Tilted bell: peak gain G at f0, base Q, high-frequency gain h (dB).
inline AnalogSection bellPrototype (double f0, double gainDb, double q, double tiltDb) noexcept
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2 * pi * f0;
    AnalogSection s;
    s.b2 = std::pow (10.0, tiltDb / 20.0);
    s.b1 = A * w / q;
    s.b0 = w * w;
    s.a1 = w / (A * q);
    s.a0 = w * w;
    return s;
}

// Second-order low shelf (gain G below f), RBJ-style analogue prototype.
inline AnalogSection lowShelfPrototype (double f, double gainDb, double q) noexcept
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2 * pi * f, sA = std::sqrt (A);
    AnalogSection s;
    s.b2 = 1;
    s.b1 = sA / q * w;
    s.b0 = A * w * w;
    s.a1 = sA / q * w / A;
    s.a0 = w * w / A;
    return s;
}

// Design one EQ band from its four table parameters.
inline BandCoeffs designBand (BandKind kind, const std::array<double, 4>& p, double fs) noexcept
{
    BandCoeffs out;
    switch (kind)
    {
        case BandKind::bell:
        {
            const double gain = p[1];
            if (std::abs (gain) < 1e-9 && std::abs (p[3]) < 1e-9)
                return out;
            if (gain >= 0.0)
                out.s[0] = designMatched (bellPrototype (std::exp (p[0]), gain, std::exp (p[2]), p[3]), fs);
            else // cuts are realised as the reciprocal of the matching boost
                out.s[0] = designMatched (bellPrototype (std::exp (p[0]), -gain, std::exp (p[2]), -p[3]), fs).inverted();
            out.count = 1;
            return out;
        }
        case BandKind::lowShelf:
        {
            if (std::abs (p[1]) < 1e-9 && std::abs (p[3]) < 1e-9)
                return out;
            const double f = std::exp (p[0]);
            out.s[0] = designMatched (lowShelfPrototype (f, p[1], std::exp (p[2])), fs);
            out.s[1] = designMatched (bellPrototype (f * lowShelfCompanionRatio, p[3], lowShelfCompanionQ, 0.0), fs);
            out.count = 2;
            return out;
        }
        case BandKind::highShelf:
        {
            const double fz = std::exp (p[0]), qz = std::exp (p[1]), fp = std::exp (p[2]), qp = std::exp (p[3]);
            if (std::abs (fz - fp) < 1e-9 * fp && std::abs (qz - qp) < 1e-9 * qp)
                return out;
            const double wz = 2 * pi * fz, wp = 2 * pi * fp, k = (wp * wp) / (wz * wz);
            AnalogSection s;
            s.b2 = k;
            s.b1 = k * wz / qz;
            s.b0 = k * wz * wz;
            s.a1 = wp / qp;
            s.a0 = wp * wp;
            out.s[0] = designMatched (s, fs);
            out.count = 1;
            return out;
        }
    }
    return out;
}

// ---- band identities ------------------------------------------------------------------------------
enum class Band { lf, lmf, hmf, hf };

inline BandCoeffs designEqBand (Band band, bool bell, double freqR, double qR, double gainDb, double fs) noexcept
{
    switch (band)
    {
        case Band::lf:
            return bell ? designBand (BandKind::bell, lookup (tables::lfBell, freqR, 0, gainDb), fs)
                        : designBand (BandKind::lowShelf, lookup (tables::lfShelf, freqR, 0, gainDb), fs);
        case Band::lmf: return designBand (BandKind::bell, lookup (tables::lmf, freqR, qR, gainDb), fs);
        case Band::hmf: return designBand (BandKind::bell, lookup (tables::hmf, freqR, qR, gainDb), fs);
        case Band::hf:
            return bell ? designBand (BandKind::bell, lookup (tables::hfBell, freqR, 0, gainDb), fs)
                        : designBand (BandKind::highShelf, lookup (tables::hfShelf, freqR, 0, gainDb), fs);
    }
    return {};
}

// ---- high-pass / low-pass -------------------------------------------------------------------------
namespace detail
{
// log-linear interpolation of a 10-entry table at r = 0.1 ... 1.0 (extrapolated below 0.1 from the first segment)
inline double tableLog (const double* t, double r) noexcept
{
    const double x = std::clamp (r, 0.0, 1.0) * 10.0 - 1.0; // index
    int i = int (std::floor (x));
    i = std::clamp (i, 0, 8);
    const double w = x - i;
    return std::exp (std::log (t[i]) + w * (std::log (t[i + 1]) - std::log (t[i])));
}
inline double tableLin (const double* t, double r) noexcept
{
    const double x = std::clamp (r, 0.1, 1.0) * 10.0 - 1.0;
    const int i = std::clamp (int (std::floor (x)), 0, 8);
    const double w = x - i;
    return t[i] + w * (t[i + 1] - t[i]);
}
} // namespace detail

// Third-order Butterworth high-pass (real pole + pole pair with Q = 1, all at fc). r = 0 is Off (identity).
inline BandCoeffs designHighPass (double r, double fs) noexcept
{
    BandCoeffs out;
    if (r <= 0.0)
        return out;
    const double fc = detail::tableLog (tables::hpCorner, r);
    const double w = 2 * pi * fc;
    out.s[0] = designMatchedFirstOrder (1.0, 0.0, w, fs);
    AnalogSection s;
    s.b2 = 1; s.b1 = 0; s.b0 = 0;
    s.a1 = w / 1.0; s.a0 = w * w;
    out.s[1] = designMatched (s, fs, std::min (w, 0.5 * pi * fs));
    out.count = 2;
    return out;
}

// Low-pass: two-pole section (f, Q) and a first-order pole. r = 0 is Off.
inline BandCoeffs designLowPass (double r, double fs) noexcept
{
    BandCoeffs out;
    if (r <= 0.0)
        return out;
    const double f = detail::tableLog (tables::lpFreq, r);
    const double q = detail::tableLin (tables::lpQ, r);
    const double f2 = detail::tableLog (tables::lpPole, r);
    const double w = 2 * pi * f, w2 = 2 * pi * f2;
    AnalogSection s;
    s.b2 = 0; s.b1 = 0; s.b0 = w * w;
    s.a1 = w / q; s.a0 = w * w;
    out.s[0] = designMatched (s, fs);
    out.s[1] = designMatchedFirstOrder (0.0, w2, w2, fs);
    out.count = 2;
    return out;
}

} // namespace craft::consolee::dsp
