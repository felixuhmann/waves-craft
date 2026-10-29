// Embedded spec tables and the control laws built on them (T0 tapers, T1 lookup, T2 gain law, section 9).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

#include "SpecMeasure.h"

using namespace optotest;
using Catch::Approx;

TEST_CASE ("spec tables are embedded with their published shapes", "[tables]")
{
    CHECK (spec::numTables == 13);
    CHECK (spec::T1C.numRows == 9);
    CHECK (spec::T1C.numCols == 17);
    CHECK (spec::T1L.numRows == 9);
    CHECK (spec::T2.numCols == 10);
    CHECK (spec::T3.numRows == 30);
    CHECK (spec::T3_1.numRows == 30);
    CHECK (spec::T4.numRows == 6);
    CHECK (spec::T5.numRows == 5);
    CHECK (spec::T6.numRows == 4);
    CHECK (spec::T7.numRows == 36);
    CHECK (spec::T7b.numRows == 2);
    CHECK (spec::T8.numCols == 15);
    CHECK (spec::T0.numCols == 12);
    CHECK (spec::T0_1.numCols == 12);

    // spot checks against the Markdown source
    CHECK (spec::T1C.at (4, 0) == 50.0);
    CHECK (spec::T1C.headerValue[16] == 0.0);
    CHECK (spec::T1C.at (4, 16) == Approx (20.29));
    CHECK (spec::T1L.at (8, 16) == Approx (31.23));
    CHECK (spec::T2.at (0, 4) == Approx (-0.05));
    CHECK (std::string (spec::T3.textAt (29, 0)) == "L 50");
    CHECK (std::isnan (spec::T3.at (0, 7)));          // "—" placeholder
    CHECK (spec::T3.headerValue[3] == Approx (0.001)); // "A 0.001"
    CHECK (spec::T3_1.headerValue[14] == Approx (19.0));
    CHECK (spec::T4.headerValue[8] == Approx (10000.0));
    CHECK (spec::T0_1.at (0, 6) == Approx (42.36));
    CHECK (spec::T7b.at (1, 8) == Approx (-76.0));
}

TEST_CASE ("T0 host tapers map normalised values to the display scale", "[tables][T0]")
{
    const auto gain = dsp::gainTaper();
    const auto pr = dsp::peakReductionTaper();
    for (int i = 0; i <= 10; ++i)
    {
        const double r = i / 10.0;
        CHECK (gain.toDisplay (r) == Approx (spec::T0.at (0, i + 1)).margin (1e-9));
        CHECK (pr.toDisplay (r) == Approx (spec::T0_1.at (0, i + 1)).margin (1e-9));
    }
    // piecewise linear in between, and invertible
    CHECK (gain.toDisplay (0.05) == Approx (9.32));
    for (double d = 0.0; d <= 100.0; d += 0.37)
    {
        CHECK (gain.toDisplay (gain.toNormalised (d)) == Approx (d).margin (1e-9));
        CHECK (pr.toDisplay (pr.toNormalised (d)) == Approx (d).margin (1e-9));
    }
    // defaults (spec 2): Gain 40.00, Peak Reduction 46.00
    CHECK (gain.toNormalised (40.0) == Approx (0.53).margin (0.005));
    CHECK (pr.toNormalised (46.0) == Approx (0.6).margin (0.005));
}

TEST_CASE ("T1 static curve lookup interpolates as specified", "[tables][T1]")
{
    const auto& c = dsp::staticCurve (dsp::Mode::Compress);
    const auto& l = dsp::staticCurve (dsp::Mode::Limit);
    // exact at the table points
    for (int r = 0; r < spec::T1C.numRows; ++r)
        for (int k = 1; k < spec::T1C.numCols; ++k)
        {
            CHECK (c.gainReductionDb (spec::T1C.at (r, 0), spec::T1C.headerValue[k]) == Approx (spec::T1C.at (r, k)).margin (1e-9));
            CHECK (l.gainReductionDb (spec::T1L.at (r, 0), spec::T1L.headerValue[k]) == Approx (spec::T1L.at (r, k)).margin (1e-9));
        }
    // linear in level and in Peak Reduction
    CHECK (c.gainReductionDb (50.0, -10.0) == Approx (0.5 * (11.13 + 14.20)));
    CHECK (c.gainReductionDb (55.0, -8.0) == Approx (0.5 * (14.20 + 18.84)));
    // below the lowest tabulated row: no reduction
    CHECK (c.gainReductionDb (10.0, 0.0) == 0.0);
    CHECK (c.gainReductionDb (0.0, 20.0) == 0.0);
    // PR <= 30: no reduction below 0 dBFS (spec 4.1)
    CHECK (c.gainReductionDb (30.0, -0.1) == 0.0);
    // monotonic in level, continues above 0 dBFS
    double prev = -1.0;
    for (double lev = -70.0; lev <= 12.0; lev += 0.5)
    {
        const double g = c.gainReductionDb (70.0, lev);
        CHECK (g >= prev);
        prev = g;
    }
    CHECK (c.gainReductionDb (100.0, 6.0) > 34.17);
}

TEST_CASE ("T2 gain law", "[tables][T2]")
{
    for (int k = 1; k < spec::T2.numCols; ++k)
        CHECK (dsp::gainLawDb (spec::T2.headerValue[k]) == Approx (spec::T2.at (0, k)).margin (1e-9));
    CHECK (dsp::gainLawDb (5.0) == Approx (0.5 * (-51.36 - 34.95)));
    for (double d = 0.0; d <= 100.0; d += 1.3)
        CHECK (dsp::gainLawInverse (dsp::gainLawDb (d)) == Approx (d).margin (1e-9));
}

TEST_CASE ("Auto Makeup coupling follows the measured offsets (spec 9)", "[tables][automakeup]")
{
    CHECK (dsp::autoMakeupDb (50.0) - dsp::autoMakeupDb (40.0) == Approx (3.7).margin (0.05));
    CHECK (dsp::autoMakeupDb (60.0) - dsp::autoMakeupDb (40.0) == Approx (6.9).margin (0.05));
    CHECK (dsp::autoMakeupDb (70.0) - dsp::autoMakeupDb (40.0) == Approx (9.6).margin (0.05));
    double prev = -1.0;
    for (double p = 0.0; p <= 100.0; p += 1.0)
    {
        CHECK (dsp::autoMakeupDb (p) >= prev);
        prev = dsp::autoMakeupDb (p);
    }
}
