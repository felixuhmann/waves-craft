// Spec section 3: parameter mappings, resolutions and defaults; section 1: 2-voice variant identity.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace testutil;
namespace pm = craft::doubler::param;
using Catch::Approx;

TEST_CASE ("Parameter mappings follow spec section 3", "[params]")
{
    CHECK (pm::MasterGain::fromNorm (0.0) == -24.0);
    CHECK (pm::MasterGain::fromNorm (1.0) == 0.0);
    CHECK (pm::LevelGain::fromNorm (0.5) == -24.0);
    CHECK (pm::Pan::fromNorm (0.25) == -22.5);
    CHECK (pm::EqGain::fromNorm (0.88) == Approx (0.0).margin (1e-12));
    CHECK (pm::EqGain::fromNorm (0.921) == Approx (4.1));
    CHECK (pm::DelayMs::fromNorm (0.094) == Approx (9.4));
    CHECK (pm::Feedback::fromNorm (0.604) == 60.0);
    CHECK (pm::Detune::fromNorm (0.53) == 6.0);
    CHECK (pm::Depth::fromNorm (0.75) == 100.0);

    // frequency taper F(r) = 15.625 * 2^(125 r / 12)
    CHECK (pm::Frequency::fromNorm (0.0) == 16.0);
    CHECK (pm::Frequency::fromNorm (1.0) == 21357.0);
    CHECK (pm::Frequency::fromNorm (0.003) == 16.0);
    CHECK (pm::Frequency::fromNorm (0.258) == 101.0);
    CHECK (pm::Frequency::fromNorm (0.613) == 1306.0);
    CHECK (pm::Frequency::fromNorm (pm::Frequency::toNorm (578.0)) == 578.0);

    // rate 0.1 * 4000^r, 0.1 resolution
    CHECK (pm::Rate::fromNorm (0.0) == Approx (0.1));
    CHECK (pm::Rate::fromNorm (1.0) == Approx (400.0));
    CHECK (pm::Rate::fromNorm (0.278) == Approx (1.0));

    // routing thresholds r < 0.3 / < 0.8 / >= 0.8
    CHECK (pm::Routing::fromNorm (0.29) == 0.0);
    CHECK (pm::Routing::fromNorm (0.3) == 1.0);
    CHECK (pm::Routing::fromNorm (0.79) == 1.0);
    CHECK (pm::Routing::fromNorm (0.8) == 2.0);
    for (double v : { 0.0, 1.0, 2.0 })
        CHECK (pm::Routing::fromNorm (pm::Routing::toNorm (v)) == v);

    // every mapping round-trips through its inverse
    for (double r = 0.0; r <= 1.0; r += 0.01)
    {
        CHECK (pm::LevelGain::fromNorm (pm::LevelGain::toNorm (pm::LevelGain::fromNorm (r))) == pm::LevelGain::fromNorm (r));
        CHECK (pm::Frequency::fromNorm (pm::Frequency::toNorm (pm::Frequency::fromNorm (r))) == pm::Frequency::fromNorm (r));
        CHECK (pm::Rate::fromNorm (pm::Rate::toNorm (pm::Rate::fromNorm (r))) == pm::Rate::fromNorm (r));
    }
}

TEST_CASE ("Defaults of both variants", "[params]")
{
    const auto s4 = defaultSettings (4);
    CHECK (s4.masterDb == 0.0);
    CHECK (s4.fbHighPassHz == 16.0);
    CHECK_FALSE (s4.range20);
    CHECK_FALSE (s4.alignDirect);
    CHECK (s4.directOn);
    CHECK (s4.eqLowGainDb == 0.0);
    CHECK (s4.eqHighGainDb == 4.1);
    CHECK (s4.eqLowFreqHz == 101.0);
    CHECK (s4.eqHighFreqHz == 1306.0);
    const bool on[4] = { true, false, true, false };
    const double gain[4] = { -6, -12, -6, -12 }, pan[4] = { -45, 45, 45, -45 }, delay[4] = { 9.4, 16.0, 23.7, 21.0 },
                 detune[4] = { 6, 10, -6, -10 };
    for (size_t i = 0; i < 4; ++i)
    {
        CHECK (s4.voices[i].on == on[i]);
        CHECK (s4.voices[i].gainDb == gain[i]);
        CHECK (s4.voices[i].panDeg == pan[i]);
        CHECK (s4.voices[i].delayMs == delay[i]);
        CHECK (s4.voices[i].detuneCents == detune[i]);
        CHECK (s4.voices[i].feedbackPct == 0.0);
        CHECK (s4.voices[i].depthCents == 0.0);
        CHECK (s4.voices[i].rateHz == 1.0);
    }
    const auto s2 = defaultSettings (2);
    CHECK (s2.voices[0].on);
    CHECK (s2.voices[1].on);
    CHECK (s2.voices[0].gainDb == -6.0);
    CHECK (s2.voices[1].gainDb == -6.0);
    CHECK (s2.voices[0].panDeg == -45.0);
    CHECK (s2.voices[1].panDeg == 45.0);
    CHECK (s2.voices[0].delayMs == 9.4);
    CHECK (s2.voices[1].delayMs == 23.7);
    CHECK (s2.voices[0].detuneCents == 6.0);
    CHECK (s2.voices[1].detuneCents == -6.0);
}

TEST_CASE ("2-voice engine is bit-identical to the 4-voice engine with voices 3-4 off", "[params]")
{
    const double fs = 44100.0;
    auto s = defaultSettings (2);
    s.voices[0].depthCents = 30;
    s.voices[0].feedbackPct = 40;
    s.eqLowGainDb = -6.0;
    auto e2 = makeEngine (s, fs, 2);
    auto s4 = s;
    s4.voices[2].on = s4.voices[3].on = false;
    auto e4 = makeEngine (s4, fs, 4);

    const auto l = noise (int (fs), 1), r = noise (int (fs), 2);
    const auto o2 = run (e2, l, r), o4 = run (e4, l, r);
    CHECK (o2.l == o4.l);
    CHECK (o2.r == o4.r);
}

TEST_CASE ("No latency is reported", "[params]")
{
    STATIC_REQUIRE (DoublerEngine::latencySamples == 0);
}
