// Craft Console G - spec section 2: host value r in [0,1] -> displayed value, by the 41-point piecewise-linear
// tables of tables/param_tapers.csv (embedded at build time).
#pragma once

#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

#include "Csv.h"
#include "console_g/SpecTables.h"

namespace craft::consoleg::dsp
{

enum class TaperId
{
    compThreshold, compRatio, compRelease, expRelease, expThreshold, expRange, hpFreq, lpFreq, lfGain, lfFreq,
    lmfGain, lmfFreq, lmfQ, hmfGain, hmfFreq, hmfQ, hfGain, hfFreq, outputGain, inputTrim, count
};

class Taper
{
public:
    static constexpr int points = 41;

    std::string name;
    std::array<double, points> value {};  // NaN = "Out" (filter off), +inf for the ratio end point
    std::array<int, points> decimals {};
    std::string unit;                     // "dB", "s", "Hz", "kHz" or empty
    bool kiloDisplay = false;             // shown in kHz (values are stored in Hz)

    // Displayed value for host value r (spec section 2). Off positions return NaN.
    double valueAt (double r) const noexcept
    {
        r = std::min (1.0, std::max (0.0, r));
        const double x = r * (points - 1);
        int i = int (std::floor (x));
        if (i >= points - 1)
            i = points - 2;
        const double t = x - i;
        const double v0 = value[size_t (i)], v1 = value[size_t (i + 1)];
        if (std::isnan (v0) || std::isnan (v1))
        {
            // Only the r = 0 end point is "Off": the lower half of the first segment is off, the upper half snaps
            // to the first on-value.
            if (std::isnan (v0) && ! std::isnan (v1))
                return t < 0.5 ? v0 : v1;
            return std::isnan (v1) ? v0 : v1;
        }
        if (std::isinf (v0) || std::isinf (v1))
        {
            // ratio end point: linear in 1/R, i.e. in 1 - 1/R (the compressor's own interpolation variable)
            const double i0 = std::isinf (v0) ? 0.0 : 1.0 / v0, i1 = std::isinf (v1) ? 0.0 : 1.0 / v1;
            const double inv = i0 + t * (i1 - i0);
            return inv <= 0.0 ? std::numeric_limits<double>::infinity() : 1.0 / inv;
        }
        return v0 + t * (v1 - v0);
    }

    bool isOff (double r) const noexcept { return std::isnan (valueAt (r)); }

    // Display text, rounded as in the table (decimals of the nearest table point).
    std::string format (double r, bool withUnit = true) const
    {
        const double v = valueAt (r);
        if (std::isnan (v))
            return "Off";
        if (std::isinf (v))
            return "+Inf";
        const int k = int (std::lround (std::min (1.0, std::max (0.0, r)) * (points - 1)));
        int dec = decimals[size_t (k)];
        double shown = kiloDisplay ? v / 1000.0 : v;
        char buf[64];
        const double q = std::pow (10.0, dec);
        shown = std::round (shown * q) / q;
        if (shown == 0.0)
            shown = 0.0; // no "-0.0"
        std::snprintf (buf, sizeof (buf), "%.*f", dec, shown);
        std::string s (buf);
        if (withUnit && ! unit.empty())
            s += " " + unit;
        return s;
    }

    // Inverse mapping for typed values: the smallest r whose value equals v (clamped to the range).
    double rForValue (double v) const noexcept
    {
        if (std::isnan (v))
            return 0.0;
        double best = 0.0, bestErr = std::numeric_limits<double>::infinity();
        for (int i = 0; i + 1 < points; ++i)
        {
            const double v0 = value[size_t (i)], v1 = value[size_t (i + 1)];
            if (std::isnan (v0) || std::isnan (v1))
                continue;
            double r;
            if (std::isinf (v0) || std::isinf (v1))
            {
                const double i0 = std::isinf (v0) ? 0.0 : 1.0 / v0, i1 = std::isinf (v1) ? 0.0 : 1.0 / v1;
                const double iv = std::isinf (v) ? 0.0 : 1.0 / v;
                const double t = (i1 != i0) ? (iv - i0) / (i1 - i0) : 0.0;
                r = (i + std::min (1.0, std::max (0.0, t))) / double (points - 1);
            }
            else
            {
                const double t = (v1 != v0) ? (v - v0) / (v1 - v0) : 0.0;
                r = (i + std::min (1.0, std::max (0.0, t))) / double (points - 1);
            }
            const double err = std::abs (valueAt (r) - v);
            if (err < bestErr - 1.0e-12 || (std::isinf (v) && std::isinf (valueAt (r))))
            {
                bestErr = std::isinf (v) && std::isinf (valueAt (r)) ? 0.0 : err;
                best = r;
                if (bestErr == 0.0)
                    break;
            }
        }
        return best;
    }
};

class Tapers
{
public:
    static const Tapers& get()
    {
        static const Tapers instance;
        return instance;
    }

    const Taper& operator[] (TaperId id) const noexcept { return tapers[size_t (id)]; }

private:
    Tapers()
    {
        const char* names[] = { "comp_threshold", "comp_ratio", "comp_release", "exp_release", "exp_threshold",
                                "exp_range", "hp_freq", "lp_freq", "lf_gain", "lf_freq", "lmf_gain", "lmf_freq",
                                "lmf_q", "hmf_gain", "hmf_freq", "hmf_q", "hf_gain", "hf_freq", "output_gain",
                                "input_trim" };
        const auto table = parseCsv (spec_tables::param_tapers, spec_tables::param_tapers_count);
        for (size_t k = 0; k < size_t (TaperId::count); ++k)
        {
            Taper& t = tapers[k];
            t.name = names[k];
            bool found = false;
            for (const auto& row : table.rows)
            {
                if (row.empty() || row[0] != t.name)
                    continue;
                found = true;
                for (int i = 0; i < Taper::points && i + 1 < int (row.size()); ++i)
                {
                    const std::string& cell = row[size_t (i + 1)];
                    t.value[size_t (i)] = parseCell (cell);
                    t.decimals[size_t (i)] = cellDecimals (cell);
                    if (cell.find ("KHz") != std::string::npos)
                        t.kiloDisplay = true;
                    if (t.unit.empty())
                    {
                        if (cell.find ("KHz") != std::string::npos) t.unit = "kHz";
                        else if (cell.find ("Hz") != std::string::npos) t.unit = "Hz";
                        else if (cell.find ("dB") != std::string::npos) t.unit = "dB";
                        else if (cell.find (" s") != std::string::npos) t.unit = "s";
                    }
                }
            }
            if (! found)
                throw std::runtime_error ("console-g: missing taper " + t.name);
        }
    }

    std::array<Taper, size_t (TaperId::count)> tapers;
};

} // namespace craft::consoleg::dsp
