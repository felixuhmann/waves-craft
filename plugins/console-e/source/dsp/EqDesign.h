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
            if (! bell)
                return designBand (BandKind::highShelf, lookup (tables::hfShelf, freqR, 0, gainDb), fs);
            // the 16 / 20 kHz columns of the HF bell rows carry the reference's 48 kHz realisation; above 48 kHz
            // the parameters move toward the analogue prototype fitted to the columns up to 12.5 kHz
            auto p = lookup (tables::hfBell, freqR, 0, gainDb);
            const double w = std::clamp ((fs - 48000.0) / 48000.0, 0.0, 1.0);
            if (w > 0.0)
            {
                const auto h = lookup (tables::hfBellHighRate, freqR, 0, gainDb);
                for (size_t i = 0; i < 4; ++i)
                    p[i] += w * (h[i] - p[i]);
            }
            return designBand (BandKind::bell, p, fs);
        }
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

// Low-pass (spec r2 section 4, Appendix A.2): an analogue pole pair (f0, Q) mapped by impulse invariance and a
// normalised second-order numerator 1 + c1 z^-1 + c2 z^-2 with the same coefficients at every sample rate, unity at
// DC. At 48 kHz this reproduces A.2 within 0.005 dB; the near-Nyquist roll-off it adds scales with the sample rate
// (steeper at 44.1/48 kHz, analogue-like at 96 kHz), as described for the reference. r = 0 is Off.
inline BandCoeffs designLowPass (double r, double fs) noexcept
{
    BandCoeffs out;
    if (r <= 0.0)
        return out;
    const double f0 = detail::tableLog (tables::lpF0, std::max (r, 0.1));
    double q = detail::tableLin (tables::lpQ, r);
    // r < 0.4: the fitted pole pair sits at 0.8 ... 1 x Nyquist at 48 kHz and, with its numerator, only shapes the
    // response near Nyquist. Where it would lie well inside the band (higher rates) the section moves to a plain
    // analogue-matched two-pole whose Q matches A.2 at 12.5 ... 20 kHz (0.85 / 0.66 / 0.60 at r = 0.1 / 0.2 / 0.3).
    double wModel = 1.0;
    if (r < 0.4)
    {
        static constexpr double qAnalogue[] = { 0.851, 0.664, 0.60, 0.6872 };
        const double x = std::clamp (r, 0.1, 0.4) * 10.0 - 1.0;
        const int i = std::min (int (x), 2);
        const double qa = qAnalogue[i] + (x - i) * (qAnalogue[i + 1] - qAnalogue[i]);
        wModel = std::clamp ((96000.0 - fs) / 48000.0, 0.0, 1.0); // 48 kHz model up to 48 kHz, analogue from 96 kHz
        q = qa + (q - qa) * wModel;
    }
    const double c1 = detail::tableLin (tables::lpC1, r);
    const double c2 = detail::tableLin (tables::lpC2, r);
    const double w = 2 * pi * std::min (f0, 0.998 * fs);
    BiquadCoeffs c;
    matchedPoles (w / q, w * w, 1.0 / fs, c.a1, c.a2);
    const double k = (1.0 + c.a1 + c.a2) / (1.0 + c1 + c2);
    c.b0 = k;
    c.b1 = k * c1;
    c.b2 = k * c2;
    if (wModel < 1.0)
    {
        AnalogSection s;
        s.b2 = 0; s.b1 = 0; s.b0 = w * w;
        s.a1 = w / q; s.a0 = w * w;
        const auto m = designMatched (s, fs); // same poles, magnitude-matched numerator
        c.b0 = wModel * c.b0 + (1 - wModel) * m.b0;
        c.b1 = wModel * c.b1 + (1 - wModel) * m.b1;
        c.b2 = wModel * c.b2 + (1 - wModel) * m.b2;
    }
    out.s[0] = c;
    out.count = 1;
    return out;
}

} // namespace craft::consolee::dsp
