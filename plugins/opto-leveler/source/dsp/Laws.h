// Craft Opto Leveler - control laws from the spec tables (T0 tapers, T1 static curves, T2 gain law, section 9).
// Host independent; the tables are compiled in from the spec Markdown (generated SpecTables.h).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "SpecTables.h"

namespace craft::opto_leveler::dsp
{

enum class Mode
{
    Compress = 0,
    Limit = 1
};

namespace detail
{
    // Piecewise-linear interpolation through (xs[i], ys[i]), xs ascending; clamps outside the range.
    template <typename XS, typename YS>
    double interp (const XS& xs, const YS& ys, int n, double x)
    {
        if (x <= xs (0))
            return ys (0);
        if (x >= xs (n - 1))
            return ys (n - 1);
        int i = 0;
        while (i < n - 2 && x > xs (i + 1))
            ++i;
        const double x0 = xs (i), x1 = xs (i + 1);
        const double t = (x - x0) / (x1 - x0);
        return ys (i) + t * (ys (i + 1) - ys (i));
    }
} // namespace detail

// ---------------------------------------------------------------------------------------------
// T0: host taper (normalised r -> display units), piecewise linear through the 11 tabulated points.
struct Taper
{
    const spec::Table& table; // 1 row: "display", columns r = 0 ... 1

    int points() const { return table.numCols - 1; }
    double r (int i) const { return table.headerValue[i + 1]; }
    double display (int i) const { return table.at (0, i + 1); }

    double toDisplay (double norm) const
    {
        return detail::interp ([this] (int i) { return r (i); }, [this] (int i) { return display (i); }, points(),
                               std::clamp (norm, 0.0, 1.0));
    }
    double toNormalised (double disp) const
    {
        return detail::interp ([this] (int i) { return display (i); }, [this] (int i) { return r (i); }, points(),
                               std::clamp (disp, 0.0, 100.0));
    }
};

inline Taper gainTaper() { return { spec::T0 }; }
inline Taper peakReductionTaper() { return { spec::T0_1 }; }

// ---------------------------------------------------------------------------------------------
// T2: Gain display -> make-up gain in dB (path gain at -60 dBFS / 1 kHz relative to the PR 0 path).
inline double gainLawDb (double display)
{
    const auto& t = spec::T2;
    return detail::interp ([&] (int i) { return t.headerValue[i + 1]; }, [&] (int i) { return t.at (0, i + 1); },
                           t.numCols - 1, std::clamp (display, 0.0, 100.0));
}

// Inverse of the gain law (dB -> display), used by Auto Makeup.
inline double gainLawInverse (double db)
{
    const auto& t = spec::T2;
    return detail::interp ([&] (int i) { return t.at (0, i + 1); }, [&] (int i) { return t.headerValue[i + 1]; },
                           t.numCols - 1, db);
}

// ---------------------------------------------------------------------------------------------
// T1C / T1L: static gain reduction (dB) vs detector level (dBFS, sine-peak calibrated) and Peak Reduction.
// Linear in level between the columns, linear in PR between the rows, 0 below the lowest tabulated row.
// Above 0 dBFS the last segment is extended (the hardware keeps compressing above the tabulated range).
class StaticCurve
{
public:
    static constexpr int maxCols = 16;
    static constexpr double maxLevelDb = 24.0; // extrapolation limit above the table
    static constexpr double maxGrDb = 45.0;

    explicit StaticCurve (const spec::Table& t) : table (t) {}

    double minLevel() const { return table.headerValue[1]; }
    double levelAt (int c) const { return table.headerValue[c + 1]; }
    int numLevels() const { return table.numCols - 1; }
    int numRows() const { return table.numRows; }
    double prAt (int row) const { return table.at (row, 0); }
    double grAt (int row, int c) const { return table.at (row, c + 1); }

    // GR of one table row at level (dBFS).
    double rowGr (int row, double levelDb) const
    {
        const int n = numLevels();
        if (levelDb <= levelAt (0))
            return grAt (row, 0);
        if (levelDb >= levelAt (n - 1))
        {
            const double slope = (grAt (row, n - 1) - grAt (row, n - 2)) / (levelAt (n - 1) - levelAt (n - 2));
            return grAt (row, n - 1) + std::max (0.0, slope) * (std::min (levelDb, maxLevelDb) - levelAt (n - 1));
        }
        // uniform column grid (4 dB), but do not rely on it
        int c = int ((levelDb - levelAt (0)) / (levelAt (1) - levelAt (0)));
        c = std::clamp (c, 0, n - 2);
        while (c > 0 && levelDb < levelAt (c))
            --c;
        while (c < n - 2 && levelDb > levelAt (c + 1))
            ++c;
        const double t = (levelDb - levelAt (c)) / (levelAt (c + 1) - levelAt (c));
        return grAt (row, c) + t * (grAt (row, c + 1) - grAt (row, c));
    }

    double gainReductionDb (double peakReduction, double levelDb) const
    {
        const int rows = numRows();
        if (peakReduction < prAt (0))
            return 0.0;
        if (peakReduction >= prAt (rows - 1))
            return std::min (maxGrDb, rowGr (rows - 1, levelDb));
        int r = 0;
        while (r < rows - 2 && peakReduction > prAt (r + 1))
            ++r;
        const double t = (peakReduction - prAt (r)) / (prAt (r + 1) - prAt (r));
        const double a = rowGr (r, levelDb), b = rowGr (r + 1, levelDb);
        return std::min (maxGrDb, a + t * (b - a));
    }

private:
    const spec::Table& table;
};

inline const StaticCurve& staticCurve (Mode m)
{
    static const StaticCurve compress (spec::T1C), limit (spec::T1L);
    return m == Mode::Limit ? limit : compress;
}

// ---------------------------------------------------------------------------------------------
// Section 9 Auto Makeup: make-up added for a Peak Reduction setting (dB). Measured relative to PR 40:
// +3.7 / +6.9 / +9.6 dB at 50 / 60 / 70. Below 40 no compensation; above 70 extended with the 60 -> 70 slope
// flattening like the static curves (assumption, see IMPLEMENTATION_NOTES).
inline double autoMakeupDb (double peakReduction)
{
    static constexpr double pr[] = { 0.0, 40.0, 50.0, 60.0, 70.0, 80.0, 100.0 };
    static constexpr double db[] = { 0.0, 0.0, 3.7, 6.9, 9.6, 12.0, 15.5 };
    return detail::interp ([] (int i) { return pr[i]; }, [] (int i) { return db[i]; }, 7, peakReduction);
}

// ---------------------------------------------------------------------------------------------
// Display strings / parsing
inline std::string formatFixed (double v, int decimals, const char* suffix = "")
{
    char buf[48];
    const double eps = 0.5 * std::pow (10.0, -decimals);
    if (std::abs (v) < eps)
        v = 0.0;
    std::snprintf (buf, sizeof buf, "%.*f%s", decimals, v, suffix);
    return buf;
}

inline double parseNumber (std::string text, double fallback)
{
    std::replace (text.begin(), text.end(), ',', '.');
    const char* s = text.c_str();
    while (*s == ' ' || *s == '\t')
        ++s;
    char* end = nullptr;
    const double v = std::strtod (s, &end);
    return end == s ? fallback : v;
}

inline double snapTo (double v, double step) { return std::round (v / step) * step + 0.0; }

} // namespace craft::opto_leveler::dsp
