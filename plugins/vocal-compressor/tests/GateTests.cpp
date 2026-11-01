// Gate: static expansion (9.5), combined gains, closing trajectory (9.6), instant re-opening, D5.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <catch2/generators/catch_generators.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;

namespace
{
// Steady gain (dB, 0.99 excluded) of a 1 kHz sine at level L.
double steadyGain (double comp, double gate, double level)
{
    const double fs = 48000.0;
    const auto x = sineSchedule ({ { level, ms (1500, fs) } }, fs);
    const auto r = run (x, fs, settings (comp, gate));
    return gainDb (r, ms (1400, fs));
}
} // namespace

TEST_CASE ("Gate static curve 1:2 below the threshold (9.5)", "[gate]")
{
    struct P
    {
        double gate, level, expected;
    };
    const P points[] = { { -40, -46, -6.0 }, { -40, -50, -10.0 }, { -40, -70, -30.0 }, { -40, -90, -50.0 },
                         { -78, -84, -6.0 }, { -40, -40, 0.0 },   { -40, -30, 0.0 },   { -40, -10, 0.0 },
                         { -20, -22, -2.0 }, { -60, -61, -1.0 } };
    for (const auto& p : points)
    {
        INFO ("gate " << p.gate << " level " << p.level);
        CHECK_THAT (steadyGain (0.0, p.gate, p.level), WithinAbs (p.expected, 0.2));
    }
}

TEST_CASE ("Gate and compressor gains multiply; compressor sees the ungated level (9.5, D4)", "[gate]")
{
    CHECK_THAT (steadyGain (-18.0, -40.0, -50.0), WithinAbs (7.8, 0.2));
    CHECK_THAT (steadyGain (-18.0, -40.0, -60.0), WithinAbs (-2.1, 0.2));
}

TEST_CASE ("Gate off (r = 0) never reduces (D5)", "[gate]")
{
    for (double level : { -120.0, -90.0, -70.0, -40.0 })
        CHECK_THAT (steadyGain (0.0, param::Gate::offValue, level), WithinAbs (0.0, 1e-9));
    CHECK (param::Gate::fromNorm (0.0) == param::Gate::offValue);
    CHECK (param::Gate::isOff (param::Gate::fromNorm (0.0)));
    CHECK_FALSE (param::Gate::isOff (param::Gate::fromNorm (1.0e-4)));
}

namespace
{
struct ClosingRow
{
    double gate, hi, lo;
    std::vector<double> t, values;
};

void checkClosing (const ClosingRow& row, double fs)
{
    const long pre = ms (1500, fs);
    const auto x = sineSchedule ({ { row.hi, pre }, { row.lo, ms (1700, fs) } }, fs);
    const auto r = run (x, fs, settings (0.0, row.gate));
    const double before = gainDb (r, pre - ms (10, fs));
    for (std::size_t j = 0; j < row.t.size(); ++j)
    {
        INFO ("fs " << fs << " gate " << row.gate << " " << row.hi << " -> " << row.lo << " t " << row.t[j]);
        CHECK_THAT (gainDb (r, pre + ms (row.t[j], fs)) - before, WithinAbs (row.values[j], 0.75));
    }
}
} // namespace

TEST_CASE ("Gate closing trajectories (9.6, 9.6.1) within 0.75 dB", "[gate]")
{
    const std::vector<double> t96 { 100, 200, 300, 350, 400, 500, 600, 700, 800, 900, 1000 };
    const std::vector<double> t961 { 50, 100, 200, 300, 400, 500, 600, 700, 750, 800, 850, 900, 1000, 1200, 1500 };
    const ClosingRow rows[] = {
        { -20, -20, -70, t96, { -6.75, -13.38, -19.96, -23.22, -26.43, -32.68, -38.49, -43.55, -47.52, -50.01, -50.01 } },
        { -40, -20, -70, t96, { 0.00, 0.00, 0.00, -3.22, -6.43, -12.68, -18.49, -23.55, -27.52, -30.01, -30.01 } },
        { -60, -20, -70, t96, { 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, -3.55, -7.52, -10.01, -10.01 } },
        { 0, 0, -50, t961, { -3.41, -6.74, -13.37, -19.95, -26.43, -32.67, -38.49, -43.55, -45.68, -47.51, -49.02, -50.00, -50.00, -50.00, -50.00 } },
        { 0, 0, -30, t961, { -3.33, -6.55, -12.79, -18.59, -23.63, -27.57, -30.00, -30.00, -30.00, -30.00, -30.00, -30.00, -30.00, -30.00, -30.00 } },
        { -20, 0, -50, t961, { -0.00, -0.00, -0.00, -0.00, -6.43, -12.68, -18.49, -23.55, -25.69, -27.51, -29.02, -30.01, -30.01, -30.01, -30.01 } },
        { -20, -10, -60, t961, { 0.00, 0.00, -3.38, -9.96, -16.43, -22.68, -28.49, -33.55, -35.69, -37.51, -39.02, -40.00, -40.00, -40.00, -40.00 } },
        { -40, -10, -60, t961, { 0.00, 0.00, 0.00, 0.00, 0.00, -2.68, -8.49, -13.55, -15.69, -17.51, -19.02, -20.01, -20.01, -20.01, -20.01 } } };
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    for (const auto& row : rows)
        checkClosing (row, fs);
}

TEST_CASE ("Gate detector release time vs distance (9.6.1)", "[gate]")
{
    const double D[] = { 0.1, 0.25, 0.5, 1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 35, 40, 45 };
    const double tMs[] = { 4.5, 11.0, 21.3, 40.6, 74.8, 104.8, 131.9, 156.7, 179.9, 222.6, 261.9, 298.8, 351.1, 433.4, 512.2,
                           589.3, 665.4, 740.9, 816.1 };
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    for (int i = 0; i < 19; ++i)
    {
        GateDetector g;
        g.prepare (fs);
        const double level = dbToLin (-60.0);
        g.process (dbToLin (-60.0 + D[i]));
        long n = 0;
        while (g.process (level) > -60.0 + 1e-12 && n < long (fs * 2))
            ++n;
        INFO ("fs " << fs << " D " << D[i]);
        // the table is the reference's measured time; the r2 law reproduces it within a few ms
        CHECK_THAT (1000.0 * double (n + 1) / fs, WithinAbs (tMs[i], 4.0 + 0.01 * tMs[i]));
    }
}

TEST_CASE ("Gate re-opens completely at the first loud sample (look-ahead)", "[gate]")
{
    const double fs = 48000.0;
    const long pre = ms (1500, fs), closed = ms (1500, fs);
    const auto x = sineSchedule ({ { -20.0, pre }, { -70.0, closed }, { -20.0, ms (200, fs) } }, fs);
    const auto r = run (x, fs, settings (0.0, -40.0));
    CHECK (gainDb (r, pre + closed - 200) < -25.0); // closed before
    for (long i = pre + closed; i < pre + closed + ms (100, fs); ++i)
        REQUIRE_THAT (gainDb (r, i), WithinAbs (0.0, 0.01));
}

TEST_CASE ("Gate releases at about 66 dB/s far above the level", "[gate]")
{
    const double fs = 48000.0;
    const long pre = ms (1000, fs);
    // into silence the distance stays large: S = 21.57 + 44.19 = 65.76 dB/s
    const auto x = sineSchedule ({ { 0.0, pre }, { -999.0, ms (1000, fs) } }, fs);
    const auto r = run (x, fs, settings (0.0, -10.0));
    const double g200 = gainDb (r, pre + ms (200, fs)), g600 = gainDb (r, pre + ms (600, fs));
    CHECK_THAT (g200 - g600, WithinAbs (65.76 * 0.4, 0.05));
}
