// Craft Console G - spec section 3: high-pass (18 dB/oct, bilinear of fixed analog prototypes, tables/hp.csv)
// and low-pass (12 dB/oct, magnitude-matched to tables/lp_target.csv).
#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "Biquad.h"
#include "Csv.h"
#include "MathUtil.h"
#include "console_g/SpecTables.h"

namespace craft::consoleg::dsp
{

// ---------------------------------------------------------------------------------------------
// High-pass prototype: H(s) = B3 s^3 / ((s^2 + 2 zeta w s + w^2)(s + sigma)).
// The table denominators are factored into the complex pole pair (w, zeta) and the real pole sigma; between table
// rows (ln w, ln zeta, ln sigma) are interpolated linearly in log10 of the display value (spec 3.1). The numerator
// is B3 s^3: the spec states all zeros are at s = 0 (the tabulated B0..B2 are fit residue, |B_k s^k| << |s^3|).
struct HighPassPrototype
{
    double w = 0.0, zeta = 0.0, sigma = 0.0, gain = 1.0;

    // Analog response (for tests).
    std::complex<double> response (double f) const noexcept
    {
        const std::complex<double> s (0.0, 2.0 * pi * f);
        return gain * s * s * s / ((s * s + 2.0 * zeta * w * s + w * w) * (s + sigma));
    }

    // Digital sections (bilinear, no prewarp): second order and first order.
    void digitise (double fs, BiquadCoeffs& second, BiquadCoeffs& first) const noexcept
    {
        second = bilinear2 (gain, 0.0, 0.0, 1.0, 2.0 * zeta * w, w * w, fs);
        first = bilinear1 (1.0, 0.0, 1.0, sigma, fs);
    }
};

class HighPassTable
{
public:
    static const HighPassTable& get()
    {
        static const HighPassTable instance;
        return instance;
    }

    struct Row
    {
        double display, A0, A1, A2, B3;
        HighPassPrototype proto;
    };
    const std::vector<Row>& rows() const noexcept { return rowList; }

    HighPassPrototype prototype (double displayHz) const noexcept
    {
        const double x = std::log10 (std::max (displayHz, 1.0));
        HighPassPrototype p;
        p.w = std::exp (interpLinear (logDisplay, lnW, x));
        p.zeta = std::exp (interpLinear (logDisplay, lnZeta, x));
        p.sigma = std::exp (interpLinear (logDisplay, lnSigma, x));
        p.gain = interpLinear (logDisplay, gains, x);
        return p;
    }

    // Factor s^3 + A2 s^2 + A1 s + A0 = (s + sigma)(s^2 + 2 zeta w s + w^2).
    static HighPassPrototype factor (double A0, double A1, double A2, double B3) noexcept
    {
        // the real root lies in (-A2, 0) for a stable cubic: p(0) = A0 > 0, p(-A2) = A0 - A1 A2 < 0
        auto poly = [&] (double s) { return ((s + A2) * s + A1) * s + A0; };
        double lo = -A2, hi = 0.0;
        if (poly (lo) > 0.0)
            lo = -A2 - std::abs (A0) - std::abs (A1) - 1.0;
        for (int i = 0; i < 200; ++i)
        {
            const double mid = 0.5 * (lo + hi);
            (poly (mid) > 0.0 ? hi : lo) = mid;
        }
        const double sigma = -0.5 * (lo + hi);
        HighPassPrototype p;
        p.sigma = sigma;
        p.w = std::sqrt (A0 / sigma);
        p.zeta = (A2 - sigma) / (2.0 * p.w);
        p.gain = B3;
        return p;
    }

private:
    HighPassTable()
    {
        const auto t = parseCsv (spec_tables::hp, spec_tables::hp_count);
        const int cD = t.column ("freq_display_Hz"), cA0 = t.column ("A0"), cA1 = t.column ("A1"), cA2 = t.column ("A2"),
                  cB3 = t.column ("B3");
        for (const auto& r : t.rows)
        {
            Row row { parseCell (r[size_t (cD)]), parseCell (r[size_t (cA0)]), parseCell (r[size_t (cA1)]),
                      parseCell (r[size_t (cA2)]), parseCell (r[size_t (cB3)]), {} };
            row.proto = factor (row.A0, row.A1, row.A2, row.B3);
            rowList.push_back (row);
            logDisplay.push_back (std::log10 (row.display));
            lnW.push_back (std::log (row.proto.w));
            lnZeta.push_back (std::log (row.proto.zeta));
            lnSigma.push_back (std::log (row.proto.sigma));
            gains.push_back (row.B3);
        }
    }

    std::vector<Row> rowList;
    std::vector<double> logDisplay, lnW, lnZeta, lnSigma, gains;
};

// ---------------------------------------------------------------------------------------------
// Low-pass target (spec 3.2): magnitude in dB per host value r = k/40 at 1/6-octave frequencies; linear in r and
// in log f between table points. Above the last tabulated frequency the last slope (dB per log f) continues.
class LowPassTarget
{
public:
    static const LowPassTarget& get()
    {
        static const LowPassTarget instance;
        return instance;
    }

    const std::vector<double>& rowR() const noexcept { return rs; }
    const std::vector<double>& freqs() const noexcept { return fs; }

    double rowDb (size_t row, double f) const noexcept
    {
        const auto& d = db[row];
        const double x = std::log10 (std::max (f, 1.0));
        if (x <= logF.front())
            return d.front();
        const size_t n = logF.size();
        if (x >= logF[n - 1])
        {
            const double slope = (d[n - 1] - d[n - 2]) / (logF[n - 1] - logF[n - 2]);
            return d[n - 1] + slope * (x - logF[n - 1]);
        }
        return interpLinear (logF, d, x);
    }

    // Target for arbitrary r (r below the first row uses the first row).
    double targetDb (double r, double f) const noexcept
    {
        if (r <= rs.front())
            return rowDb (0, f);
        if (r >= rs.back())
            return rowDb (rs.size() - 1, f);
        const size_t i = segmentIndex (rs, r);
        const double t = (r - rs[i]) / (rs[i + 1] - rs[i]);
        return rowDb (i, f) + t * (rowDb (i + 1, f) - rowDb (i, f));
    }

private:
    LowPassTarget()
    {
        const auto t = parseCsv (spec_tables::lp_target, spec_tables::lp_target_count);
        for (size_t c = 1; c < t.header.size(); ++c)
        {
            std::string h = t.header[c];
            fs.push_back (std::strtod (h.c_str(), nullptr));
            logF.push_back (std::log10 (fs.back()));
        }
        for (const auto& r : t.rows)
        {
            rs.push_back (parseCell (r[0]));
            std::vector<double> row;
            for (size_t c = 1; c < r.size(); ++c)
                row.push_back (parseCell (r[c]));
            db.push_back (row);
        }
    }

    std::vector<double> rs, fs, logF;
    std::vector<std::vector<double>> db;
};

// Per-sample-rate realisation of the low-pass rows: one biquad per table row, fitted to the target magnitude
// (poles by a Nelder-Mead search, numerator by the linear magnitude fit of MagnitudeFit) over 20 Hz ...
// min(20 kHz, 0.45 fs). Arbitrary r interpolates the coefficients of the two neighbouring rows linearly (the
// stability triangle is convex, so the result is stable).
class LowPassDesigner
{
public:
    static constexpr int subdivisions = 4; // fitted points per table interval (coefficient interpolation in between)

    void prepare (double sampleRate)
    {
        if (sampleRate == fsPrepared && ! rowCoeffs.empty())
            return;
        fsPrepared = sampleRate;
        const auto& rs = LowPassTarget::get().rowR();
        denseR.clear();
        for (size_t k = 0; k + 1 < rs.size(); ++k)
            for (int j = 0; j < subdivisions; ++j)
                denseR.push_back (rs[k] + (rs[k + 1] - rs[k]) * j / subdivisions);
        denseR.push_back (rs.back());
        denseCoeffs.clear();
        rowCoeffs.clear();
        std::array<double, 2> start {};
        bool warm = false;
        for (size_t k = 0; k < denseR.size(); ++k)
        {
            denseCoeffs.push_back (fitRow (denseR[k], sampleRate, start, warm));
            warm = true;
            if (k % subdivisions == 0)
                rowCoeffs.push_back (denseCoeffs.back());
        }
    }

    BiquadCoeffs coeffs (double r) const noexcept
    {
        if (denseCoeffs.empty())
            return {};
        if (r <= denseR.front())
            return denseCoeffs.front();
        if (r >= denseR.back())
            return denseCoeffs.back();
        const size_t i = segmentIndex (denseR, r);
        return BiquadCoeffs::lerp (denseCoeffs[i], denseCoeffs[i + 1], (r - denseR[i]) / (denseR[i + 1] - denseR[i]));
    }

    const std::vector<BiquadCoeffs>& rows() const noexcept { return rowCoeffs; }

private:
    struct Grid
    {
        std::vector<double> w, mag, wt, f;
    };

    static Grid makeGrid (double r, double fs)
    {
        const auto& target = LowPassTarget::get();
        Grid g;
        const double fMax = std::min (20000.0, 0.45 * fs);
        const int n = 64;
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow (fMax / 20.0, double (i) / (n - 1));
            g.f.push_back (f);
            g.w.push_back (2.0 * pi * f / fs);
            g.mag.push_back (dbToGain (target.targetDb (r, f)));
            g.wt.push_back (f > 15000.0 ? 0.3 : 1.0);
        }
        return g;
    }

    // Poles from (log fc, log q) of an analog pair mapped with the bilinear transform (always stable).
    static void poles (double logFc, double logQ, double fs, double& a1, double& a2) noexcept
    {
        const double w = 2.0 * pi * std::exp (logFc), q = std::exp (logQ);
        const auto c = bilinear2 (0.0, 0.0, 1.0, 1.0, w / q, w * w, fs);
        a1 = c.a1;
        a2 = c.a2;
    }

    static double cost (const Grid& g, const BiquadCoeffs& c, double fs) noexcept
    {
        double e = 0.0;
        for (size_t i = 0; i < g.f.size(); ++i)
        {
            const double d = c.magnitudeDb (g.f[i], fs) - gainToDb (g.mag[i]);
            e += g.wt[i] * d * d;
        }
        return e;
    }

    static BiquadCoeffs design (const Grid& g, double logFc, double logQ, double fs) noexcept
    {
        double a1, a2;
        poles (logFc, logQ, fs, a1, a2);
        BiquadCoeffs c;
        if (! MagnitudeFit::fitNumerator (a1, a2, g.w.data(), g.mag.data(), g.wt.data(), int (g.w.size()), c))
            c = bilinear2 (0.0, 0.0, 1.0, 1.0, 2.0 * pi * std::exp (logFc) / std::exp (logQ),
                           std::pow (2.0 * pi * std::exp (logFc), 2.0), fs);
        return c;
    }

    // start: in/out (warm start from the previous, neighbouring r)
    static BiquadCoeffs fitRow (double r, double fs, std::array<double, 2>& start, bool warm)
    {
        const Grid g = makeGrid (r, fs);
        auto f = [&] (const std::array<double, 2>& p) { return cost (g, design (g, p[0], p[1], fs), fs); };

        std::array<double, 2> best = warm ? start : std::array<double, 2> { std::log (10000.0), std::log (0.7) };
        double bestCost = f (best);
        for (int i = 0; i <= 40; ++i)
            for (double q : { 0.5, 0.6, 0.7, 0.85 })
            {
                const std::array<double, 2> p { std::log (1500.0 * std::pow (60.0, i / 40.0)), std::log (q) };
                const double c = f (p);
                if (c < bestCost)
                {
                    bestCost = c;
                    best = p;
                }
            }

        // Nelder-Mead refinement
        const double step = 0.1;
        std::array<std::array<double, 2>, 3> s { best, std::array<double, 2> { best[0] + step, best[1] },
                                                 std::array<double, 2> { best[0], best[1] + step } };
        std::array<double, 3> v { f (s[0]), f (s[1]), f (s[2]) };
        for (int it = 0; it < 300; ++it)
        {
            // order
            for (int a = 0; a < 3; ++a)
                for (int b = a + 1; b < 3; ++b)
                    if (v[size_t (b)] < v[size_t (a)])
                    {
                        std::swap (v[size_t (a)], v[size_t (b)]);
                        std::swap (s[size_t (a)], s[size_t (b)]);
                    }
            const std::array<double, 2> c { 0.5 * (s[0][0] + s[1][0]), 0.5 * (s[0][1] + s[1][1]) };
            auto at = [&] (double k) {
                return std::array<double, 2> { c[0] + k * (s[2][0] - c[0]), c[1] + k * (s[2][1] - c[1]) };
            };
            const auto xr = at (-1.0);
            const double fr = f (xr);
            if (fr < v[0])
            {
                const auto xe = at (-2.0);
                const double fe = f (xe);
                if (fe < fr) { s[2] = xe; v[2] = fe; }
                else { s[2] = xr; v[2] = fr; }
            }
            else if (fr < v[1])
            {
                s[2] = xr;
                v[2] = fr;
            }
            else
            {
                const auto xc = at (fr < v[2] ? -0.5 : 0.5);
                const double fc = f (xc);
                if (fc < std::min (fr, v[2]))
                {
                    s[2] = xc;
                    v[2] = fc;
                }
                else
                {
                    for (int k = 1; k < 3; ++k)
                    {
                        s[size_t (k)] = { 0.5 * (s[0][0] + s[size_t (k)][0]), 0.5 * (s[0][1] + s[size_t (k)][1]) };
                        v[size_t (k)] = f (s[size_t (k)]);
                    }
                }
            }
            if (std::abs (v[2] - v[0]) < 1.0e-10)
                break;
        }
        size_t bi = 0;
        for (size_t k = 1; k < 3; ++k)
            if (v[k] < v[bi])
                bi = k;
        start = s[bi];
        return design (g, s[bi][0], s[bi][1], fs);
    }

    double fsPrepared = 0.0;
    std::vector<double> denseR;
    std::vector<BiquadCoeffs> denseCoeffs, rowCoeffs;
};

} // namespace craft::consoleg::dsp
