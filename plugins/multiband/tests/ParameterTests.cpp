// spec r2 section 2: parameter mappings, defaults and display formats.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Parameters.h"

using namespace craft::multiband;
using Catch::Approx;

TEST_CASE ("Mappings of section 2", "[parameters]")
{
    CHECK (param::OutputGain::fromNorm (0.0) == -18.0);
    CHECK (param::OutputGain::fromNorm (1.0) == 18.0);
    CHECK (param::OutputGain::fromNorm (0.5) == 0.0);
    CHECK (param::BandRange::fromNorm (0.0) == -24.0);
    CHECK (param::BandRange::fromNorm (1.0) == 18.0);
    CHECK (param::BandThreshold::fromNorm (0.0) == -80.0);
    CHECK (param::BandThreshold::fromNorm (1.0) == 0.0);
    CHECK (param::Crossover::fromNorm (0.0) == 16.0);
    CHECK (param::Crossover::fromNorm (1.0) == 21357.0);
    CHECK (param::Crossover::fromNorm (0.5) == Approx (std::round (16.0 * std::sqrt (21357.0 / 16.0))));
    CHECK (param::CrossoverQ::fromNorm (0.0) == Approx (0.10));
    CHECK (param::CrossoverQ::fromNorm (1.0) == Approx (0.75));
    CHECK (param::Knee::fromNorm (2.0 / 3.0) == Approx (0.50));
    CHECK (param::Attack::fromNorm (0.0) == Approx (0.5));
    CHECK (param::Attack::fromNorm (1.0) == Approx (500.0));
    CHECK (param::Release::fromNorm (0.0) == Approx (5.0));
    CHECK (param::Release::fromNorm (1.0) == Approx (5000.0));
    CHECK (param::Release::fromNorm (0.5) == Approx (158.11));
}

TEST_CASE ("Defaults are reachable values of the mappings", "[parameters]")
{
    for (double f : param::defaultCrossover)
        CHECK (param::snap<param::Crossover> (f) == Approx (f).margin (1.0));
    for (double t : param::defaultThreshold)
        CHECK (param::snap<param::BandThreshold> (t) == Approx (t));
    for (double a : param::defaultAttack)
        CHECK (param::snap<param::Attack> (a) == Approx (a).margin (0.011));
    for (double r : param::defaultRelease)
        CHECK (param::snap<param::Release> (r) == Approx (r).margin (0.011));
    CHECK (param::snap<param::BandRange> (-8.0) == Approx (-8.0));
    CHECK (param::snap<param::Knee> (0.5) == Approx (0.5));
    CHECK (param::snap<param::CrossoverQ> (0.6) == Approx (0.6));
}

TEST_CASE ("Inverse mappings round-trip", "[parameters]")
{
    for (double r = 0.0; r <= 1.0; r += 0.01)
    {
        CHECK (param::Attack::toNorm (param::Attack::fromNorm (r)) == Approx (r).margin (2e-3));
        CHECK (param::Release::toNorm (param::Release::fromNorm (r)) == Approx (r).margin (2e-3));
        CHECK (param::Crossover::fromNorm (param::Crossover::toNorm (param::Crossover::fromNorm (r))) == param::Crossover::fromNorm (r));
        CHECK (param::BandRange::toNorm (param::BandRange::fromNorm (r)) == Approx (r).margin (2e-3));
    }
}

TEST_CASE ("Display formats", "[parameters]")
{
    CHECK (param::formatDb (-8.0) == "-8.0");
    CHECK (param::formatDb (-0.04) == "0.0");
    CHECK (param::formatHz (11071.2) == "11071");
    CHECK (param::formatMs (34.99) == "34.99");
    CHECK (param::formatQ (0.6) == "0.60");
    CHECK (param::formatKnee (0.0) == "Soft");
    CHECK (param::formatKnee (0.75) == "Hard");
    CHECK (param::formatKnee (0.5) == "0.50");
}
