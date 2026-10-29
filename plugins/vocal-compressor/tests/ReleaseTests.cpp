// Program-dependent release: tables 9.3 / 9.4 within the section 8 tolerances, 50 Hz ripple, sample-rate
// independence, short transients.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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

// Remaining gain reduction (dB) relative to the steady gain at `lowerDb`, at t ms after sample `edge`.
std::vector<double> remaining (const Result& r, long edge, double comp, double lowerDb, double fs)
{
    const double finalDb = staticGainDb (comp, lowerDb);
    std::vector<double> out;
    for (double t : tMs)
        out.push_back (finalDb - gainDb (r, edge + ms (t, fs)));
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
    for (const auto& row : sustained)
    {
        const auto got = runSustained (row, 48000.0);
        for (int j = 0; j < 11; ++j)
        {
            INFO ("comp " << row.comp << " step " << row.upper << "->" << row.lower << " t " << tMs[j] << " ms: got "
                          << got[std::size_t (j)] << " ref " << row.values[j]);
            CHECK_THAT (got[std::size_t (j)], WithinAbs (row.values[j], releaseTolerance (row.values[j])));
        }
    }
}

TEST_CASE ("Release after bursts (9.4) within tolerance", "[release]")
{
    for (const auto& row : bursts)
    {
        const auto got = runBurst (row, 48000.0);
        for (int j = 0; j < 11; ++j)
        {
            INFO ("comp " << row.comp << " burst " << row.burstMs << " ms, t " << tMs[j] << " ms: got " << got[std::size_t (j)]
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

TEST_CASE ("Steady 50 Hz tone: gain ripple per cycle <= 0.2 dB", "[release]")
{
    const double fs = 48000.0;
    for (double comp : { -6.0, -18.0, -36.0 })
        for (double level : { 0.0, -12.0, -30.0 })
        {
            const auto x = sineSchedule ({ { level, ms (3000, fs) } }, fs, 50.0);
            const auto r = run (x, fs, settings (comp));
            double lo = 1e9, hi = -1e9;
            for (long i = ms (2500, fs); i < ms (2540, fs); ++i)
            {
                lo = std::min (lo, gainDb (r, i));
                hi = std::max (hi, gainDb (r, i));
            }
            INFO ("comp " << comp << " level " << level);
            CHECK (hi - lo <= 0.2);
            CHECK_THAT (hi, WithinAbs (staticGainDb (comp, level), 0.2)); // the steady gain is the static one
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
