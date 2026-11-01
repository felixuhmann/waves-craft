// Spec r1 section 3: parameter mappings, ranges, defaults and choice lists.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::mf;
using Catch::Approx;

TEST_CASE ("Freq / comb delay / amp gain share one knob position", "[params]")
{
    CHECK (param::Freq::fromNorm (0.0) == Approx (30.0));
    CHECK (param::Freq::fromNorm (1.0) == Approx (18000.0));
    CHECK (param::Freq::fromNorm (0.5) == Approx (30.0 * std::sqrt (600.0)));
    CHECK (param::Freq::toNorm (param::Freq::fromNorm (0.37)) == Approx (0.37));
    CHECK (param::combDelayMs (0.0) == Approx (11.0));
    CHECK (param::combDelayMs (1.0) == Approx (28.0));
    CHECK (param::ampGainDb (0.0) == Approx (-80.0));
    CHECK (param::ampGainDb (1.0) == Approx (0.0));
    // defaults: 18 kHz = r 1 = 28 ms = 0 dB
    CHECK (param::Freq::toNorm (param::Freq::defaultValue) == Approx (1.0));
}

TEST_CASE ("Linear and exponential parameter ranges", "[params]")
{
    CHECK (param::Resonance::fromNorm (0.5) == Approx (50.0));
    CHECK (param::Spread::fromNorm (0.0) == Approx (-50.0));
    CHECK (param::Spread::fromNorm (1.0) == Approx (50.0));
    CHECK (param::Smooth::defaultValue == Approx (10.0));
    CHECK (param::Gain::fromNorm (0.0) == Approx (-40.0));
    CHECK (param::Gain::fromNorm (1.0) == Approx (10.0));
    CHECK (param::Gain::fromNorm (0.8) == Approx (0.0));
    CHECK (param::Mix::fromNorm (0.0) == Approx (-100.0));
    CHECK (param::Mix::defaultValue == Approx (100.0));
    CHECK (param::Swing::fromNorm (1.0) == Approx (75.0));
    CHECK (param::Feedback::defaultValue == Approx (30.0));
    CHECK (param::DelayMix::defaultValue == Approx (-100.0));

    CHECK (param::LfoRate::fromNorm (0.0) == Approx (0.1));
    CHECK (param::LfoRate::fromNorm (1.0) == Approx (100.0));
    CHECK (param::LfoRate::fromNorm (param::LfoRate::toNorm (4.0)) == Approx (4.0));
    CHECK (param::SeqRate::fromNorm (1.0) == Approx (500.0));
    CHECK (param::SeqRate::fromNorm (0.0) == Approx (0.1));
    CHECK (param::DelayTime::fromNorm (0.0) == Approx (50.0));
    CHECK (param::DelayTime::fromNorm (1.0) == Approx (1000.0));
    CHECK (param::DelayTime::fromNorm (param::DelayTime::toNorm (415.0)) == Approx (415.0));
}

TEST_CASE ("Sync choice lists and defaults", "[params]")
{
    REQUIRE (lfoSyncChoices.size() == 12);
    CHECK (std::string (lfoSyncChoices[lfoSyncDefault].text) == "1/12");
    CHECK (lfoSyncChoices[0].beats() == Approx (16.0));
    CHECK (lfoSyncChoices[3].beats() == Approx (2.0));   // 1/2
    REQUIRE (seqSyncChoices.size() == 10);
    CHECK (std::string (seqSyncChoices[seqSyncDefault].text) == "1/32");
    REQUIRE (delaySyncChoices.size() == 10);
    CHECK (std::string (delaySyncChoices[delaySyncDefault].text) == "1/8");
    CHECK (delayRatioChoices[1] == Approx (1.5));

    // r maps uniformly onto a list (nearest): index = round(r (n - 1))
    auto pick = [] (double r, int n) { return int (std::round (r * (n - 1))); };
    CHECK (pick (0.0, 12) == 0);
    CHECK (pick (1.0, 12) == 11);
    CHECK (pick (0.5, 3) == 1);
}

TEST_CASE ("Settings defaults match the spec", "[params]")
{
    Settings s;
    CHECK (s.type == FilterType::LowPass);
    CHECK_FALSE (s.slope24);
    CHECK (s.freqNorm == Approx (1.0));
    CHECK (s.smooth == Approx (10.0));
    CHECK (s.mix == Approx (100.0));
    CHECK (s.lfoShape == LfoShape::Sine);
    CHECK (s.lfoRateHz == Approx (4.0));
    CHECK (s.lfoSync);
    CHECK (s.lastStep == 16);
    CHECK (s.seqSync);
    CHECK (s.delayMs == Approx (415.0));
    CHECK (s.delaySync);
    CHECK (s.delayFeedback == Approx (30.0));
    CHECK (s.delayMix == Approx (-100.0));
    CHECK_FALSE (s.delayAnalog);
    CHECK_FALSE (s.delayRouteFeedback);
}
