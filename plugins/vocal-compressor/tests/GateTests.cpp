// Gate: static expansion (9.5), combined gains, closing trajectory (9.6), instant re-opening, D5.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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

TEST_CASE ("Gate closing trajectory (9.6) within 1.5 dB", "[gate]")
{
    const double ts[] = { 100, 200, 300, 350, 400, 500, 600, 700, 800, 900, 1000 };
    struct Row
    {
        double gate;
        double values[11];
    };
    const Row rows[] = {
        { -20, { -6.75, -13.38, -19.96, -23.22, -26.43, -32.68, -38.49, -43.55, -47.52, -50.01, -50.01 } },
        { -40, { 0.00, 0.00, 0.00, -3.22, -6.43, -12.68, -18.49, -23.55, -27.52, -30.01, -30.01 } },
        { -60, { 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, -3.55, -7.52, -10.01, -10.01 } } };
    const double fs = 48000.0;
    const long pre = ms (1500, fs);
    const auto x = sineSchedule ({ { -20.0, pre }, { -70.0, ms (1200, fs) } }, fs);
    for (const auto& row : rows)
    {
        const auto r = run (x, fs, settings (0.0, row.gate));
        for (int j = 0; j < 11; ++j)
        {
            INFO ("gate " << row.gate << " t " << ts[j]);
            CHECK_THAT (gainDb (r, pre + ms (ts[j], fs)), WithinAbs (row.values[j], 1.5));
        }
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

TEST_CASE ("Gate releases at 63 dB/s in the linear part of the ramp", "[gate]")
{
    const double fs = 48000.0;
    const long pre = ms (1000, fs);
    // into silence: no floor, the detector decays at exactly 63 dB/s
    const auto x = sineSchedule ({ { 0.0, pre }, { -999.0, ms (1000, fs) } }, fs);
    const auto r = run (x, fs, settings (0.0, -10.0));
    const double g200 = gainDb (r, pre + ms (200, fs)), g600 = gainDb (r, pre + ms (600, fs));
    CHECK_THAT (g200 - g600, WithinAbs (63.0 * 0.4, 0.05));
}
