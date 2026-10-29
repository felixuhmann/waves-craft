// Static compressor/expander behaviour: Appendix A, spec 5, 10.1, 10.3, 10.4.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "SpecTables.h"
#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
Settings compSettings (double ratioPos, double thresholdDb)
{
    Settings s;
    s.ratioPos = ratioPos;
    s.compThresholdDb = thresholdDb;
    s.compAttackMs = 2.0;
    s.compReleaseMs = 50.0;
    return s;
}

// The spec's measurement: rows up to +41.28 with T = -40 dB, rows from +43.28 with T = -80 dB (sine peak <= 0 dBFS).
double rowThreshold (double x) { return x < 42.0 ? -40.0 : -80.0; }
double rowPeak (double x) { return x - 1.277 + rowThreshold (x); }

double tolerance (int col, double x, double measured, double unlimitedOutPeak)
{
    if (col >= 12) // cancellation ratios: +-0.3 near the notch
        return measured < -15.0 ? 0.3 : 0.1;
    if (col <= 3 && unlimitedOutPeak > -0.5) // expansion near the output limit
        return 0.5;
    (void) x;
    return 0.1;
}
} // namespace

TEST_CASE ("Appendix A: static gain of a steady 1 kHz sine for all 14 ratio positions", "[static]")
{
    // every row at 48 kHz, every 4th row also at 44.1 and 96 kHz (spec 9)
    int checked = 0;
    for (double fs : { 48000.0, 44100.0, 96000.0 })
        for (int col = 0; col < spectables::cols; ++col)
            for (int row = 0; row < spectables::rows; ++row)
            {
                if (fs != 48000.0 && row % 4 != 1)
                    continue;
                const double x = spectables::x[row], expected = spectables::f[row][col];
                const double peak = rowPeak (x);
                const double got = steadyGainDb (compSettings (spectables::positions[col], rowThreshold (x)), fs, peak, 1000.0, 0.15);
                const double unlimitedPeak = peak + curves::compressorCurveDb (spectables::positions[col], x);
                INFO ("fs " << fs << " r " << spectables::positions[col] << " x " << x << " peak " << peak);
                CHECK (got == Approx (expected).margin (tolerance (col, x, expected, unlimitedPeak)));
                ++checked;
            }
    CHECK (checked > 1000);
}

TEST_CASE ("10.1 static table (threshold -40 dB)", "[static]")
{
    struct Row { double r; double g[6]; };
    const Row rows[] = {
        { 0.3, { -0.03, -1.38, -4.48, -7.28, -8.59, -9.05 } },
        { 0.5, { -0.05, -2.15, -7.97, -16.24, -23.88, -29.01 } },
        { 0.8, { -0.05, -2.21, -8.28, -17.41, -27.44, -37.52 } },
        { 1.0, { -0.07, -2.99, -13.92, -18.08, -12.22, -10.97 } },
    };
    const double levels[6] = { -50, -40, -30, -20, -10, 0 };
    for (const auto& row : rows)
        for (int i = 0; i < 6; ++i)
        {
            INFO ("r " << row.r << " level " << levels[i]);
            CHECK (steadyGainDb (compSettings (row.r, -40.0), 48000.0, levels[i]) == Approx (row.g[i]).margin (row.r == 1.0 ? 0.3 : 0.1));
        }
}

TEST_CASE ("Threshold shift invariance", "[static]")
{
    for (double r : { 0.3, 0.5, 0.8, 1.0 })
        for (double x : { -10.0, 0.0, 5.0, 20.0, 35.0 })
        {
            const double ref = curves::compressorGainDb (r, x - 40.0, -40.0);
            for (double T : { -20.0, -60.0, -80.0 })
                CHECK (curves::compressorGainDb (r, x + T, T) == Approx (ref).margin (1.0e-9));
        }
}

TEST_CASE ("1.00:1 is exactly 0 dB, the neutral position and the displayed ratios", "[static][ratio]")
{
    for (double x = -100.0; x <= 100.0; x += 0.37)
        CHECK (curves::compressorCurveDb (ratio::neutralPos, x) == 0.0);
    CHECK (formatDisplay (Quantity::Ratio, 0.218) == "1.00:1");
    const double pos[] = { 0, .1, .2, .3, .4, .5, .6, .7, .8, .9, 1 };
    const double disp[] = { 0.50, 0.65, 0.93, 1.45, 2.46, 4.58, 9.29, 20.62, 50.00, -15.90, -5.00 };
    for (int i = 0; i < 11; ++i)
        CHECK (ratio::fromPosition (pos[i]) == Approx (disp[i]).margin (0.006));
    CHECK (ratio::fromPosition (0.8001) == Approx (-50.0).margin (0.1));
    for (double r : { 0.05, 0.218, 0.33, 0.77, 0.85, 0.95 })
        CHECK (ratio::toPosition (ratio::fromPosition (r)) == Approx (r).margin (1.0e-6));
    // max reduction (spec 5) of the moderate ratios
    CHECK (curves::compressorCurveDb (0.25, 80.0) == Approx (-3.6).margin (0.05));
    CHECK (curves::compressorCurveDb (0.3, 80.0) == Approx (-9.3).margin (0.05));
    CHECK (curves::compressorCurveDb (0.6, 80.0) == Approx (-46.4).margin (0.1));
}

TEST_CASE ("Makeup adds to the curve", "[static]")
{
    auto s = compSettings (0.5, -40.0);
    s.makeupDb = 12.0;
    CHECK (steadyGainDb (s, 48000.0, -20.0) == Approx (-16.24 + 12.0).margin (0.1));
    s.ratioPos = ratio::neutralPos;
    s.makeupDb = -6.5;
    CHECK (steadyGainDb (s, 48000.0, -20.0) == Approx (-6.5).margin (1.0e-6));
}

TEST_CASE ("10.3 expansion output limit", "[static][expansion]")
{
    auto s = compSettings (0.0, -80.0);
    const double in[] = { -10.0, -4.0, 0.0 }, out[] = { 2.6, 2.8, 2.83 };
    for (int i = 0; i < 3; ++i)
        CHECK (in[i] + steadyGainDb (s, 48000.0, in[i]) == Approx (out[i]).margin (0.5));
    s.ratioPos = 0.15;
    CHECK (steadyGainDb (s, 48000.0, 0.0) == Approx (2.46).margin (0.5));
    s.ratioPos = 0.2;
    CHECK (steadyGainDb (s, 48000.0, 0.0) == Approx (1.43).margin (0.5));
    // the output never exceeds the ceiling
    for (double lvl = -40.0; lvl <= 0.0; lvl += 4.0)
        CHECK (lvl + steadyGainDb (compSettings (0.0, -100.0), 48000.0, lvl) < 3.0);
}

TEST_CASE ("10.4 PeakRef offsets", "[static][peakref]")
{
    struct Row { double r; double o[4]; };
    const double T[4] = { -10, -20, -60, -80 };
    const Row rows[] = {
        { 0.3, { 5.27, 7.72, 9.25, 9.26 } },
        { 0.5, { 9.85, 18.25, 32.61, 32.94 } },
        { 0.8, { 10.28, 19.81, 40.0, 40.0 } },
        { 0.0, { -11.21, -19.13, -24.04, -24.08 } },
        { 1.0, { 5.77, 8.60, 10.44, 10.46 } },
    };
    for (const auto& row : rows)
        for (int i = 0; i < 4; ++i)
        {
            INFO ("r " << row.r << " T " << T[i]);
            CHECK (curves::peakRefOffsetDb (row.r, T[i]) == Approx (row.o[i]).margin (0.1));
        }
    CHECK (curves::peakRefOffsetDb (ratio::neutralPos, -40.0) == 0.0);

    // in the engine: PeakRef gain = LowRef gain + offset
    auto s = compSettings (0.5, -20.0);
    const double low = steadyGainDb (s, 48000.0, -15.0);
    s.peakRef = true;
    CHECK (steadyGainDb (s, 48000.0, -15.0) == Approx (low + 18.25).margin (0.1));
}
