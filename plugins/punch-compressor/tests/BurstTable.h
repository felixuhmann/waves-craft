// Spec r2 section 7.5: the normative table and its stimulus (one continuous render per Release, rows in table order).
#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "TestHelpers.h"

namespace burst
{
using namespace testutil;

inline const std::vector<double> times { 0, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 3400 };

struct Row
{
    double releaseMs, burstMs, burstDb;
    std::vector<double> values;
};

inline std::vector<Row> loadTable (const std::string& path)
{
    std::vector<Row> rows;
    std::ifstream in (path);
    std::string line;
    std::getline (in, line);
    while (std::getline (in, line))
    {
        std::stringstream ss (line);
        std::string c;
        std::vector<double> v;
        while (std::getline (ss, c, ','))
            v.push_back (std::stod (c));
        if (v.size() == 15)
            rows.push_back ({ v[0], v[1], v[2], std::vector<double> (v.begin() + 3, v.end()) });
    }
    return rows;
}

inline std::vector<Row> rowsFor (const std::vector<Row>& all, double releaseMs)
{
    std::vector<Row> r;
    for (const auto& x : all)
        if (x.releaseMs == releaseMs)
            r.push_back (x);
    return r;
}

// -40 dBFS 1 s, then for each row: burst, 3.5 s at -40 dBFS. Returns the gains at `times` per row.
inline std::vector<std::vector<double>> run (const std::vector<Row>& rows, double releaseMs, double fs)
{
    std::vector<double> amp;
    std::vector<size_t> ends;
    amp.assign (size_t (fs), dbToLin (-40.0));
    for (const auto& r : rows)
    {
        amp.insert (amp.end(), size_t (std::lround (r.burstMs * 0.001 * fs)), dbToLin (r.burstDb));
        ends.push_back (amp.size());
        amp.insert (amp.end(), size_t (3.5 * fs), dbToLin (-40.0));
    }
    Trajectory t;
    t.fs = fs;
    t.x.resize (amp.size());
    for (size_t i = 0; i < amp.size(); ++i)
        t.x[i] = amp[i] * std::sin (2.0 * pi * 1000.0 * double (i) / fs);
    Settings s = trajectorySettings();
    s.releaseMs = releaseMs;
    t.y = runAligned (mono (t.x), fs, s, 4096).l;
    std::vector<std::vector<double>> out;
    for (size_t e : ends)
    {
        std::vector<double> g;
        for (double ms : times)
            g.push_back (t.gainAt (e, ms));
        out.push_back (g);
    }
    return out;
}

} // namespace burst
