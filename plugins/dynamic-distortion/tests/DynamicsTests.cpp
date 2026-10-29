// Spec r1 sections 4.3 (expansion, detector placement) and 6.2 (envelope step trajectories).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "SpecTables.h"
#include "TestHelpers.h"

using namespace ddtest;
using Catch::Approx;

namespace
{
double expansionIncrease (const Settings& s, double fs, double level, double f = 1000.0)
{
    const double small = measureTone (s, fs, f, -50.0, 0.4, 0.2).gainDb;
    return measureTone (s, fs, f, level, 0.4, 0.2).gainDb - small;
}
} // namespace

TEST_CASE ("Expansion static curve matches section 4.3", "[expansion]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (fs);
    const auto s = distortionOnly (0.0);
    for (size_t i = 0; i < ddspec::expansionLevels.size(); ++i)
    {
        CAPTURE (ddspec::expansionLevels[i]);
        CHECK (expansionIncrease (s, fs, ddspec::expansionLevels[i]) == Approx (ddspec::expansionGains[i]).margin (0.4));
    }
    // below -40 dBFS the gain is constant
    for (double level : { -70.0, -60.0, -50.0, -45.0, -41.0 })
        CHECK (expansionIncrease (s, fs, level) == Approx (0.0).margin (0.02));
}

TEST_CASE ("Expansion curve continues monotonically above the table, slope <= 0.4 dB/dB, cap +8 dB", "[expansion]")
{
    const auto& c = expansion::defaultCurve();
    double prev = c.gainDb (-80.0);
    for (double l = -79.9; l <= 12.0; l += 0.1)
    {
        const double g = c.gainDb (l);
        CHECK (g >= prev - 1e-12);
        if (l > c.knots.back().levelDb)
            CHECK ((g - prev) / 0.1 <= 0.4 + 1e-9);
        CHECK (g <= 8.0);
        prev = g;
    }
    CHECK (c.gainDb (6.0) == Approx (8.0));
}

TEST_CASE ("Detector sits after Input gain and before the tone EQ", "[expansion]")
{
    const double fs = 48000.0;
    // EQ settings do not move the expansion onset
    auto flat = distortionOnly (0.0);
    auto eq = flat;
    eq.bass = -50.0;
    eq.treble = 30.0;
    eq.mid = -40.0;
    eq.midFreq = 900.0;
    for (double level : { -36.0, -30.0, -24.0 })
        CHECK (expansionIncrease (eq, fs, level) == Approx (expansionIncrease (flat, fs, level)).margin (0.05));
    // Input gain moves it 1:1
    auto in6 = flat;
    in6.inputDb = 6.0;
    for (double level : { -40.0, -34.0, -28.0 })
    {
        const double viaInput = measureTone (in6, fs, 1000.0, level, 0.4, 0.2).gainDb - measureTone (in6, fs, 1000.0, -56.0, 0.4, 0.2).gainDb;
        CHECK (viaInput == Approx (expansionIncrease (flat, fs, level + 6.0)).margin (0.05));
    }
}

TEST_CASE ("Tone EQ, Drive and expansion act before the clipper; HP and LP after it", "[clipper][eq]")
{
    const double fs = 48000.0;
    // Bass -50 prevents a 50 Hz tone from clipping
    // (Drive 0 %, -10 dBFS: section 6.1 shows this tone clipped with the EQ flat)
    auto s = distortionOnly (0.0);
    const auto flat = measureTone (s, fs, 50.0, -10.0, 0.6, 0.4);
    s.bass = -50.0;
    const auto cut = measureTone (s, fs, 50.0, -10.0, 0.6, 0.4);
    CHECK (flat.h3Dbc > -15.0);
    CHECK (cut.h3Dbc < flat.h3Dbc - 10.0);
    // unclipped: relative to its small-signal gain the cut tone shows the expansion, the flat one is clipped down
    CHECK (cut.gainDb - smallSignalDb (s, fs, 50.0) > 3.0);
    CHECK (flat.gainDb - smallSignalDb (distortionOnly (0.0), fs, 50.0) < cut.gainDb - smallSignalDb (s, fs, 50.0) - 3.0);

    // LP 1 kHz still clips a 2 kHz tone: the fundamental sits at the clip ceiling times the LP response and
    // the third harmonic is present (attenuated by the LP only)
    auto lp = distortionOnly (50.0);
    lp.lowPassHz = 1000.0;
    const auto clipped = measureTone (lp, fs, 2000.0, -20.0);
    const auto open = measureTone (distortionOnly (50.0), fs, 2000.0, -20.0);
    CHECK (clipped.gainDb - open.gainDb == Approx (gainToDb (post::lpMagnitude (2000.0, 1000.0))).margin (0.3));
    CHECK (clipped.h3Dbc > -30.0);
    CHECK (clipped.h3Dbc == Approx (open.h3Dbc + gainToDb (post::lpMagnitude (6000.0, 1000.0) / post::lpMagnitude (2000.0, 1000.0))).margin (1.0));
}

TEST_CASE ("Detector is a peak follower that follows low-frequency waveforms with fast Attack", "[expansion]")
{
    const double fs = 48000.0;
    // 50 Hz at -30 dBFS (below the clip): distortion comes from the gain following the waveform
    const double fast = measureTone (distortionOnly (0.0, 0.1, 5.0), fs, 50.0, -30.0, 0.6, 0.4).h3Dbc;
    const double slow = measureTone (distortionOnly (0.0, 100.0, 31.8), fs, 50.0, -30.0, 0.6, 0.4).h3Dbc;
    CHECK (fast > -40.0);
    CHECK (slow < fast - 10.0);
    // a 1 kHz tone sees the steady gain of the section 4.3 curve regardless of Attack (section 6.2 '+990' column)
    const double a1 = measureTone (distortionOnly (0.0, 1.0), fs, 1000.0, -20.0, 1.5, 0.2).gainDb;
    const double a100 = measureTone (distortionOnly (0.0, 100.0), fs, 1000.0, -20.0, 1.5, 0.2).gainDb;
    CHECK (a100 == Approx (a1).margin (0.1));
}

TEST_CASE ("Envelope step trajectories match section 6.2", "[envelope]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const std::vector<double> upMs (ddspec::stepUpMs.begin(), ddspec::stepUpMs.end());
    const std::vector<double> downMs (ddspec::stepDownMs.begin(), ddspec::stepDownMs.end());
    for (const auto& row : ddspec::stepRows)
    {
        // Release 3000: precede the step with >= 5 s at -50 dBFS; the 'up' columns then equal the A 1 / R 31.8
        // row (the table's values reflect the reference's preceding programme, see the note under section 6.2).
        const bool longRelease = row.release > 1000.0;
        const auto tr = stepResponse (distortionOnly (0.0, row.attack, row.release), fs, upMs, downMs, longRelease ? 6.0 : 1.0);
        CAPTURE (fs, row.attack, row.release);
        for (size_t i = 0; i < upMs.size(); ++i)
        {
            const double target = longRelease ? ddspec::stepRows[1].v[i] : row.v[i];
            CAPTURE (upMs[i]);
            CHECK (tr.up[i] == Approx (target).margin (ddspec::stepTolerance));
        }
        for (size_t i = 1; i < downMs.size(); ++i) // '+10 ms after the drop' is informative only
        {
            CAPTURE (downMs[i]);
            CHECK (tr.down[i] == Approx (row.v[7 + i]).margin (ddspec::stepTolerance));
        }
    }
}
