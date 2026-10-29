// Craft Console G - spec section 4: LF / HF shelves (bilinear of tabulated prototypes), LMF bell (bilinear of an
// analog peaking bell with tabulated effective parameters) and HMF bell (magnitude-matched to an analog bell).
#pragma once

#include <array>
#include <cmath>
#include <map>
#include <vector>

#include "Biquad.h"
#include "Csv.h"
#include "MathUtil.h"
#include "Tapers.h"
#include "console_g/SpecTables.h"

namespace craft::consoleg::dsp
{

// ---------------------------------------------------------------------------------------------
// Shelf prototype H(s) = (B2 s^2 + B1 s + B0) / (s^2 + A1 s + A0) in the interpolation representation of spec 4.1:
// q = (ln B2, ln wz, ln zz, ln wp, ln zp), wz = sqrt(B0/B2), zz = B1/(2 B2 wz), wp = sqrt(A0), zp = A1/(2 wp).
struct ShelfPrototype
{
    double B2 = 1, B1 = 0, B0 = 0, A1 = 0, A0 = 0;

    std::array<double, 5> toQ() const noexcept
    {
        const double wz = std::sqrt (B0 / B2), zz = B1 / (2.0 * B2 * wz), wp = std::sqrt (A0), zp = A1 / (2.0 * wp);
        return { std::log (B2), std::log (wz), std::log (zz), std::log (wp), std::log (zp) };
    }
    static ShelfPrototype fromQ (const std::array<double, 5>& q) noexcept
    {
        const double B2 = std::exp (q[0]), wz = std::exp (q[1]), zz = std::exp (q[2]), wp = std::exp (q[3]),
                     zp = std::exp (q[4]);
        return { B2, 2.0 * B2 * zz * wz, B2 * wz * wz, 2.0 * zp * wp, wp * wp };
    }
    std::complex<double> response (double f) const noexcept
    {
        const std::complex<double> s (0.0, 2.0 * pi * f);
        return (B2 * s * s + B1 * s + B0) / (s * s + A1 * s + A0);
    }
    BiquadCoeffs digitise (double fs) const noexcept { return bilinear2 (B2, B1, B0, 1.0, A1, A0, fs); }
};

class ShelfTable
{
public:
    static const ShelfTable& lf()
    {
        static const ShelfTable t (spec_tables::lf_shelf, spec_tables::lf_shelf_count);
        return t;
    }
    static const ShelfTable& hf()
    {
        static const ShelfTable t (spec_tables::hf_shelf, spec_tables::hf_shelf_count);
        return t;
    }

    struct Point
    {
        double gain, freq;
        ShelfPrototype proto;
    };
    const std::vector<Point>& points() const noexcept { return pts; }

    // Prototype for display gain g (dB) and display frequency f (Hz); exactly flat at g = 0.
    ShelfPrototype prototype (double g, double f) const noexcept
    {
        if (std::abs (g) < 1.0e-9)
            return { 1.0, 0.0, 1.0, 0.0, 1.0 }; // H = 1 (identical numerator and denominator after digitising)
        const double x = std::log10 (std::max (f, 1.0));
        std::array<double, 5> q;
        if (x <= rowLogF.front() || rows.size() == 1)
            q = rows.front().eval (g);
        else if (x >= rowLogF.back())
            q = rows.back().eval (g);
        else
        {
            const size_t i = segmentIndex (rowLogF, x);
            const double t = (x - rowLogF[i]) / (rowLogF[i + 1] - rowLogF[i]);
            const auto q0 = rows[i].eval (g), q1 = rows[i + 1].eval (g);
            for (size_t k = 0; k < 5; ++k)
                q[k] = q0[k] + t * (q1[k] - q0[k]);
        }
        return ShelfPrototype::fromQ (q);
    }

private:
    struct FreqRow
    {
        double freq = 0;
        std::array<Pchip, 5> curves;
        std::array<double, 5> eval (double g) const noexcept
        {
            std::array<double, 5> q;
            for (size_t k = 0; k < 5; ++k)
                q[k] = curves[k] (g);
            return q;
        }
    };

    ShelfTable (const char* const* lines, int count)
    {
        const auto t = parseCsv (lines, count);
        const int cG = t.column ("gain_dB"), cF = t.column ("freq_Hz"), cB0 = t.column ("B0"), cB1 = t.column ("B1"),
                  cB2 = t.column ("B2"), cA0 = t.column ("A0"), cA1 = t.column ("A1");
        std::map<double, std::map<double, ShelfPrototype>> byFreq;
        for (const auto& r : t.rows)
        {
            Point p { parseCell (r[size_t (cG)]), parseCell (r[size_t (cF)]),
                      { parseCell (r[size_t (cB2)]), parseCell (r[size_t (cB1)]), parseCell (r[size_t (cB0)]),
                        parseCell (r[size_t (cA1)]), parseCell (r[size_t (cA0)]) } };
            pts.push_back (p);
            byFreq[p.freq][p.gain] = p.proto;
        }
        for (const auto& [freq, gains] : byFreq)
        {
            // A frequency row needs points on both sides of 0 dB for the gain interpolation (the single LF 40 Hz
            // point is not usable as a row; it lies between the 30 and 50 Hz rows).
            bool neg = false, pos = false;
            for (const auto& gp : gains)
                (gp.first < 0 ? neg : pos) = true;
            if (gains.size() < 4 || ! neg || ! pos)
                continue;

            // flat point q_flat = (0, ln wp, ln zp, ln wp, ln zp) of the nearest non-zero gain (ties: mean)
            double nearest = 1e9;
            for (const auto& gp : gains)
                nearest = std::min (nearest, std::abs (gp.first));
            double lnWp = 0, lnZp = 0;
            int nNear = 0;
            for (const auto& gp : gains)
                if (std::abs (std::abs (gp.first) - nearest) < 1e-9)
                {
                    const auto q = gp.second.toQ();
                    lnWp += q[3];
                    lnZp += q[4];
                    ++nNear;
                }
            lnWp /= nNear;
            lnZp /= nNear;

            std::vector<double> gs;
            std::array<std::vector<double>, 5> qs;
            bool flatDone = false;
            for (const auto& gp : gains)
            {
                if (! flatDone && gp.first > 0)
                {
                    gs.push_back (0.0);
                    const std::array<double, 5> qf { 0.0, lnWp, lnZp, lnWp, lnZp };
                    for (size_t k = 0; k < 5; ++k)
                        qs[k].push_back (qf[k]);
                    flatDone = true;
                }
                gs.push_back (gp.first);
                const auto q = gp.second.toQ();
                for (size_t k = 0; k < 5; ++k)
                    qs[k].push_back (q[k]);
            }
            FreqRow row;
            row.freq = freq;
            for (size_t k = 0; k < 5; ++k)
                row.curves[k].set (gs, qs[k]);
            rows.push_back (std::move (row));
            rowLogF.push_back (std::log10 (freq));
        }
    }

    std::vector<Point> pts;
    std::vector<FreqRow> rows;
    std::vector<double> rowLogF;
};

// ---------------------------------------------------------------------------------------------
// Effective bell parameters (centre frequency, Q, gain) of the analog peaking bell
//   H(s) = (s^2 + s w0 A / Qe + w0^2) / (s^2 + s w0 / (A Qe) + w0^2),  A = 10^(Ge/40).
struct BellParams
{
    double f0 = 1000.0, q = 0.7, gainDb = 0.0;

    std::complex<double> response (double f) const noexcept
    {
        const double w0 = 2.0 * pi * f0, A = std::pow (10.0, gainDb / 40.0);
        const std::complex<double> s (0.0, 2.0 * pi * f);
        return (s * s + s * w0 * A / q + w0 * w0) / (s * s + s * w0 / (A * q) + w0 * w0);
    }
    BiquadCoeffs bilinear (double fs) const noexcept
    {
        const double w0 = 2.0 * pi * f0, A = std::pow (10.0, gainDb / 40.0);
        return bilinear2 (1.0, w0 * A / q, w0 * w0, 1.0, w0 / (A * q), w0 * w0, fs);
    }
    // Magnitude-matched digital bell (spec 4.3 / D4): matched poles, least-squares numerator.
    BiquadCoeffs matched (double fs) const noexcept
    {
        if (std::abs (gainDb) < 1.0e-9)
            return {};
        const double A = std::pow (10.0, gainDb / 40.0);
        const double fc = std::min (f0, 0.45 * fs);
        double a1, a2;
        MagnitudeFit::matchedPoles (fc, q * A, fs, a1, a2);
        constexpr int n = 40;
        double w[n + 1], mag[n + 1], wt[n + 1];
        const double fMax = 0.49 * fs;
        BellParams p = *this;
        p.f0 = fc;
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow (fMax / 20.0, double (i) / (n - 1));
            w[i] = 2.0 * pi * f / fs;
            mag[i] = std::abs (p.response (f));
            wt[i] = f > 20000.0 ? 0.05 : 1.0;
        }
        w[n] = 2.0 * pi * fc / fs; // extra weight at the centre
        mag[n] = std::abs (p.response (fc));
        wt[n] = 4.0;
        BiquadCoeffs c;
        if (! MagnitudeFit::fitNumerator (a1, a2, w, mag, wt, n + 1, c))
            return p.bilinear (fs);
        return c;
    }
};

// Interpolation of the tabulated effective parameters (spec 4.2 / 4.3). Separable scheme, exact at every table point:
//   value(g, Q, f) = base(g) * qRatio(g, rQ) * fRatio(sign g, f)
// - base(g): the series at Q 1.5 / base frequency, PCHIP along display gain (gain 0: Ge = 0)
// - qRatio: value(g_i, Q) / value(g_i, 1.5) at the base frequency, PCHIP along the host value rQ of Q, then
//   PCHIP along display gain across the gains that have a Q series
// - fRatio: frequency series at +-12 dB / Q 1.5, value(f) / value(base f), PCHIP in log f.
class BellTable
{
public:
    static const BellTable& lmf()
    {
        static const BellTable t (spec_tables::lmf_bell, spec_tables::lmf_bell_count, 500.0, TaperId::lmfQ, true);
        return t;
    }
    static const BellTable& hmf()
    {
        static const BellTable t (spec_tables::hmf_bell_target, spec_tables::hmf_bell_target_count, 3500.0,
                                  TaperId::hmfQ, false);
        return t;
    }

    struct Point
    {
        double gain, q, freq;
        bool div3;
        BellParams p;
    };
    const std::vector<Point>& points() const noexcept { return pts; }

    // g: display gain (dB), rQ: host value of the Q control, f: display frequency (Hz); shifted: LMF /3 (the x3 of
    // the HMF is applied by the caller to the effective centre frequency).
    BellParams params (double g, double rQ, double f, bool shifted = false) const noexcept
    {
        BellParams out;
        if (std::abs (g) < 1.0e-9)
        {
            out.f0 = f;
            out.gainDb = 0.0;
            return out;
        }
        const double ratioF = qRatio[0] (g, rQ), ratioQ = qRatio[1] (g, rQ), ratioG = qRatio[2] (g, rQ);
        const bool pos = g > 0;
        const auto& fs = pos ? freqPos : freqNeg;
        const double lf = std::log (std::max (f, 1.0));
        const double baseF = baseCurve[0] (g), baseQ = baseCurve[1] (g), baseG = baseCurve[2] (g);
        // f0_eff scales with the display frequency per the frequency series, with the gain-dependent factor
        // f0(g, base)/f0(+-12, base) (spec 4.2)
        out.f0 = fs[0] (lf) * (baseF * ratioF) / fs[0] (std::log (baseFreq));
        out.q = baseQ * ratioQ * fs[1] (lf) / fs[1] (std::log (baseFreq));
        out.gainDb = baseG * ratioG * fs[2] (lf) / fs[2] (std::log (baseFreq));
        if (shifted && hasDiv3)
        {
            // /3: own frequency mapping from the two /3 table points (log-log line), and their Q / gain corrections
            const double x = std::log (std::max (f, 1.0));
            const double t = (x - div3LogF[0]) / (div3LogF[1] - div3LogF[0]);
            const double f0Base = std::exp (div3LogF0[0] + t * (div3LogF0[1] - div3LogF0[0]));
            const double tc = std::min (1.0, std::max (0.0, t));
            const double qCorr = div3QCorr[0] + tc * (div3QCorr[1] - div3QCorr[0]);
            const double gCorr = div3GCorr[0] + tc * (div3GCorr[1] - div3GCorr[0]);
            out.f0 = f0Base * (baseF * ratioF) / fs[0] (std::log (baseFreq));
            out.q *= qCorr;
            out.gainDb *= gCorr;
        }
        return out;
    }

private:
    // 2-D ratio: PCHIP along rQ per gain, then PCHIP along gain. The gain curves are precomputed on a dense rQ grid
    // (linear in between) so that the audio thread never allocates.
    struct QRatio
    {
        static constexpr int gridSize = 401;
        std::vector<double> gains;
        std::vector<Pchip> alongQ;
        std::vector<Pchip> alongGain; // per rQ grid point

        void build()
        {
            alongGain.clear();
            for (int k = 0; k < gridSize; ++k)
            {
                const double rQ = double (k) / (gridSize - 1);
                std::vector<double> gs, vs;
                for (size_t i = 0; i < gains.size(); ++i)
                {
                    if (gains[i] > 0 && (gs.empty() || gs.back() < 0) && i > 0)
                    {
                        gs.push_back (0.0); // 0 dB point: mean of the two gains closest to zero on either side
                        vs.push_back (0.5 * (alongQ[i - 1] (rQ) + alongQ[i] (rQ)));
                    }
                    gs.push_back (gains[i]);
                    vs.push_back (alongQ[i] (rQ));
                }
                alongGain.emplace_back (gs, vs);
            }
        }

        double operator() (double g, double rQ) const noexcept
        {
            const double x = std::min (1.0, std::max (0.0, rQ)) * (gridSize - 1);
            const int i = std::min (gridSize - 2, int (x));
            const double t = x - i;
            return alongGain[size_t (i)] (g) * (1.0 - t) + alongGain[size_t (i + 1)] (g) * t;
        }
    };

    BellTable (const char* const* lines, int count, double baseFrequency, TaperId qTaperId, bool lmfLayout)
        : baseFreq (baseFrequency), qTaper (qTaperId)
    {
        const auto t = parseCsv (lines, count);
        const int cG = t.column ("gain_dB"), cQ = t.column ("Q"), cF = t.column ("freq_display_Hz");
        const int cF0 = t.column (lmfLayout ? "f0_eff_Hz" : "f0_Hz"), cQe = t.column ("Q_eff"),
                  cGe = t.column ("G_eff_dB");
        const int cDiv = lmfLayout ? t.column ("div3") : -1;
        for (const auto& r : t.rows)
        {
            Point p;
            p.gain = parseCell (r[size_t (cG)]);
            p.q = parseCell (r[size_t (cQ)]);
            p.freq = parseCell (r[size_t (cF)]);
            p.div3 = cDiv >= 0 && parseCell (r[size_t (cDiv)]) > 0.5;
            p.p = { parseCell (r[size_t (cF0)]), parseCell (r[size_t (cQe)]), parseCell (r[size_t (cGe)]) };
            pts.push_back (p);
        }
        auto find = [&] (double g, double q, double f, bool d3) -> const Point* {
            for (const auto& p : pts)
                if (std::abs (p.gain - g) < 1e-9 && std::abs (p.q - q) < 1e-9 && std::abs (p.freq - f) < 1e-9 && p.div3 == d3)
                    return &p;
            return nullptr;
        };

        // base series at Q 1.5, base frequency
        std::map<double, BellParams> base;
        for (const auto& p : pts)
            if (! p.div3 && std::abs (p.q - 1.5) < 1e-9 && std::abs (p.freq - baseFreq) < 1e-9)
                base[p.gain] = p.p;
        {
            std::vector<double> gs, vf, vq, vg;
            bool zeroDone = false;
            BellParams lastNeg {};
            for (const auto& [g, bp] : base)
            {
                if (! zeroDone && g > 0)
                {
                    gs.push_back (0.0);
                    vf.push_back (0.5 * (lastNeg.f0 + bp.f0));
                    vq.push_back (0.5 * (lastNeg.q + bp.q));
                    vg.push_back (0.0);
                    zeroDone = true;
                }
                gs.push_back (g);
                vf.push_back (bp.f0);
                vq.push_back (bp.q);
                vg.push_back (bp.gainDb);
                if (g < 0)
                    lastNeg = bp;
            }
            baseCurve[0].set (gs, vf);
            baseCurve[1].set (gs, vq);
            baseCurve[2].set (gs, vg);
        }

        // Q ratios at the base frequency
        std::map<double, std::map<double, BellParams>> byGain; // gain -> rQ -> params
        const auto& qt = Tapers::get()[qTaper];
        for (const auto& p : pts)
            if (! p.div3 && std::abs (p.freq - baseFreq) < 1e-9)
                byGain[p.gain][qt.rForValue (p.q)] = p.p;
        for (int k = 0; k < 3; ++k)
            qRatio[k] = {};
        for (const auto& [g, series] : byGain)
        {
            if (series.size() < 3)
                continue;
            const Point* ref = find (g, 1.5, baseFreq, false);
            if (ref == nullptr)
                continue;
            std::vector<double> rs, vf, vq, vg;
            for (const auto& [rq, bp] : series)
            {
                rs.push_back (rq);
                vf.push_back (bp.f0 / ref->p.f0);
                vq.push_back (bp.q / ref->p.q);
                vg.push_back (bp.gainDb / ref->p.gainDb);
            }
            qRatio[0].gains.push_back (g);
            qRatio[0].alongQ.emplace_back (rs, vf);
            qRatio[1].gains.push_back (g);
            qRatio[1].alongQ.emplace_back (rs, vq);
            qRatio[2].gains.push_back (g);
            qRatio[2].alongQ.emplace_back (rs, vg);
        }

        // frequency series at -12 / +12, Q 1.5
        for (int sgn = 0; sgn < 2; ++sgn)
        {
            const double g = sgn == 0 ? -12.0 : 12.0;
            std::map<double, BellParams> series;
            for (const auto& p : pts)
                if (! p.div3 && std::abs (p.q - 1.5) < 1e-9 && std::abs (p.gain - g) < 1e-9)
                    series[p.freq] = p.p;
            std::vector<double> lfs, vf, vq, vg;
            for (const auto& [f, bp] : series)
            {
                lfs.push_back (std::log (f));
                vf.push_back (bp.f0);
                vq.push_back (bp.q);
                vg.push_back (bp.gainDb);
            }
            auto& dst = sgn == 0 ? freqNeg : freqPos;
            dst[0].set (lfs, vf);
            dst[1].set (lfs, vq);
            dst[2].set (lfs, vg);
        }
        for (auto& qr : qRatio)
            qr.build();

        // /3 points (LMF only)
        std::vector<const Point*> d3;
        for (const auto& p : pts)
            if (p.div3)
                d3.push_back (&p);
        if (d3.size() >= 2)
        {
            hasDiv3 = true;
            for (size_t k = 0; k < 2; ++k)
            {
                const Point* p = d3[k];
                const Point* ref = find (p->gain, p->q, p->freq, false);
                div3LogF[k] = std::log (p->freq);
                div3LogF0[k] = std::log (p->p.f0);
                div3QCorr[k] = ref != nullptr ? p->p.q / ref->p.q : 1.0;
                div3GCorr[k] = ref != nullptr ? p->p.gainDb / ref->p.gainDb : 1.0;
            }
        }
    }

    std::vector<Point> pts;
    double baseFreq;
    TaperId qTaper;
    std::array<Pchip, 3> baseCurve;
    std::array<QRatio, 3> qRatio;
    std::array<Pchip, 3> freqNeg, freqPos;
    bool hasDiv3 = false;
    double div3LogF[2] {}, div3LogF0[2] {}, div3QCorr[2] { 1, 1 }, div3GCorr[2] { 1, 1 };
};

} // namespace craft::consoleg::dsp
