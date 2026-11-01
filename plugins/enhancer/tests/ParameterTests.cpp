// Spec r1 section 1: parameter mappings.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace enh_test;
using Catch::Approx;

TEST_CASE ("gain mappings", "[param]")
{
    CHECK (param::Gain::fromNorm (0.0) == -18.0);
    CHECK (param::Gain::fromNorm (0.5) == 0.0);
    CHECK (param::Gain::fromNorm (1.0) == 18.0);
    CHECK (param::Direct::isOff (param::Direct::fromNorm (0.0)));
    CHECK (param::Direct::fromNorm (0.025) == Approx (-46.8));
    CHECK (param::Direct::fromNorm (1.0) == 0.0);
    CHECK (param::BandGain::isOff (param::BandGain::fromNorm (0.0)));
    CHECK (param::BandGain::fromNorm (0.025) == Approx (1.2));
    CHECK (param::BandGain::fromNorm (0.5) == 24.0);
    CHECK (param::BandGain::fromNorm (1.0) == 48.0);
}

TEST_CASE ("crossover taper", "[param]")
{
    CHECK (param::Crossover::fromNorm (0.0) == 16.0);
    CHECK (param::Crossover::fromNorm (0.5) == 578.0);
    CHECK (param::Crossover::fromNorm (1.0) == 21357.0);
    for (double f : { 90.0, 500.0, 2594.0, 9310.0 })
        CHECK (param::Crossover::fromNorm (param::Crossover::toNorm (f)) == Approx (f).margin (1.0));
    const auto o = orderedCrossovers ({ 1000.0, 500.0, 2000.0, 1500.0 });
    CHECK (o[1] == 1000.0);
    CHECK (o[3] == 2000.0);
}

TEST_CASE ("width and punch", "[param]")
{
    CHECK (param::Width::fromNorm (0.5) == 1.0);
    CHECK (param::Width::fromNorm (1.0) == 3.0);
    CHECK (param::Width::toNorm (1.0) == Approx (0.5));
    CHECK (param::Punch::fromNorm (0.6) == 3.0);
    CHECK (param::Punch::toNorm (5.0) == 1.0);
}

TEST_CASE ("display strings", "[param]")
{
    CHECK (formatDb (0.0) == "0.0dB");
    CHECK (formatDirect (-48.0) == "Off");
    CHECK (formatBandGain (0.0) == "Off");
    CHECK (parseNumber ("off", 1.0, -48.0) == -48.0);
    CHECK (parseNumber ("2.5k", 1.0, 0.0) == 2500.0);
}
