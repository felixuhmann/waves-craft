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
        {
            // measured rows differ per rate (spec r3 C.3): 44.1, 48 and 96 kHz sets, interpolated in log(fs)
            const auto kind = bell ? BandKind::bell : BandKind::highShelf;
            auto p = lookup (bell ? tables::hfBell : tables::hfShelf, freqR, 0, gainDb);
            if (fs < 48000.0)
            {
                const auto h = lookup (bell ? tables::hfBell44 : tables::hfShelf44, freqR, 0, gainDb);
                const double w = std::clamp (std::log (48000.0 / fs) / std::log (48000.0 / 44100.0), 0.0, 1.0);
                for (size_t i = 0; i < 4; ++i)
                    p[i] += w * (h[i] - p[i]);
            }
            else if (fs > 48000.0)
            {
                const auto h = lookup (bell ? tables::hfBell96 : tables::hfShelf96, freqR, 0, gainDb);
                const double w = std::clamp (std::log (fs / 48000.0) / std::log (2.0), 0.0, 1.0);
                for (size_t i = 0; i < 4; ++i)
                    p[i] += w * (h[i] - p[i]);
            }
            return designBand (kind, p, fs);
        }
    }
    return {};
}

// ---- high-pass / low-pass ---------------------------------------------------------------------------
namespace detail
{
// position r -> index and weight in tables with entries at r = 0.05 ... 1 (clamped below 0.05)
inline void position (double r, int& i, double& w) noexcept
{
    const double x = std::clamp (r, 0.05, 1.0) * 20.0 - 1.0;
    i = std::clamp (int (x), 0, 18);
    w = x - i;
}
} // namespace detail

// Third-order Butterworth high-pass (real pole + pole pair with Q = 1, all at fc). r = 0 is Off (identity).
inline BandCoeffs designHighPass (double r, double fs) noexcept
{
    BandCoeffs out;
    if (r <= 0.0)
        return out;
    int i;
    double t;
    detail::position (r, i, t);
    const double fc = std::exp ((1 - t) * std::log (tables::hpCorner[i]) + t * std::log (tables::hpCorner[i + 1]));
    const double w = 2 * pi * fc;
    out.s[0] = designMatchedFirstOrder (1.0, 0.0, w, fs);
    AnalogSection s;
    s.b2 = 1; s.b1 = 0; s.b0 = 0;
    s.a1 = w; s.a0 = w * w;
    out.s[1] = designMatched (s, fs, std::min (w, 0.5 * pi * fs));
    out.count = 2;
    return out;
}

namespace detail
{
struct LowPassTable { const double* f0; const double* q; const double* c1; const double* c2; };

// one table position realised at rate fs: impulse-invariant pole pair, normalised numerator, unity at DC
inline BiquadCoeffs lowPassEntry (const LowPassTable& t, int i, double fs) noexcept
{
    const double w = 2 * pi * std::min (t.f0[i], 0.998 * fs);
    BiquadCoeffs c;
    matchedPoles (w / t.q[i], w * w, 1.0 / fs, c.a1, c.a2);
    const double k = (1.0 + c.a1 + c.a2) / (1.0 + t.c1[i] + t.c2[i]);
    c.b0 = k;
    c.b1 = k * t.c1[i];
    c.b2 = k * t.c2[i];
    return c;
}

inline BiquadCoeffs mix (const BiquadCoeffs& a, const BiquadCoeffs& b, double w) noexcept
{
    BiquadCoeffs c;
    c.b0 = a.b0 + w * (b.b0 - a.b0);
    c.b1 = a.b1 + w * (b.b1 - a.b1);
    c.b2 = a.b2 + w * (b.b2 - a.b2);
    c.a1 = a.a1 + w * (b.a1 - a.a1);
    c.a2 = a.a2 + w * (b.a2 - a.a2);
    return c;
}

// coefficient interpolation between the positions (a convex mix of stable sections stays stable)
inline BiquadCoeffs lowPassAt (const LowPassTable& t, int i, double w, double fs) noexcept
{
    return mix (lowPassEntry (t, i, fs), lowPassEntry (t, i + 1, fs), w);
}
} // namespace detail

// Low-pass (spec r3 section 4, A.2, C.1, C.2): measured per rate, one biquad per position and rate; between the
// measured rates the coefficients are interpolated in log(fs); above 96 kHz an analogue two-pole fitted to the
// 96 kHz targets. r = 0 is Off.
inline BandCoeffs designLowPass (double r, double fs) noexcept
{
    BandCoeffs out;
    if (r <= 0.0)
        return out;
    using namespace tables;
    int i;
    double t;
    detail::position (r, i, t);
    const detail::LowPassTable t44 { lpF0_44, lpQ_44, lpC1_44, lpC2_44 }, t48 { lpF0_48, lpQ_48, lpC1_48, lpC2_48 },
                               t96 { lpF0_96, lpQ_96, lpC1_96, lpC2_96 };
    if (fs <= 44100.0)
        out.s[0] = detail::lowPassAt (t44, i, t, fs);
    else if (fs < 48000.0)
        out.s[0] = detail::mix (detail::lowPassAt (t44, i, t, fs), detail::lowPassAt (t48, i, t, fs),
                                std::log (fs / 44100.0) / std::log (48000.0 / 44100.0));
    else if (fs <= 96000.0)
        out.s[0] = detail::mix (detail::lowPassAt (t48, i, t, fs), detail::lowPassAt (t96, i, t, fs), std::log (fs / 48000.0) / std::log (2.0));
    else
    {
        const auto entry = [fs] (int k) {
            const double w = 2 * pi * lpF0_analogue[k];
            AnalogSection s;
            s.b2 = 0; s.b1 = 0; s.b0 = w * w;
            s.a1 = w / lpQ_analogue[k]; s.a0 = w * w;
            return designMatched (s, fs);
        };
        out.s[0] = detail::mix (entry (i), entry (i + 1), t);
    }
    out.count = 1;
    return out;
}

} // namespace craft::consolee::dsp
