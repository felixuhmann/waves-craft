// Craft FET Limiter - the normative spec r1 tables, parsed from the embedded CSV (generated SpecTables.h).
// Host independent. Parsing happens once (thread-safe static), never on the audio thread after prepare().
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "fet_limiter/SpecTables.h"

namespace craft::fet_limiter::dsp
{

enum class Revision
{
    B = 0, // "early" voicing  (host value r < 0.5)
    A = 1  // "late, low-noise" voicing (default)
};

enum class Ratio
{
    R20 = 0,
    R12 = 1,
    R8 = 2,
    R4 = 3,
    All = 4
};

inline int revIndex (Revision r) noexcept { return r == Revision::A ? 0 : 1; }
inline const char* ratioName (Ratio r) noexcept
{
    static const char* names[] = { "20", "12", "8", "4", "All" };
    return names[int (r)];
}

// Piecewise-linear function through sorted points, clamped outside.
struct Curve
{
    std::vector<double> x, y;

    double operator() (double v) const noexcept
    {
        if (x.empty())
            return 0.0;
        if (v <= x.front())
            return y.front();
        if (v >= x.back())
            return y.back();
        const auto it = std::upper_bound (x.begin(), x.end(), v);
        const auto i = size_t (it - x.begin()) - 1;
        const double t = (v - x[i]) / (x[i + 1] - x[i]);
        return y[i] + t * (y[i + 1] - y[i]);
    }

    // inverse of a monotonically increasing curve (clamped)
    double inverse (double v) const noexcept
    {
        if (x.empty())
            return 0.0;
        if (v <= y.front())
            return x.front();
        if (v >= y.back())
            return x.back();
        for (size_t i = 0; i + 1 < y.size(); ++i)
            if (v <= y[i + 1])
            {
                const double d = y[i + 1] - y[i];
                return d > 0.0 ? x[i] + (v - y[i]) / d * (x[i + 1] - x[i]) : x[i + 1];
            }
        return x.back();
    }
};

inline std::vector<std::string> splitCsv (const char* line)
{
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (const char* p = line; *p != 0; ++p)
    {
        if (*p == '"')
            quoted = ! quoted;
        else if (*p == ',' && ! quoted)
        {
            out.push_back (cur);
            cur.clear();
        }
        else
            cur += *p;
    }
    out.push_back (cur);
    return out;
}

inline double parseValue (const std::string& s)
{
    if (s == "-inf")
        return -std::numeric_limits<double>::infinity();
    if (s.empty())
        return std::numeric_limits<double>::quiet_NaN();
    return std::strtod (s.c_str(), nullptr);
}

// A parsed CSV: header + rows of strings.
struct CsvTable
{
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;

    int column (const std::string& name) const
    {
        for (size_t i = 0; i < header.size(); ++i)
            if (header[i] == name)
                return int (i);
        return -1;
    }
};

template <size_t N>
CsvTable parseCsv (const char* const (&lines)[N])
{
    CsvTable t;
    t.header = splitCsv (lines[0]);
    for (size_t i = 1; i < N; ++i)
        t.rows.push_back (splitCsv (lines[i]));
    return t;
}

struct SpecTables
{
    // Gain staging (spec 4), per revision index (0 = A, 1 = B): display dB -> gain dB. The input law's first row
    // is -inf at display -60.
    Curve gainIn[2], gainOut[2];
    // Settled static gain change GR(x) <= 0 (dB) per revision index and ratio (spec 5), All including its makeup.
    Curve staticGr[2][5];
    CsvTable timing, trajectories, freqResponse, harmonics, noise;

    SpecTables()
    {
        using namespace spec_tables;
        auto gainLaw = [] (const CsvTable& t, Curve* dst) {
            for (const auto& r : t.rows)
            {
                const int rev = r[0] == "A" ? 0 : 1;
                dst[rev].x.push_back (parseValue (r[1]));
                dst[rev].y.push_back (parseValue (r[2]));
            }
        };
        gainLaw (parseCsv (gainInputCsv), gainIn);
        gainLaw (parseCsv (gainOutputCsv), gainOut);

        // Static law: "E rows (settled, 4 s steps) take precedence over A rows". Where a curve has E rows, its
        // settled shape is defined by them; A rows (fast-release staircase) are used outside the E rows' x range
        // and for curves without E rows (see IMPLEMENTATION_NOTES, assumption S1).
        const auto st = parseCsv (staticGrCsv);
        std::map<double, double> e[2][5], a[2][5];
        for (const auto& r : st.rows)
        {
            const int rev = r[0] == "A" ? 0 : 1;
            int ratio = 4;
            if (r[1] == "20") ratio = 0;
            else if (r[1] == "12") ratio = 1;
            else if (r[1] == "8") ratio = 2;
            else if (r[1] == "4") ratio = 3;
            const double x = parseValue (r[2]), g = parseValue (r[3]);
            (r[4] == "E" ? e : a)[rev][ratio][x] = g;
        }
        for (int rev = 0; rev < 2; ++rev)
            for (int ratio = 0; ratio < 5; ++ratio)
            {
                std::map<double, double> pts = e[rev][ratio];
                const bool hasE = ! pts.empty();
                const double lo = hasE ? pts.begin()->first : 0.0, hi = hasE ? pts.rbegin()->first : 0.0;
                for (const auto& [x, g] : a[rev][ratio])
                    if (! hasE || x < lo || x > hi)
                        pts[x] = g;
                for (const auto& [x, g] : pts)
                {
                    staticGr[rev][ratio].x.push_back (x);
                    staticGr[rev][ratio].y.push_back (g);
                }
            }

        timing = parseCsv (timingCsv);
        trajectories = parseCsv (releaseTrajectoriesCsv);
        freqResponse = parseCsv (freqResponseCsv);
        harmonics = parseCsv (harmonicsCsv);
        noise = parseCsv (noiseCsv);
    }
};

inline const SpecTables& specTables()
{
    static const SpecTables t;
    return t;
}

} // namespace craft::fet_limiter::dsp
