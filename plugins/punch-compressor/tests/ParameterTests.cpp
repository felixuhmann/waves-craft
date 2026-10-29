// Section 2: parameter maps, ratio table, release-note table, display formats.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Parameters.h"

using namespace craft::punch;
using Catch::Approx;

TEST_CASE ("Threshold, Mix, Punch and Output maps", "[params]")
{
    CHECK (param::Threshold::fromNorm (0.0) == 0.0);
    CHECK (param::Threshold::fromNorm (1.0) == -48.0);
    CHECK (param::Threshold::fromNorm (0.375) == Approx (-18.0));
    CHECK (param::Threshold::toNorm (-18.0) == Approx (0.375));
    CHECK (param::Mix::fromNorm (0.5) == Approx (50.0));
    CHECK (param::Punch::fromNorm (1.0) == Approx (30.0));
    CHECK (param::Punch::fromNorm (1.0 / 30.0) == Approx (1.0));
    CHECK (param::Output::fromNorm (0.0) == Approx (-18.0));
    CHECK (param::Output::fromNorm (0.5) == Approx (0.0));
    CHECK (param::Output::fromNorm (1.0) == Approx (18.0));
    for (double r = 0.0; r <= 1.0; r += 0.01)
    {
        CHECK (param::Threshold::fromNorm (r) == Approx (-48.0 * r).margin (0.05));
        CHECK (param::Output::fromNorm (r) == Approx (-18.0 + 36.0 * r).margin (0.05));
    }
}

TEST_CASE ("Attack and Release are exponential maps shown with 3 significant digits", "[params]")
{
    CHECK (param::Attack::fromNorm (0.0) == Approx (0.5));
    CHECK (param::Attack::fromNorm (1.0) == Approx (100.0));
    CHECK (param::Attack::fromNorm (param::Attack::toNorm (7.0)) == Approx (7.0));
    CHECK (param::Release::fromNorm (0.0) == Approx (3.0));
    CHECK (param::Release::fromNorm (1.0) == Approx (3000.0));
    CHECK (param::Release::fromNorm (param::Release::toNorm (100.0)) == Approx (100.0));
    for (double r = 0.0; r <= 1.0; r += 0.01)
    {
        CHECK (param::Attack::fromNorm (r) == Approx (0.5 * std::pow (200.0, r)).epsilon (0.006));
        CHECK (param::Release::fromNorm (r) == Approx (3.0 * std::pow (1000.0, r)).epsilon (0.006));
    }
    CHECK (param::formatMs (7.0) == "7.00");
    CHECK (param::formatMs (0.5) == "0.500");
    CHECK (param::formatMs (31.62) == "31.6");
    CHECK (param::formatMs (3000.0) == "3000");
}

TEST_CASE ("Ratio follows the 41-point table, log R linear in r", "[params]")
{
    for (int k = 0; k <= 40; ++k)
        CHECK (param::Ratio::fromNorm (k / 40.0) == Approx (param::Ratio::table[size_t (k)]).margin (1.0e-9));
    // midpoint between two table points: geometric mean
    const double mid = std::sqrt (2.92 * 3.24);
    CHECK (param::Ratio::fromNorm (10.5 / 40.0) == Approx (mid).margin (0.005));
    CHECK (param::Ratio::fromNorm (param::Ratio::toNorm (3.0)) == Approx (3.0));
    // monotone
    double prev = 0.0;
    for (double r = 0.0; r <= 1.0; r += 0.001)
    {
        const double v = param::Ratio::continuous (r);
        CHECK (v >= prev);
        prev = v;
    }
    CHECK (param::formatRatio (3.0) == "3.00");
}

TEST_CASE ("Release sync, analog and note value tables", "[params]")
{
    CHECK (param::syncFromNorm (0.0) == param::Sync::Bpm);
    CHECK (param::syncFromNorm (0.249) == param::Sync::Bpm);
    CHECK (param::syncFromNorm (0.25) == param::Sync::Host);
    CHECK (param::syncFromNorm (0.749) == param::Sync::Host);
    CHECK (param::syncFromNorm (0.75) == param::Sync::Ms);
    CHECK (param::analogFromNorm (0.124) == 0);
    CHECK (param::analogFromNorm (0.125) == 1);
    CHECK (param::analogFromNorm (0.5) == 2);
    CHECK (param::analogFromNorm (0.874) == 3);
    CHECK (param::analogFromNorm (0.875) == 4);

    const char* expected[41] = { "1/64T", "1/64T", "1/64", "1/64", "1/64D", "1/64D", "1/32T", "1/32T", "1/32", "1/32",
                                 "1/32D", "1/32D", "1/16T", "1/16T", "1/16", "1/16", "1/16D", "1/16D", "1/8T", "1/8T",
                                 "1/8", "1/8", "1/8", "1/8D", "1/8D", "1/4T", "1/4T", "1/4", "1/4", "1/4D", "1/4D",
                                 "1/2T", "1/2T", "1/2", "1/2", "1/2D", "1/2D", "1B", "1B", "2B", "2B" };
    for (int k = 0; k <= 40; ++k)
    {
        CHECK (std::string (param::note (k).name) == expected[k]);
        // r in [k/40, (k+1)/40) selects entry k
        CHECK (int (param::Note::fromNorm (k / 40.0)) == k);
        if (k < 40)
            CHECK (int (param::Note::fromNorm ((k + 0.999) / 40.0)) == k);
    }
    // default 1/32D
    CHECK (std::string (param::note (int (param::Note::defaultValue)).name) == "1/32D");
    // durations: 1/8 at 120 BPM = 250 ms; T = x2/3, D = x3/2; 1B = 4 beats
    CHECK (param::noteMs (20, 120.0) == Approx (250.0));
    CHECK (param::noteMs (27, 120.0) == Approx (500.0));
    CHECK (param::noteMs (18, 120.0) == Approx (250.0 * 2.0 / 3.0));
    CHECK (param::noteMs (23, 120.0) == Approx (375.0));
    CHECK (param::noteMs (37, 120.0) == Approx (2000.0));
    CHECK (param::noteMs (40, 120.0) == Approx (4000.0));
    CHECK (param::noteMs (0, 120.0) == Approx (500.0 / 16.0 * 2.0 / 3.0));
}

TEST_CASE ("Effective release in the sync modes", "[params]")
{
    Settings s;
    s.releaseMs = 100.0;
    s.note = 20; // 1/8
    s.hostBpm = 120.0;
    s.sync = param::Sync::Ms;
    CHECK (s.effectiveReleaseMs() == Approx (100.0));
    s.sync = param::Sync::Host;
    CHECK (s.effectiveReleaseMs() == Approx (250.0));
    s.sync = param::Sync::Bpm;
    s.manualBpm = 29.0; // Off -> host tempo
    CHECK (s.effectiveReleaseMs() == Approx (250.0));
    s.manualBpm = 60.0;
    CHECK (s.effectiveReleaseMs() == Approx (500.0));
    CHECK (param::formatTempo (29.0) == "Off");
    CHECK (param::formatTempo (120.0) == "120.0");
    CHECK (param::Tempo::fromNorm (0.0) < 30.0);
    CHECK (param::Tempo::fromNorm (1.0) == Approx (300.0));
}
