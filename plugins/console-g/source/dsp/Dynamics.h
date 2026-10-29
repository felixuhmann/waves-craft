// Craft Console G - spec sections 5 and 6: linked compressor and expander / gate sharing one gain element.
#pragma once

#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "Csv.h"
#include "MathUtil.h"
#include "console_g/SpecTables.h"

namespace craft::consoleg::dsp
{

// ---------------------------------------------------------------------------------------------
// Static compressor law: tables/comp_static.csv (output gain incl. make-up vs input level, 1 kHz sine, default
// timing), interpolated linearly in threshold and in u = 1 - 1/ratio (spec 5); ratio 1 (u = 0) is unity gain.
class CompressorTable
{
public:
    static constexpr int numLevels = 28; // -54 ... 0 dBFS in 2 dB steps
    static constexpr double firstLevel = -54.0, levelStep = 2.0;

    static const CompressorTable& get()
    {
        static const CompressorTable t;
        return t;
    }

    using Curve = std::array<double, numLevels>;

    static double uOf (double ratio) noexcept { return std::isinf (ratio) ? 1.0 : std::max (0.0, 1.0 - 1.0 / ratio); }

    // Curve for display threshold T (dB) and ratio R.
    Curve curve (double T, double ratio) const noexcept
    {
        Curve out {};
        const double u = uOf (ratio);
        if (u <= 0.0)
            return out;
        const double tc = std::min (thresholds.back(), std::max (thresholds.front(), T));
        const size_t ti = segmentIndex (thresholds, tc);
        const double tt = (tc - thresholds[ti]) / (thresholds[ti + 1] - thresholds[ti]);
        const size_t ui = segmentIndex (us, u);
        const double tu = std::min (1.0, std::max (0.0, (u - us[ui]) / (us[ui + 1] - us[ui])));
        for (int k = 0; k < numLevels; ++k)
        {
            const double g00 = at (ti, ui, k), g01 = at (ti, ui + 1, k), g10 = at (ti + 1, ui, k),
                         g11 = at (ti + 1, ui + 1, k);
            const double g0 = g00 + tu * (g01 - g00), g1 = g10 + tu * (g11 - g10);
            out[size_t (k)] = g0 + tt * (g1 - g0);
        }
        return out;
    }

    // Evaluates a curve at an input level (dBFS): constant below -54, linear extrapolation of the last slope above 0.
    static double evaluate (const Curve& c, double levelDb) noexcept
    {
        const double x = (levelDb - firstLevel) / levelStep;
        if (x <= 0.0)
            return c[0];
        if (x >= numLevels - 1)
            return c[numLevels - 1] + (x - (numLevels - 1)) * (c[numLevels - 1] - c[numLevels - 2]);
        const int i = int (x);
        const double t = x - i;
        return c[size_t (i)] + t * (c[size_t (i + 1)] - c[size_t (i)]);
    }

    const std::vector<double>& thresholdGrid() const noexcept { return thresholds; }
    const std::vector<double>& ratioGrid() const noexcept { return ratios; }
    double tableValue (size_t ti, size_t ri, int k) const noexcept { return at (ti, ri + 1, k); }

private:
    CompressorTable()
    {
        const auto t = parseCsv (spec_tables::comp_static, spec_tables::comp_static_count);
        std::map<double, std::map<double, Curve>> m;
        std::map<double, double> ratioOfU;
        for (const auto& r : t.rows)
        {
            const double T = parseCell (r[0]), R = parseCell (r[1]);
            Curve c {};
            for (int k = 0; k < numLevels; ++k)
                c[size_t (k)] = parseCell (r[size_t (k + 2)]);
            m[T][uOf (R)] = c;
            ratioOfU[uOf (R)] = R;
        }
        for (const auto& [T, _] : m)
            thresholds.push_back (T);
        us.push_back (0.0);
        for (const auto& [u, R] : ratioOfU)
        {
            us.push_back (u);
            ratios.push_back (R);
        }
        for (const auto& [T, byU] : m)
        {
            std::vector<Curve> row;
            row.push_back (Curve {}); // ratio 1
            for (const auto& [u, c] : byU)
                row.push_back (c);
            data.push_back (row);
        }
    }

    double at (size_t ti, size_t ui, int k) const noexcept { return data[ti][ui][size_t (k)]; }

    std::vector<double> thresholds, us, ratios;
    std::vector<std::vector<Curve>> data; // [threshold][u][level]
};

// ---------------------------------------------------------------------------------------------
// Time-constant laws. The per-setting fit of tables/comp_model_fit.csv gives one-pole attack / release constants
// for release 0.4 s (T 0/-10/-20, R 2/4/inf), which this implementation interpolates bilinearly in T and
// u = 1 - 1/R ("auto" attack is program / setting dependent: slower for deeper reduction). The release control
// scales the release constant along the measured 0.1 / 0.4 / 1.2 s points (spec 5 timing table) and beyond.
class CompressorTiming
{
public:
    static const CompressorTiming& get()
    {
        static const CompressorTiming t;
        return t;
    }

    double autoAttackMs (double T, double ratio) const noexcept { return bilinear (attack, T, ratio); }
    double releaseMs04 (double T, double ratio) const noexcept { return bilinear (release, T, ratio); }

private:
    CompressorTiming()
    {
        const auto t = parseCsv (spec_tables::comp_model_fit, spec_tables::comp_model_fit_count);
        const int cT = t.column ("threshold_dB"), cR = t.column ("ratio"), cRel = t.column ("release_s"),
                  cF = t.column ("fast_attack"), cA = t.column ("attack_tau_ms"), cRt = t.column ("release_tau_ms");
        for (const auto& r : t.rows)
        {
            if (std::abs (parseCell (r[size_t (cRel)]) - 0.4) > 1e-9 || parseCell (r[size_t (cF)]) > 0.5)
                continue;
            const double T = parseCell (r[size_t (cT)]), u = CompressorTable::uOf (parseCell (r[size_t (cR)]));
            attack[T][u] = parseCell (r[size_t (cA)]);
            release[T][u] = parseCell (r[size_t (cRt)]);
        }
    }

    static double bilinear (const std::map<double, std::map<double, double>>& m, double T, double ratio) noexcept
    {
        std::vector<double> ts;
        for (const auto& kv : m)
            ts.push_back (kv.first);
        const double tc = std::min (ts.back(), std::max (ts.front(), T));
        const size_t i = segmentIndex (ts, tc);
        const double tt = (tc - ts[i]) / (ts[i + 1] - ts[i]);
        auto along = [&] (const std::map<double, double>& row) {
            std::vector<double> u, v;
            for (const auto& kv : row)
            {
                u.push_back (kv.first);
                v.push_back (kv.second);
            }
            return interpLinear (u, v, CompressorTable::uOf (ratio));
        };
        const double a = along (m.at (ts[i])), b = along (m.at (ts[i + 1]));
        return a + tt * (b - a);
    }

    std::map<double, std::map<double, double>> attack, release;
};

// ---------------------------------------------------------------------------------------------
// Expander / gate constants derived from tables/exp_static.csv at start-up (spec 6):
//  - per threshold T: the slope s(T) (dB per dB, ~1.2) and the onset level Lth(T) (dBFS) of the expansion,
//    least-squares fitted to the settled range-40 down-staircase points;
//  - expander floor: the attenuation state D (dB) is limited to a*range; the applied attenuation is
//    max(0, D - o) (soft floor), fitted to the settled floors (range 5 -> 0.79 dB ... 30 -> 35.2 dB);
//  - gate open / close levels per T: midpoints of the staircase steps where the gate opened (up) / closed (down);
//  - gate floor: -25.4 dB at range 20 (table) and -52.7 dB at range 40 (spec text), linear in dB vs range.
class ExpanderModel
{
public:
    static const ExpanderModel& get()
    {
        static const ExpanderModel m;
        return m;
    }

    double onsetDb (double T) const noexcept { return interpLinear (ts, onset, T); }
    // Onset for a given range: the slope region of ranges 20 / 30 lies slightly below the range-40 line
    // (exp_static: about 0.5 / 0.2 dB less attenuation); offsets fitted per threshold, linear in range, clamped.
    double onsetDb (double T, double range) const noexcept
    {
        const double base = onsetDb (T);
        const double o20 = interpLinear (ts, shift20, T), o30 = interpLinear (ts, shift30, T);
        static const std::vector<double> rs { 20.0, 30.0, 40.0 };
        return base + interpLinear (rs, std::vector<double> { o20, o30, 0.0 }, range);
    }
    double slope (double T) const noexcept { return interpLinear (ts, slopes, T); }
    double gateOpenDb (double T) const noexcept { return interpLinear (gateTs, gateOpen, T); }
    double gateCloseDb (double T) const noexcept { return interpLinear (gateTs, gateClose, T); }
    double expanderDepthMax (double range) const noexcept { return depthPerDb * std::max (0.0, range); }
    double expanderOffset() const noexcept { return offsetDb; }
    double gateFloorDb (double range) const noexcept
    {
        static const std::vector<double> r { 0.0, 20.0, 40.0 };
        const std::vector<double> f { 0.0, gateFloor20, gateFloor40 };
        if (range > 40.0)
            return gateFloor40 * range / 40.0;
        return interpLinear (r, f, std::max (0.0, range));
    }
    double gateOffset() const noexcept { return gateOffsetDb; }

private:
    ExpanderModel()
    {
        const auto t = parseCsv (spec_tables::exp_static, spec_tables::exp_static_count);
        std::vector<double> levels;
        for (size_t c = 4; c < t.header.size(); ++c)
            levels.push_back (std::strtod (t.header[c].c_str() + 2, nullptr)); // "in-54"
        struct Row
        {
            std::string mode, dir;
            double T, range;
            std::vector<double> g;
        };
        std::vector<Row> rows;
        for (const auto& r : t.rows)
        {
            Row row { r[0], r[3], parseCell (r[1]), parseCell (r[2]), {} };
            for (size_t c = 4; c < r.size(); ++c)
                row.g.push_back (parseCell (r[c]));
            rows.push_back (row);
        }

        // slope / onset (range 40, down)
        std::map<double, std::pair<double, double>> fit;
        for (const auto& row : rows)
        {
            if (row.mode != "expander" || row.dir != "down" || std::abs (row.range - 40.0) > 1e-9)
                continue;
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            int n = 0;
            for (size_t k = 0; k < levels.size(); ++k)
                if (row.g[k] < -1.0 && row.g[k] > -40.0)
                {
                    sx += levels[k];
                    sy += row.g[k];
                    sxx += levels[k] * levels[k];
                    sxy += levels[k] * row.g[k];
                    ++n;
                }
            const double s = (n * sxy - sx * sy) / (n * sxx - sx * sx); // g = s (L - Lth)
            const double b = (sy - s * sx) / n;
            fit[row.T] = { s, -b / s };
        }
        for (const auto& [T, sl] : fit)
        {
            ts.push_back (T);
            slopes.push_back (sl.first);
            onset.push_back (sl.second);
            // onset shift of ranges 20 / 30 with the same slope: mean residual over their settled slope points
            for (double range : { 20.0, 30.0 })
            {
                double acc = 0.0;
                int n = 0;
                for (const auto& row : rows)
                {
                    if (row.mode != "expander" || row.dir != "down" || row.T != T || std::abs (row.range - range) > 1e-9)
                        continue;
                    const double floorAtt = 1.375 * range - 6.1;
                    for (size_t k = 0; k < levels.size(); ++k)
                        if (row.g[k] < -1.0 && -row.g[k] < floorAtt - 4.0)
                        {
                            // g = s (L - Lth')  ->  Lth' = L - g / s
                            acc += (levels[k] - row.g[k] / sl.first) - sl.second;
                            ++n;
                        }
                }
                (range == 20.0 ? shift20 : shift30).push_back (n > 0 ? acc / n : 0.0);
            }
        }

        // floors: settled values (down staircase at -54 dBFS, highest threshold)
        double maxT = -1e9;
        for (const auto& row : rows)
            maxT = std::max (maxT, row.T);
        std::vector<std::pair<double, double>> floors; // range, attenuation
        for (const auto& row : rows)
            if (row.mode == "expander" && row.dir == "down" && std::abs (row.T - maxT) < 1e-9 && row.range >= 10.0
                && row.range <= 30.0)
                floors.push_back ({ row.range, -row.g[0] });
        {
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            const double n = double (floors.size());
            for (const auto& [r, a] : floors)
            {
                sx += r;
                sy += a;
                sxx += r * r;
                sxy += r * a;
            }
            depthPerDb = (n * sxy - sx * sy) / (n * sxx - sx * sx); // atten = a*range - o
            offsetDb = -(sy - depthPerDb * sx) / n;
        }

        // gate open / close
        std::map<double, double> openAt, closeAt;
        for (const auto& row : rows)
        {
            if (row.mode != "gate")
                continue;
            gateFloor20 = std::min (gateFloor20, *std::min_element (row.g.begin(), row.g.end()));
            if (row.dir == "up")
            {
                for (size_t k = 1; k < levels.size(); ++k)
                    if (row.g[k - 1] < -10.0 && row.g[k] > -0.5)
                    {
                        openAt[row.T] = 0.5 * (levels[k - 1] + levels[k]);
                        break;
                    }
            }
            else
            {
                // closes during the first step (descending) that shows attenuation
                for (size_t k = levels.size() - 1; k > 0; --k)
                    if (row.g[k] > -0.5 && row.g[k - 1] < -5.0)
                    {
                        closeAt[row.T] = 0.5 * (levels[k - 1] + levels[k]);
                        break;
                    }
            }
        }
        for (const auto& [T, o] : openAt)
        {
            gateTs.push_back (T);
            gateOpen.push_back (o);
            gateClose.push_back (closeAt.count (T) ? closeAt[T] : o - 3.0);
        }
        gateFloor20 = -gateFloor20;
        gateOffsetDb = offsetDb; // same depth offset as the expander (fits the gate post-silence steps, spec 9.2)
    }

    std::vector<double> ts, slopes, onset, shift20, shift30, gateTs, gateOpen, gateClose;
    double depthPerDb = 1.375, offsetDb = 6.1;
    double gateFloor20 = 0.0;
    static constexpr double gateFloor40 = 52.7; // spec 6: gate floor -52.7 dB at range 40
    double gateOffsetDb = 0.0;
};

} // namespace craft::consoleg::dsp
