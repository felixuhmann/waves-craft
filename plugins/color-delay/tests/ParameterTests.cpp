// Spec r1 section 2 (mappings, resolution, defaults), section 3a (note lengths), section 6 (Dry/Wet law),
// section 7 (level laws).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "TestHelpers.h"

using namespace craft::cdl;
using Catch::Approx;

TEST_CASE ("Parameter mappings and resolution", "[params]")
{
    namespace p = param;
    CHECK (p::GainDb::fromNorm (0.0) == Approx (-18.0));
    CHECK (p::GainDb::fromNorm (1.0) == Approx (18.0));
    CHECK (p::GainDb::fromNorm (0.5) == Approx (0.0).margin (1e-12));
    CHECK (p::GainDb::fromNorm (0.5 + 0.0013) == Approx (0.0).margin (1e-12)); // 0.0468 dB -> 0.0
    CHECK (p::GainDb::fromNorm (0.5 + 0.0015) == Approx (0.1));

    CHECK (p::DelayMs::fromNorm (0.25) == Approx (500.0));
    CHECK (p::DelayMs::fromNorm (0.0) == Approx (0.0));
    CHECK (p::DelayMs::fromNorm (1.0) == Approx (2000.0));
    CHECK (p::DelayMs::fromNorm (0.0509) == Approx (100.0)); // 101.8 -> 100 (4 ms steps)
    CHECK (p::DelayMs::fromNorm (0.0511) == Approx (104.0));

    CHECK (p::Percent::fromNorm (0.299) == Approx (29.9));
    CHECK (p::Percent::fromNorm (0.12345) == Approx (12.3));

    CHECK (p::FilterHz::fromNorm (0.0) == Approx (16.0));
    CHECK (p::FilterHz::fromNorm (1.0) == Approx (21357.0));
    CHECK (p::FilterHz::fromNorm (p::FilterHz::toNorm (1000.0)) == Approx (1000.0));
    CHECK (p::FilterHz::fromNorm (0.5) == Approx (std::round (16.0 * std::sqrt (21357.0 / 16.0))));

    CHECK (p::VerbSize::fromNorm (0.0) == Approx (1.0));
    CHECK (p::VerbSize::fromNorm (1.0) == Approx (100.0));
    CHECK (p::VerbSize::fromNorm (p::VerbSize::toNorm (54.9)) == Approx (54.9));

    CHECK (p::Detune::fromNorm (0.54) == Approx (8.0));
    CHECK (p::Detune::fromNorm (0.0) == Approx (-100.0));

    CHECK (p::PhaserRate::fromNorm (1.0) == Approx (20.0));
    CHECK (p::PhaserRate::fromNorm (0.0) == Approx (20.0 * std::exp (-8.28)).epsilon (1e-9));
    CHECK (p::PhaserRate::fromNorm (p::PhaserRate::toNorm (0.34)) == Approx (0.34));

    // every quantised value maps back onto itself
    for (int i = 0; i <= 500; ++i)
    {
        const double ms = 4.0 * i;
        REQUIRE (p::DelayMs::fromNorm (p::DelayMs::toNorm (ms)) == Approx (ms));
    }
    for (int i = 0; i <= 1000; ++i)
    {
        const double v = 0.1 * i;
        REQUIRE (p::Percent::fromNorm (p::Percent::toNorm (v)) == Approx (v).margin (1e-9));
    }
}

TEST_CASE ("Note values: 19 steps, ascending, host-sync lengths (spec section 11)", "[params]")
{
    namespace p = param;
    CHECK (p::noteIndexFromNorm (0.0) == 0);
    CHECK (p::noteIndexFromNorm (1.0) == 18);
    CHECK (std::string (p::noteNames[(size_t) p::noteDefaultIndex]) == "1/4");
    for (int i = 1; i < p::numNotes; ++i)
        CHECK (p::noteQuarters (i) > p::noteQuarters (i - 1));

    auto idx = [] (const char* name) {
        for (int i = 0; i < param::numNotes; ++i)
            if (std::string (param::noteNames[(size_t) i]) == name)
                return i;
        return -1;
    };
    CHECK (p::delaySamplesNote (idx ("1/4"), 120.0, 48000.0) == 24000);
    CHECK (p::delaySamplesNote (idx ("1/8D"), 120.0, 48000.0) == 18000);
    CHECK (p::delaySamplesNote (idx ("1/16T"), 120.0, 48000.0) == 3999);
    CHECK (p::delaySamplesNote (idx ("Whole"), 120.0, 48000.0) == 96000);
    // limited to 2.0 s
    CHECK (p::delaySamplesNote (idx ("Whole"), 60.0, 48000.0) == 96000);

    // ms mode: 0 ms = 1 sample, rounding
    CHECK (p::delaySamplesMs (0.0, 48000.0) == 1);
    CHECK (p::delaySamplesMs (100.0, 48000.0) == 4800);
    CHECK (p::delaySamplesMs (2000.0, 44100.0) == 88200);

    // through the engine
    Settings s;
    s.hostSync = true;
    s.noteIndex = { idx ("1/16T"), idx ("Whole") };
    s.delayLink = false;
    ColourDelayEngine e;
    e.setSettings (s);
    e.prepare (48000.0);
    e.setTempo (120.0);
    CHECK (e.getDelaySamples (0) == 3999);
    CHECK (e.getDelaySamples (1) == 96000);
    s.delayLink = true;
    e.setSettings (s);
    CHECK (e.getDelaySamples (1) == 3999);
}

TEST_CASE ("Dry/Wet law (spec section 6)", "[params]")
{
    namespace p = param;
    const double table[21] = { -1000.0, -26.2, -20.0, -17.0, -14.0, -12.2, -10.5, -9.2, -8.0, -7.0, -6.0,
                               -5.2,    -4.4,  -3.8,  -3.1,  -2.5,  -1.9,  -1.4,  -0.9, -0.4, 0.0 };
    CHECK (p::wetGain (0.0) == 0.0);
    CHECK (p::dryGain (100.0) == 0.0);
    CHECK (p::wetGain (100.0) == 1.0);
    CHECK (p::dryGain (0.0) == 1.0);
    for (int i = 1; i <= 20; ++i)
    {
        const double m = 5.0 * i;
        CHECK (20.0 * std::log10 (p::wetGain (m)) == Approx (table[i]).margin (1e-9));
        CHECK (20.0 * std::log10 (p::dryGain (100.0 - m)) == Approx (table[i]).margin (1e-9));
    }
    // linear in dB between grid points, linear in amplitude between 0 and 5 %
    CHECK (20.0 * std::log10 (p::wetGain (52.5)) == Approx (-5.6).margin (1e-9));
    CHECK (p::wetGain (2.5) == Approx (0.5 * std::pow (10.0, -26.2 / 20.0)));
    CHECK (p::wetGain (25.0) == Approx (0.24547).margin (5e-6));
    CHECK (p::dryGain (25.0) == Approx (0.74989).margin (5e-6));
}

TEST_CASE ("Level laws (spec section 7)", "[params]")
{
    namespace p = param;
    CHECK (p::reverbLevelGain (100.0) == 1.0);
    CHECK (p::effectLevelGain (100.0) == 1.0);
    CHECK (p::reverbLevelGain (0.0) == 0.0);
    // measured: reverb -17.5 dB at 50 %, -37 dB at 25 %; others -21 dB at 50 % (tolerance +-1.5 dB)
    CHECK (std::abs (20.0 * std::log10 (p::reverbLevelGain (50.0)) + 17.5) <= 1.5);
    CHECK (std::abs (20.0 * std::log10 (p::reverbLevelGain (25.0)) + 37.0) <= 1.5);
    CHECK (std::abs (20.0 * std::log10 (p::effectLevelGain (50.0)) + 21.0) <= 1.5);
}
