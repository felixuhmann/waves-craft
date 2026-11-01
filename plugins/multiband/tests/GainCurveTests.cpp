// spec r2 section 4 / decision D6: static gain curve f(x; Knee, Range), threshold shift, static engine behaviour.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "dsp/GainCurve.h"

using namespace craft::multiband;
using Catch::Approx;

namespace
{
double xAt (int j) { return curve::xMin + curve::xStep * j; }

double f (double knee, double range, double x)
{
    GainCurve c;
    c.set (knee, range);
    return c.eval (x);
}
} // namespace

TEST_CASE ("Knee table rows are reproduced at Range -12", "[curve]")
{
    for (int k = 0; k < curve::numKneeRows; ++k)
        for (int j = 0; j < curve::numX; ++j)
            CHECK (f (curve::kneeValues[size_t (k)], -12.0, xAt (j)) == Approx (curve::kneeTable[k][j]).margin (1e-12));
}

TEST_CASE ("Range rows are reproduced at Knee 0.50, Hard-knee Range rows at Knee Hard", "[curve]")
{
    for (int r = 0; r < curve::numRangeRows; ++r)
        for (int j = 0; j < curve::numX; ++j)
            CHECK (f (0.5, curve::rangeValues[size_t (r)], xAt (j)) == Approx (curve::rangeTable[r][j]).margin (1e-12));
    for (int r = 0; r < curve::numHardRows; ++r)
        for (int j = 0; j < curve::numX; ++j)
            CHECK (f (0.75, curve::hardRangeValues[size_t (r)], xAt (j)) == Approx (curve::hardTable[r][j]).margin (1e-12));
}

TEST_CASE ("Spot checks of the combined rule (section 4)", "[curve]")
{
    CHECK (f (0.75, -24, -8) == Approx (0.126).margin (0.02));
    CHECK (f (0.75, -24, 0) == Approx (0.321).margin (0.02));
    CHECK (f (0.75, -24, 12) == Approx (0.665).margin (0.02));
    CHECK (f (0.75, 18, -4) == Approx (0.050).margin (0.02));
    CHECK (f (0.75, 18, 0) == Approx (0.214).margin (0.02));
    CHECK (f (0.75, 18, 12) == Approx (0.724).margin (0.02));
    CHECK (f (0.5, -12, 32) == Approx (0.95).margin (0.005));
}

TEST_CASE ("Knee interpolation is linear in the Knee value between rows", "[curve]")
{
    // halfway between the 0.30 and 0.50 rows (Range -12)
    for (int j = 0; j < curve::numX; ++j)
        CHECK (f (0.40, -12, xAt (j)) == Approx (0.5 * (curve::kneeTable[2][j] + curve::kneeTable[3][j])).margin (1e-9));
    // D6: between Knee 0.50 and Hard the Range correction blends into the Hard-knee rows
    for (int j = 0; j < curve::numX; ++j)
    {
        const double base = curve::kneeTable[4][j];
        const double w = (0.71 - 0.5) / 0.25;
        const double corr = (1 - w) * (curve::rangeTable[0][j] - curve::rangeTable[1][j]) + w * (curve::hardTable[0][j] - curve::hardTable[1][j]);
        CHECK (f (0.71, -24, xAt (j)) == Approx (std::clamp (base + corr, 0.0, 1.0)).margin (0.002));
    }
}

TEST_CASE ("f is monotonic, 0 far below and 1 far above, half the Range at +6.4 dB", "[curve]")
{
    for (double knee : { 0.0, 0.15, 0.3, 0.45, 0.5, 0.6, 0.71, 0.75 })
        for (double range : { -24.0, -18.0, -12.0, -8.0, -3.0, -0.5, 0.5, 3.0, 6.0, 12.0, 18.0 })
        {
            GainCurve c;
            c.set (knee, range);
            double prev = -1.0;
            for (double x = -80.0; x <= 80.0; x += 0.25)
            {
                const double v = c.eval (x);
                REQUIRE (v >= prev - 1e-12);
                REQUIRE (v >= 0.0);
                REQUIRE (v <= 1.0);
                prev = v;
            }
            CHECK (c.eval (-80.0) < 0.002);
            CHECK (c.eval (80.0) > 0.98);
            CHECK (c.eval (6.4) == Approx (0.5).margin (0.03));
        }
}

TEST_CASE ("Static gain through the engine: -10.17 dB at -10 dBFS (Range -12, Threshold -30)", "[curve][engine]")
{
    const double fs = 48000.0;
    for (auto [att, rel] : { std::pair<double, double> { 0.5, 5.0 }, { 5.0, 50.0 }, { 50.0, 50.0 } })
    {
        const auto s = test::band2Settings (-12.0, 0.5, att, rel, false, false);
        const auto x = test::toneSegments (fs, 1000.0, { { 1.5, -10.0 } });
        const auto g = test::runBandGain (s, fs, x, 1);
        CHECK (g.back() == Approx (-10.17).margin (0.3));
    }
}

TEST_CASE ("Static curve through the engine matches Gain + Range f(L - T) within the tolerance", "[curve][engine]")
{
    const double fs = 48000.0;
    const double band2At1k = -0.29; // |B2| at 1 kHz
    for (double knee : { 0.0, 0.5, 0.75 })
        for (double range : { -24.0, -12.0, 12.0, 18.0 })
            for (double level : { -60.0, -40.0, -30.0, -24.0, -18.0, -10.0, 0.0 })
            {
                auto s = test::band2Settings (range, knee, 0.5, 5.0, false, false);
                s.bands[1].gainDb = 2.0;
                const auto x = test::toneSegments (fs, 1000.0, { { 0.6, level } });
                const auto g = test::runBandGain (s, fs, x, 1);
                const double expect = 2.0 + range * f (knee, range, level + band2At1k + 30.0);
                const double tol = std::abs (range) <= 12.0 ? 0.3 : 0.4;
                CHECK (g.back() == Approx (expect).margin (tol));
            }
}

TEST_CASE ("Threshold shifts the curve exactly; Range 0 is a fixed gain; memoryless rising vs falling", "[curve][engine]")
{
    const double fs = 48000.0;
    auto steady = [&] (double thr, double level, double range = -12.0) {
        auto s = test::band2Settings (range, 0.5, 0.5, 5.0, false, false);
        s.bands[1].thresholdDb = thr;
        return test::runBandGain (s, fs, test::toneSegments (fs, 1000.0, { { 0.6, level } }), 1).back();
    };
    for (double d : { -20.0, -7.5, 10.0, 25.0 })
        CHECK (steady (-30.0 + d, -10.0 + d) == Approx (steady (-30.0, -10.0)).margin (0.02));

    CHECK (steady (-30.0, -10.0, 0.0) == Approx (0.0).margin (1e-9));

    // rising vs falling approach to the same level
    auto s = test::band2Settings (-12.0, 0.5, 0.5, 5.0, false, false);
    const auto up = test::runBandGain (s, fs, test::toneSegments (fs, 1000.0, { { 0.5, -40.0 }, { 0.8, -20.0 } }), 1).back();
    const auto down = test::runBandGain (s, fs, test::toneSegments (fs, 1000.0, { { 0.5, 0.0 }, { 2.5, -20.0 } }), 1).back();
    CHECK (up == Approx (down).margin (0.024)); // 0.002 in f at Range -12
}
