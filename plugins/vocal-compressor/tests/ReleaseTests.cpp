// Program-dependent release: tables 9.3 / 9.4 within the section 8 tolerances, 50 Hz ripple, sample-rate
// independence, short transients.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;

namespace
{
const double tMs[11] = { 2, 5, 10, 20, 50, 100, 200, 400, 700, 1000, 1500 };

struct SustainedRow
{
    double comp, upper, lower;
    double values[11];
};
const SustainedRow sustained[] = {
    { -6, 0, -40, { 5.82, 5.81, 5.78, 5.69, 5.25, 4.52, 3.44, 2.09, 1.04, 0.51, 0.13 } },
    { -6, -10, -40, { 2.19, 2.17, 2.15, 2.10, 1.95, 1.74, 1.38, 0.87, 0.42, 0.19, 0.01 } },
    { -6, -20, -30, { 0.46, 0.46, 0.45, 0.44, 0.41, 0.36, 0.28, 0.16, 0.05, -0.01, -0.06 } },
    { -18, 0, -40, { 17.33, 17.32, 17.30, 17.22, 16.55, 14.06, 8.68, 4.31, 2.00, 0.99, 0.29 } },
    { -18, -10, -40, { 9.42, 9.41, 9.39, 9.32, 8.82, 7.51, 5.32, 3.03, 1.47, 0.73, 0.20 } },
    { -18, -20, -30, { 2.77, 2.76, 2.74, 2.68, 2.49, 2.21, 1.73, 1.08, 0.53, 0.25, 0.03 } },
    { -36, 0, -40, { 31.71, 31.69, 31.61, 31.33, 29.41, 24.02, 14.34, 5.97, 2.46, 1.18, 0.35 } },
    { -36, -10, -40, { 22.01, 22.00, 21.97, 21.87, 21.14, 18.57, 12.23, 5.41, 2.29, 1.11, 0.33 } },
    { -36, -20, -30, { 7.63, 7.63, 7.63, 7.61, 7.50, 7.13, 5.97, 3.65, 1.66, 0.79, 0.22 } } };

struct BurstRow
{
    double comp, burstMs;
    double values[11];
};
const BurstRow bursts[] = {
    { -6, 2, { 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00 } },
    { -6, 5, { 0.01, 0.01, 0.01, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00 } },
    { -6, 10, { 0.09, 0.09, 0.09, 0.08, 0.07, 0.06, 0.03, 0.00, 0.00, 0.00, 0.00 } },
    { -6, 20, { 0.26, 0.25, 0.25, 0.24, 0.22, 0.19, 0.14, 0.07, 0.00, 0.00, 0.00 } },
    { -6, 50, { 0.73, 0.73, 0.72, 0.70, 0.65, 0.58, 0.46, 0.27, 0.11, 0.02, 0.00 } },
    { -6, 100, { 1.46, 1.45, 1.43, 1.40, 1.30, 1.16, 0.92, 0.58, 0.27, 0.11, 0.00 } },
    { -6, 200, { 2.87, 2.67, 2.62, 2.56, 2.38, 2.11, 1.67, 1.05, 0.52, 0.24, 0.03 } },
    { -6, 500, { 5.56, 5.30, 5.02, 4.72, 4.26, 3.71, 2.86, 1.77, 0.88, 0.43, 0.10 } },
    { -6, 1000, { 5.81, 5.78, 5.72, 5.59, 5.12, 4.42, 3.37, 2.05, 1.02, 0.50, 0.13 } },
    { -18, 2, { 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00 } },
    { -18, 5, { 0.02, 0.02, 0.02, 0.02, 0.01, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00 } },
    { -18, 10, { 0.17, 0.17, 0.17, 0.16, 0.15, 0.13, 0.09, 0.03, 0.00, 0.00, 0.00 } },
    { -18, 20, { 0.47, 0.47, 0.46, 0.45, 0.42, 0.37, 0.28, 0.16, 0.05, 0.00, 0.00 } },
    { -18, 50, { 1.36, 1.34, 1.33, 1.30, 1.21, 1.08, 0.85, 0.53, 0.25, 0.09, 0.00 } },
    { -18, 100, { 3.44, 3.02, 2.82, 2.69, 2.50, 2.20, 1.74, 1.09, 0.54, 0.25, 0.04 } },
    { -18, 200, { 9.12, 7.76, 6.84, 6.01, 5.07, 4.32, 3.29, 2.00, 0.99, 0.49, 0.12 } },
    { -18, 500, { 17.15, 16.87, 16.41, 15.51, 13.02, 9.94, 6.51, 3.54, 1.69, 0.84, 0.24 } },
    { -18, 1000, { 17.32, 17.30, 17.26, 17.12, 16.27, 13.55, 8.38, 4.22, 1.96, 0.97, 0.29 } },
    { -36, 2, { 0.39, 0.06, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00 } },
    { -36, 5, { 0.53, 0.14, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00 } },
    { -36, 10, { 0.70, 0.25, 0.03, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00 } },
    { -36, 20, { 1.09, 0.56, 0.23, 0.04, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00 } },
    { -36, 50, { 3.28, 2.35, 1.75, 1.28, 0.93, 0.80, 0.63, 0.39, 0.17, 0.05, 0.00 } },
    { -36, 100, { 7.84, 6.00, 4.82, 3.82, 2.80, 2.27, 1.75, 1.08, 0.53, 0.24, 0.03 } },
    { -36, 200, { 21.35, 16.62, 13.40, 10.61, 7.56, 5.64, 3.93, 2.29, 1.10, 0.54, 0.14 } },
    { -36, 500, { 31.24, 30.52, 29.39, 27.31, 22.30, 16.49, 9.93, 4.59, 2.03, 0.98, 0.29 } },
    { -36, 1000, { 31.68, 31.61, 31.45, 31.01, 28.65, 23.03, 13.73, 5.79, 2.41, 1.16, 0.35 } } };


struct PatternRow
{
    double comp;
    const char* pattern;
    double values[12];
};
const double t97Ms[12] = { 2, 5, 10, 20, 50, 100, 200, 400, 700, 1000, 1500, 2000 };
const PatternRow patterns[] = {
    { -12, "step_-6_-30_300", { 5.95, 5.28, 4.77, 4.32, 3.84, 3.36, 2.62, 1.64, 0.84, 0.44, 0.14, 0.03 } },
    { -12, "step_0_-20_50", { 1.27, 0.97, 0.85, 0.81, 0.76, 0.68, 0.55, 0.36, 0.19, 0.10, 0.03, 0.01 } },
    { -12, "step_0_-60_1500", { 11.91, 11.90, 11.88, 11.81, 11.22, 9.43, 6.39, 3.53, 1.70, 0.86, 0.26, 0.05 } },
    { -12, "step_-3_-40_150", { 3.67, 3.28, 3.09, 2.97, 2.76, 2.44, 1.94, 1.24, 0.64, 0.33, 0.10, 0.02 } },
    { -12, "step_-10_-20_2000", { 3.47, 3.47, 3.45, 3.40, 3.18, 2.81, 2.20, 1.39, 0.72, 0.37, 0.12, 0.02 } },
    { -12, "dbl_50", { 2.41, 2.31, 2.28, 2.23, 2.08, 1.86, 1.49, 0.96, 0.50, 0.26, 0.08, 0.02 } },
    { -12, "train_30", { 2.06, 2.02, 2.00, 1.95, 1.83, 1.63, 1.31, 0.85, 0.45, 0.23, 0.07, 0.01 } },
    { -24, "step_-6_-30_300", { 14.38, 13.45, 12.29, 10.74, 8.32, 6.40, 4.49, 2.60, 1.27, 0.64, 0.20, 0.04 } },
    { -24, "step_0_-20_50", { 7.27, 4.87, 3.32, 2.02, 0.75, 0.21, 0.02, 0.00, 0.00, 0.00, 0.00, 0.00 } },
    { -24, "step_0_-60_1500", { 23.79, 23.77, 23.74, 23.59, 22.41, 18.20, 10.27, 4.80, 2.19, 1.09, 0.33, 0.07 } },
    { -24, "step_-3_-40_150", { 7.15, 6.04, 5.29, 4.66, 4.01, 3.48, 2.70, 1.68, 0.86, 0.45, 0.14, 0.03 } },
    { -24, "step_-10_-20_2000", { 7.15, 7.15, 7.14, 7.13, 7.01, 6.62, 5.43, 3.16, 1.46, 0.72, 0.22, 0.04 } },
    { -24, "dbl_50", { 4.12, 3.51, 3.15, 2.90, 2.66, 2.35, 1.86, 1.19, 0.62, 0.32, 0.10, 0.02 } },
    { -24, "train_30", { 3.41, 2.98, 2.72, 2.56, 2.36, 2.10, 1.67, 1.07, 0.56, 0.29, 0.09, 0.02 } },
    { -36, "step_-6_-30_300", { 20.16, 19.47, 18.45, 16.76, 13.36, 10.09, 6.65, 3.54, 1.58, 0.77, 0.23, 0.05 } },
    { -36, "step_0_-20_50", { 16.95, 14.78, 12.32, 9.38, 5.48, 3.09, 1.41, 0.46, 0.11, 0.03, 0.00, 0.00 } },
    { -36, "step_0_-60_1500", { 35.38, 35.34, 35.23, 34.76, 31.52, 23.29, 12.05, 5.23, 2.33, 1.15, 0.35, 0.07 } },
    { -36, "step_-3_-40_150", { 13.81, 10.52, 8.49, 6.76, 4.92, 3.82, 2.83, 1.73, 0.87, 0.45, 0.14, 0.03 } },
    { -36, "step_-10_-20_2000", { 9.02, 9.02, 9.02, 9.00, 8.91, 8.61, 7.64, 5.33, 2.82, 1.45, 0.45, 0.09 } },
    { -36, "dbl_50", { 7.83, 6.01, 4.84, 3.84, 2.84, 2.31, 1.79, 1.13, 0.59, 0.30, 0.09, 0.02 } },
    { -36, "train_30", { 7.77, 5.96, 4.80, 3.81, 2.82, 2.29, 1.78, 1.13, 0.58, 0.30, 0.09, 0.02 } },
};

// Builds the section 9.7 stimulus; returns the index of the last loud sample + 1 and the final level.
std::vector<double> patternSignal (const std::string& name, double fs, long& edge, double& lower)
{
    std::vector<Segment> segs;
    if (name.rfind ("step_", 0) == 0)
    {
        // "step_<hi>_<lo>_<ms>"
        const auto a = name.find ('_', 5), b = name.find ('_', a + 1);
        const double hi = std::stod (name.substr (5, a - 5)), lo = std::stod (name.substr (a + 1, b - a - 1)),
                     len = std::stod (name.substr (b + 1));
        segs = { { lo, ms (500, fs) }, { hi, ms (len, fs) } };
        lower = lo;
    }
    else if (name == "dbl_50")
    {
        segs = { { -40, ms (1500, fs) }, { 0, ms (50, fs) }, { -40, ms (50, fs) }, { 0, ms (50, fs) } };
        lower = -40;
    }
    else // train_30
    {
        segs = { { -40, ms (1500, fs) } };
        for (int i = 0; i < 5; ++i)
        {
            segs.push_back ({ 0, ms (30, fs) });
            if (i < 4)
                segs.push_back ({ -40, ms (170, fs) });
        }
        lower = -40;
    }
    edge = 0;
    for (const auto& sg : segs)
        edge += sg.samples;
    segs.push_back ({ lower, ms (2100, fs) });
    return sineSchedule (segs, fs);
}

// Remaining gain reduction (dB) relative to the steady gain at `lowerDb`, at t ms after sample `edge`.
std::vector<double> remaining (const Result& r, long edge, double comp, double lowerDb, double fs, const double* times = tMs,
                               int count = 11)
{
    const double finalDb = staticGainDb (comp, lowerDb);
    std::vector<double> out;
    for (int i = 0; i < count; ++i)
        out.push_back (finalDb - gainDb (r, edge + ms (times[i], fs)));
    return out;
}

std::vector<double> runSustained (const SustainedRow& row, double fs)
{
    const long pre = ms (1500, fs);
    const auto x = sineSchedule ({ { row.upper, pre }, { row.lower, ms (1600, fs) } }, fs);
    return remaining (run (x, fs, settings (row.comp)), pre, row.comp, row.lower, fs);
}

std::vector<double> runBurst (const BurstRow& row, double fs)
{
    const long pre = ms (1500, fs), len = ms (row.burstMs, fs);
    const auto x = sineSchedule ({ { -40.0, pre }, { 0.0, len }, { -40.0, ms (1600, fs) } }, fs);
    return remaining (run (x, fs, settings (row.comp)), pre + len, row.comp, -40.0, fs);
}
} // namespace

TEST_CASE ("Release after sustained levels (9.3) within tolerance", "[release]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    for (const auto& row : sustained)
    {
        const auto got = runSustained (row, fs);
        for (int j = 0; j < 11; ++j)
        {
            INFO ("fs " << fs << " comp " << row.comp << " step " << row.upper << "->" << row.lower << " t " << tMs[j] << " ms: got "
                          << got[std::size_t (j)] << " ref " << row.values[j]);
            CHECK_THAT (got[std::size_t (j)], WithinAbs (row.values[j], releaseTolerance (row.values[j])));
        }
    }
}

TEST_CASE ("Release after bursts (9.4) within tolerance", "[release]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    for (const auto& row : bursts)
    {
        const auto got = runBurst (row, fs);
        for (int j = 0; j < 11; ++j)
        {
            INFO ("fs " << fs << " comp " << row.comp << " burst " << row.burstMs << " ms, t " << tMs[j] << " ms: got " << got[std::size_t (j)]
                          << " ref " << row.values[j]);
            CHECK_THAT (got[std::size_t (j)], WithinAbs (row.values[j], releaseTolerance (row.values[j])));
        }
    }
}

TEST_CASE ("Release times are fixed in milliseconds (44.1 / 96 kHz vs 48 kHz)", "[release]")
{
    const double fs = GENERATE (44100.0, 96000.0);
    for (int i : { 0, 3, 6, 7 })
    {
        const auto ref = runSustained (sustained[i], 48000.0);
        const auto got = runSustained (sustained[i], fs);
        for (int j = 1; j < 11; ++j) // from 5 ms on (2 ms is within the look-ahead smoothing, which scales in samples)
        {
            INFO ("fs " << fs << " row " << i << " t " << tMs[j]);
            CHECK_THAT (got[std::size_t (j)], WithinAbs (ref[std::size_t (j)], 0.1));
        }
    }
    for (int i : { 15, 24, 25 })
    {
        const auto ref = runBurst (bursts[i], 48000.0);
        const auto got = runBurst (bursts[i], fs);
        for (int j = 1; j < 11; ++j)
        {
            INFO ("fs " << fs << " burst row " << i << " t " << tMs[j]);
            CHECK_THAT (got[std::size_t (j)], WithinAbs (ref[std::size_t (j)], 0.2));
        }
    }
}

TEST_CASE ("Release after other level patterns (9.7) within tolerance", "[release]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    for (const auto& row : patterns)
    {
        long edge = 0;
        double lower = 0;
        const auto x = patternSignal (row.pattern, fs, edge, lower);
        const auto got = remaining (run (x, fs, settings (row.comp)), edge, row.comp, lower, fs, t97Ms, 12);
        for (int j = 0; j < 12; ++j)
        {
            INFO ("fs " << fs << " comp " << row.comp << " " << row.pattern << " t " << t97Ms[j] << " ms: got "
                        << got[std::size_t (j)] << " ref " << row.values[j]);
            CHECK_THAT (got[std::size_t (j)], WithinAbs (row.values[j], releaseTolerance (row.values[j])));
        }
    }
}

TEST_CASE ("Steady low-frequency tones: gain ripple within 9.8 + 0.15 dB (r3 stimulus)", "[release]")
{
    struct R
    {
        double comp, f, ref[4]; // windows starting 1.0 / 1.5 / 2.0 / 3.5 s after onset
    };
    const R table[] = { { -6, 50, { 0.69, 0.58, 0.56, 0.55 } },  { -6, 100, { 0.24, 0.19, 0.17, 0.17 } },
                        { -6, 200, { 0.02, 0.01, 0.01, 0.01 } }, { -18, 50, { 0.20, 0.13, 0.12, 0.12 } },
                        { -18, 100, { 0.04, 0.02, 0.02, 0.02 } }, { -18, 200, { 0.00, 0.00, 0.00, 0.00 } },
                        { -36, 50, { 0.11, 0.06, 0.05, 0.05 } },  { -36, 100, { 0.03, 0.01, 0.01, 0.01 } },
                        { -36, 200, { 0.00, 0.00, 0.00, 0.00 } } };
    const double windows[4] = { 1000, 1500, 2000, 3500 };
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    for (const auto& t : table)
    {
        // 1.5 s silence, then a -6 dBFS sine from phase 0 held 4 s
        const long onset = ms (1500, fs);
        const auto x = sineSchedule ({ { -999.0, onset }, { -6.0, ms (4000, fs) } }, fs, t.f);
        const auto r = run (x, fs, settings (t.comp));
        for (int w = 0; w < 4; ++w)
        {
            double lo = 1e9, hi = -1e9;
            const long from = onset + ms (windows[w], fs);
            for (long i = from; i < from + ms (500, fs); ++i)
                if (std::abs (x[std::size_t (i)]) > 0.5 * dbToLin (-6.0))
                {
                    lo = std::min (lo, gainDb (r, i));
                    hi = std::max (hi, gainDb (r, i));
                }
            INFO ("fs " << fs << " comp " << t.comp << " f " << t.f << " window " << windows[w] << " ms: ripple " << hi - lo);
            CHECK (hi - lo <= t.ref[w] + 0.15);
        }
    }
}

TEST_CASE ("Short transients recover within a few milliseconds (4.4 item 1)", "[release]")
{
    const double fs = 48000.0;
    for (double comp : { -6.0, -18.0, -36.0 })
    {
        const BurstRow row { comp, 10, {} };
        const auto got = runBurst (row, fs);
        INFO ("comp " << comp);
        CHECK (std::abs (got[3]) < 0.5); // 20 ms after a 10 ms burst
    }
}
