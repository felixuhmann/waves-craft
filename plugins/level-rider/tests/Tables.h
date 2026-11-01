// Craft Level Rider - spec r2 dynamic tables T2, T3, T4 and T7 and their evaluation (section 11 tolerances).
// Shared by the unit tests and the constant-fitting tool.
#pragma once

#include <string>
#include <vector>

#include "TestHelpers.h"

namespace craft::levelrider::test
{

struct CellResult
{
    std::string label;
    double value, ref;
    bool pass;
    double score; // 0 .. 1 inside the tolerance, > 1 outside
};

// ---- T2 ----------------------------------------------------------------------------------------
struct T2Row
{
    const char* name;
    double minDb, maxDb;
    bool fast;
    double targetDb;
    double gain[18];
};

inline const T2Row t2Rows[7] = {
    { "+-12", -12, 12, false, -21, { -3.9, -11.8, -3.8, -8.4, -3.9, +1.2, -4.0, +6.6, +6.4, +1.1, -12.0, -8.5, -11.9, -12.0, -4.6, +11.9, -4.1, -4.0 } },
    { "default +-6", -6, 6, false, -21, { -2.4, -6.0, -2.3, -6.0, -2.4, +2.1, -2.4, +6.0, +6.0, +2.0, -6.0, -6.0, -6.0, -6.0, -3.0, +6.0, -2.5, -2.4 } },
    { "+-3", -3, 3, false, -21, { -1.3, -3.0, -1.2, -3.0, -1.3, +2.8, -1.3, +3.0, +3.0, +2.7, -3.0, -3.0, -3.0, -3.0, -0.6, +3.0, -1.4, -1.3 } },
    { "+-12 Fast", -12, 12, true, -21, { -5.6, -12.0, -5.5, -10.0, -5.6, -0.6, -5.7, +4.7, +4.4, -0.7, -12.0, -10.2, -12.0, -12.0, -6.9, +10.0, -5.8, -5.8 } },
    { "+-6 Fast", -6, 6, true, -21, { -3.8, -6.0, -3.7, -6.0, -3.8, +0.5, -3.8, +5.3, +5.0, +0.4, -6.0, -6.0, -6.0, -6.0, -4.8, +6.0, -3.9, -3.9 } },
    { "+-12, Target -12", -12, 12, false, -12, { +3.9, -6.1, +4.0, -1.5, +3.9, +9.6, +3.8, +12.0, +12.0, +9.4, -6.3, -1.7, -6.3, -10.2, +2.1, +12.0, +3.7, +3.8 } },
    { "+-12, Target -33", -12, 12, false, -33, { -11.9, -12.0, -11.8, -12.0, -11.9, -8.3, -12.0, -4.0, -4.2, -8.4, -12.0, -12.0, -12.0, -12.0, -9.9, +0.7, -12.0, -11.9 } },
};

// ---- T3 ----------------------------------------------------------------------------------------
inline const double t3Times[13] = { 0.01, 0.03, 0.06, 0.1, 0.2, 0.3, 0.5, 1, 1.5, 2, 3, 4, 6 };
inline const int t3Step[10] = { 1, 2, 3, 4, 5, 6, 7, 9, 15, 16 }; // T2 segment the step goes into

inline const double t3Slow[10][13] = {
    { -4.6, -5.9, -7.5, -8.6, -10.3, -11.1, -11.5, -11.8, -12.0, -12.0, -11.8, -11.9, -11.9 },
    { -11.9, -11.9, -11.9, -11.8, -11.8, -11.7, -11.6, -11.2, -10.5, -9.5, -6.2, -4.0, -4.0 },
    { -4.0, -4.7, -5.6, -6.1, -7.2, -7.6, -8.0, -8.3, -8.3, -8.3, -8.4, -8.2, -8.4 },
    { -8.3, -8.2, -8.2, -8.1, -7.8, -7.5, -6.9, -5.0, -4.1, -4.0, -3.9, -4.0, -3.8 },
    { -3.9, -3.9, -3.8, -3.7, -3.3, -3.0, -2.3, -0.1, +0.9, +1.1, +1.1, +1.1, +1.1 },
    { +1.0, +0.3, -0.5, -1.3, -2.3, -2.8, -3.6, -3.7, -3.9, -4.0, -3.9, -3.9, -4.0 },
    { -4.1, -4.0, -4.0, -4.0, -3.9, -3.8, -3.7, -3.0, -2.2, -0.8, +3.4, +6.4, +6.4 },
    { +6.0, +5.3, +4.6, +4.0, +2.8, +1.9, +1.7, +1.1, +1.1, +1.3, +1.1, +1.0, +1.1 },
    { -4.2, -4.1, -4.1, -4.1, -4.0, -3.8, -3.6, -2.7, -1.5, +0.2, +5.0, +10.4, +11.9 },
    { +10.9, +8.5, +5.5, +2.6, -0.9, -2.3, -3.3, -4.0, -3.9, -3.8, -3.8, -3.9, -3.9 } };

inline const double t3Fast[10][13] = {
    { -7.7, -10.1, -11.2, -11.8, -12.0, -12.0, -12.0, -12.0, -12.0, -12.0, -12.0, -12.0, -12.0 },
    { -12.0, -12.0, -12.0, -12.0, -12.0, -12.0, -12.0, -11.9, -11.7, -11.2, -9.4, -6.1, -5.8 },
    { -5.9, -7.1, -8.1, -8.7, -9.3, -9.4, -9.5, -9.8, -9.8, -9.9, -10.0, -9.6, -9.8 },
    { -9.7, -9.7, -9.6, -9.6, -9.4, -9.2, -8.8, -7.5, -6.1, -5.7, -5.8, -5.7, -5.6 },
    { -5.7, -5.7, -5.6, -5.5, -5.3, -5.1, -4.6, -3.0, -1.0, -0.8, -0.9, -0.7, -0.8 },
    { -1.5, -2.8, -3.9, -4.4, -4.9, -5.1, -5.6, -5.5, -5.6, -5.8, -5.7, -5.7, -5.7 },
    { -5.8, -5.8, -5.7, -5.7, -5.7, -5.7, -5.6, -5.4, -5.1, -4.5, -2.3, +1.5, +4.6 },
    { +3.4, +2.3, +1.3, +0.9, -0.0, -0.7, -0.6, -1.2, -1.1, -0.9, -0.8, -0.8, -0.8 },
    { -6.7, -6.7, -6.7, -6.7, -6.7, -6.7, -6.7, -6.6, -6.5, -6.3, -5.4, -3.3, +5.2 },
    { +6.0, +0.3, -2.7, -3.8, -4.9, -5.5, -5.9, -5.7, -5.7, -5.6, -5.6, -5.4, -5.7 } };

// ---- T4 ----------------------------------------------------------------------------------------
struct T4Row
{
    double idle;
    bool fast;
    double burst;
    double gain[10];
};
inline const double t4Times[10] = { 0.1, 0.25, 0.5, 0.75, 1, 1.5, 2, 2.5, 3, 4 };
inline const T4Row t4Rows[12] = {
    { 0, false, -9, { -11.91, -11.91, -11.91, -10.97, -9.92, -7.56, -4.89, -1.96, +0.00, +0.00 } },
    { 0, false, -33, { +6.69, +6.69, +6.69, +5.02, +3.35, +0.13, +0.00, +0.00, +0.00, +0.00 } },
    { 0, false, -21, { -3.70, -3.70, -3.70, -2.24, -0.70, +0.00, +0.00, +0.00, +0.00, +0.00 } },
    { 6, false, -9, { -11.91, -11.91, -11.91, -10.97, -9.92, -7.56, -4.89, -1.96, +1.16, +6.00 } },
    { 6, false, -33, { +6.69, +6.69, +6.69, +6.00, +6.00, +6.00, +6.00, +6.00, +6.00, +6.00 } },
    { 6, false, -21, { -3.70, -3.70, -3.70, -2.24, -0.70, +2.49, +5.81, +6.00, +6.00, +6.00 } },
    { -6, false, -9, { -11.91, -11.91, -11.91, -10.97, -9.92, -7.56, -6.00, -6.00, -6.00, -6.00 } },
    { -6, false, -33, { +6.69, +6.69, +6.69, +5.02, +3.35, +0.13, -2.94, -5.79, -6.00, -6.00 } },
    { -6, false, -21, { -3.70, -3.70, -3.70, -5.11, -6.00, -6.00, -6.00, -6.00, -6.00, -6.00 } },
    { 0, true, -9, { -11.83, -11.26, -10.24, -9.13, -7.93, -5.30, -2.41, +0.00, +0.00, +0.00 } },
    { 0, true, -33, { +4.37, +3.41, +1.78, +0.18, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00 } },
    { 0, true, -21, { -5.27, -4.43, -2.97, -1.47, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00 } },
};
// The bursts of one configuration (Idle value, Attack) are rendered in sequence -9, -33, -21 dBFS, each 12 s,
// each followed by 6 s of -80 dBFS band noise (the fall the table is measured from).
inline constexpr double t4Burst = 12.0, t4Gap = 6.0;

// ---- T7 ----------------------------------------------------------------------------------------
inline const double t7Times[12] = { 0.5, 1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 19.5 };
inline const double t7Upper[7] = { -15, -15, -9, -9, -9, -6, -3 };
inline const double t7Lower[7] = { -21, -27, -27, -33, -36, -36, -39 };
inline const double t7Slow[7][12] = {
    { -7.0, -5.1, -4.1, -3.8, -3.9, -3.8, -3.9, -3.9, -3.9, -3.9, -4.0, -3.9 },
    { -7.9, -7.3, -5.2, -1.2, +1.2, +1.2, +1.1, +1.2, +1.3, +1.0, +1.4, +1.2 },
    { -11.9, -11.9, -11.8, -11.6, -11.0, -9.4, -6.2, +0.9, +1.2, +1.3, +1.0, +0.9 },
    { -12.0, -10.0, -5.0, -2.6, -8.2, -8.2, -8.1, -6.3, +0.9, +6.4, +6.6, +6.7 },
    { -11.8, -9.7, -4.7, +0.0, -1.3, -5.4, -6.2, -5.8, -2.8, +6.5, +9.3, +9.2 },
    { -12.0, -10.0, -5.0, +0.0, +0.0, +0.0, +0.0, -5.7, -5.2, -2.0, +9.1, +9.4 },
    { -12.0, -10.0, -5.0, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, -3.8, -1.4, +11.9 } };
inline const double t7Fast[7][12] = {
    { -8.9, -7.6, -5.7, -5.4, -5.6, -5.3, -5.8, -5.7, -5.9, -5.5, -5.7, -5.3 },
    { -9.7, -9.5, -8.7, -6.7, -3.1, -0.6, -0.8, -0.6, -0.9, -0.8, -0.5, -0.7 },
    { -10.2, -9.5, -11.8, -11.7, -11.6, -11.1, -10.0, -3.6, -0.7, -0.6, -0.8, -1.1 },
    { -10.2, -7.9, -2.4, +0.0, +0.0, -5.5, -7.8, -7.4, -4.3, +4.0, +4.8, +4.5 },
    { -10.2, -7.9, -2.4, +0.0, +0.0, +0.0, -0.1, -5.6, -4.7, +0.6, +7.2, +7.1 },
    { -10.2, -7.9, -2.4, +0.0, +0.0, +0.0, +0.0, -2.5, -6.0, -5.0, +4.7, +7.4 },
    { -10.2, -7.9, -2.4, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, -3.9, -2.6, +10.2 } };

// ---- evaluation --------------------------------------------------------------------------------
inline RiderEngine makeTunedEngine (double fs, double minDb, double maxDb, bool fast, double targetDb, double idleDb,
                                    const Tuning* tuning)
{
    RiderEngine e;
    if (tuning != nullptr)
        e.setTuning (*tuning);
    Settings s;
    s.rangeMinDb = minDb;
    s.rangeMaxDb = maxDb;
    s.fast = fast;
    s.targetDb = targetDb;
    s.idleDb = idleDb;
    e.setSettings (s);
    e.prepare (fs);
    return e;
}

// Trajectory rule of T3 / T7: within tolDb, or the trajectory reaches the value within +-30 % of the time.
inline CellResult trajectoryCell (const GainTrace& tr, double t0, double t, double ref, std::string label, double tolDb = 1.5)
{
    const double v = tr.at (t0 + t);
    const double err = std::abs (v - ref);
    if (err <= tolDb)
        return { std::move (label), v, ref, true, (err / tolDb) * (err / tolDb) };
    double lo = 1e9, hi = -1e9;
    for (double tt = t0 + 0.7 * t; tt <= t0 + 1.3 * t + 1e-9; tt += tr.windowSeconds)
    {
        lo = std::min (lo, tr.at (tt));
        hi = std::max (hi, tr.at (tt));
    }
    if (ref >= lo && ref <= hi)
        return { std::move (label), v, ref, true, 1.0 };
    const double miss = std::min (std::abs (ref - lo), std::abs (ref - hi));
    return { std::move (label), v, ref, false, 1.0 + (miss / tolDb) * (miss / tolDb) * 4.0 };
}

inline GainTrace runT2 (double fs, size_t row, const Tuning* tuning)
{
    const auto& r = t2Rows[row];
    auto e = makeTunedEngine (fs, r.minDb, r.maxDb, r.fast, r.targetDb, 0.0, tuning);
    std::vector<Segment> segs;
    for (double l : t2Levels())
        segs.push_back ({ l, 12.0 });
    return runSegments (e, segs);
}

inline void evalT2 (const GainTrace& tr, size_t row, std::vector<CellResult>& out)
{
    for (size_t i = 0; i < 18; ++i)
    {
        const double end = tr.segmentEnd[i];
        const double g = tr.mean (end - 0.3, end), ref = t2Rows[row].gain[i];
        const double err = std::abs (g - ref);
        out.push_back ({ "T2 " + std::string (t2Rows[row].name) + " seg " + std::to_string (i + 1), g, ref, err <= 0.6,
                         err <= 0.6 ? (err / 0.6) * (err / 0.6) : 1.0 + 4.0 * ((err - 0.6) / 0.6) * ((err - 0.6) / 0.6) });
    }
}

inline void evalT3 (const GainTrace& tr, bool fast, std::vector<CellResult>& out)
{
    const auto& table = fast ? t3Fast : t3Slow;
    for (size_t s = 0; s < 10; ++s)
    {
        const double t0 = tr.segmentEnd[size_t (t3Step[s] - 1)];
        for (size_t j = 0; j < 13; ++j)
            out.push_back (trajectoryCell (tr, t0, t3Times[j], table[s][j],
                                           std::string ("T3 ") + (fast ? "Fast " : "Slow ")
                                               + std::to_string (int (t2Levels()[size_t (t3Step[s] - 1)])) + "->"
                                               + std::to_string (int (t2Levels()[size_t (t3Step[s])])) + " @"
                                               + std::to_string (t3Times[j])));
    }
}

inline void evalT7 (double fs, bool fast, const Tuning* tuning, std::vector<CellResult>& out)
{
    auto e = makeTunedEngine (fs, -12, 12, fast, -21, 0.0, tuning);
    std::vector<Segment> segs;
    for (size_t p = 0; p < 7; ++p)
    {
        segs.push_back ({ t7Upper[p], 12.0 });
        segs.push_back ({ t7Lower[p], 20.0 });
    }
    const auto tr = runSegments (e, segs);
    const auto& table = fast ? t7Fast : t7Slow;
    for (size_t p = 0; p < 7; ++p)
    {
        const double t0 = tr.segmentEnd[2 * p];
        for (size_t j = 0; j < 12; ++j)
            out.push_back (trajectoryCell (tr, t0, t7Times[j], table[p][j],
                                           std::string ("T7 ") + (fast ? "Fast " : "Slow ")
                                               + std::to_string (int (t7Upper[p])) + "->" + std::to_string (int (t7Lower[p]))
                                               + " @" + std::to_string (t7Times[j])));
    }
}

// T4: one run per (Idle value, Attack) with the three bursts in sequence.
inline void evalT4 (double fs, const Tuning* tuning, std::vector<CellResult>& out, bool fastOnly = false, bool slowOnly = false)
{
    for (size_t first = 0; first < 12; first += 3)
    {
        const bool fast = t4Rows[first].fast;
        if ((fastOnly && ! fast) || (slowOnly && fast))
            continue;
        auto e = makeTunedEngine (fs, -12, 12, fast, -21, t4Rows[first].idle, tuning);
        std::vector<Segment> segs;
        for (size_t k = 0; k < 3; ++k)
        {
            segs.push_back ({ t4Rows[first + k].burst, t4Burst });
            segs.push_back ({ -80.0, t4Gap });
        }
        const auto tr = runSegments (e, segs);
        for (size_t k = 0; k < 3; ++k)
        {
            const auto& row = t4Rows[first + k];
            const double t0 = tr.segmentEnd[2 * k];
            for (size_t j = 0; j < 10; ++j)
            {
                const double v = tr.at (t0 + t4Times[j]), err = std::abs (v - row.gain[j]);
                out.push_back ({ "T4 Idle " + std::to_string (int (row.idle)) + (fast ? " Fast" : " Slow") + " burst "
                                     + std::to_string (int (row.burst)) + " @" + std::to_string (t4Times[j]),
                                 v, row.gain[j], err <= 1.0, err <= 1.0 ? err * err : 1.0 + 4.0 * (err - 1.0) * (err - 1.0) });
            }
        }
    }
}

} // namespace craft::levelrider::test
