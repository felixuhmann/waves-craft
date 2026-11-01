// Parameter mappings, defaults and display strings: spec r1 section 2.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace mltest;
using Catch::Approx;
namespace pm = dsp::param;

TEST_CASE ("Threshold / Out Ceiling: -30 + 30 r dB, 0.1 dB steps (spec 2)")
{
    CHECK (pm::Level::fromNorm (0.0) == -30.0);
    CHECK (pm::Level::fromNorm (1.0) == 0.0);
    CHECK (pm::Level::fromNorm (0.6) == -12.0);
    CHECK (pm::Level::fromNorm (0.9) == -3.0);
    CHECK (pm::Level::fromNorm (0.123) == Approx (-26.3).margin (1e-12));
    CHECK (pm::Level::toNorm (-12.0) == Approx (0.6));
    CHECK (pm::Level::snap (-12.04) == -12.0);
    CHECK (pm::Level::snap (-12.0) == -12.0); // grid values are exact
    CHECK (dsp::formatDb (-12.0) == "-12.0 dB");
    CHECK (dsp::formatDb (0.0) == "0.0 dB");
    CHECK (dsp::formatDb (-0.04) == "0.0 dB");
}

TEST_CASE ("Release: 0.01 * 10^(5 r) ms, default 1.00 ms at r = 0.4 (spec 2)")
{
    CHECK (pm::Release::fromNorm (0.0) == Approx (0.01));
    CHECK (pm::Release::fromNorm (1.0) == Approx (1000.0));
    CHECK (pm::Release::fromNorm (0.4) == Approx (1.0));
    CHECK (pm::Release::toNorm (1.0) == Approx (0.4));
    CHECK (pm::Release::fromNorm (0.5) == Approx (3.16));
    CHECK (pm::Release::fromNorm (0.7) == Approx (31.62));
    CHECK (dsp::formatMs (1.0) == "1.00 ms");
    CHECK (dsp::formatMs (316.23) == "316.23 ms");
}

TEST_CASE ("Word length steps and defaults (spec 2, D5)")
{
    CHECK (dsp::wordLengthForIndex (0) == 24);
    CHECK (dsp::wordLengthForIndex (1) == 22);
    CHECK (dsp::wordLengthForIndex (2) == 20);
    CHECK (dsp::wordLengthForIndex (3) == 18);
    CHECK (dsp::wordLengthForIndex (4) == 16);
    const dsp::Settings s;
    CHECK (s.thresholdDb == 0.0);
    CHECK (s.ceilingDb == 0.0);
    CHECK (s.releaseMs == 1.0);
    CHECK (s.adaptiveRelease);
    CHECK (s.wordLength == 24);
    CHECK (s.dither == dsp::Dither::Type1);
    CHECK (s.shaping == dsp::Shaping::Normal);
}

TEST_CASE ("Number parsing for typed values")
{
    CHECK (dsp::parseNumber ("-12", 0.0) == -12.0);
    CHECK (dsp::parseNumber (" -3.5 dB", 0.0) == -3.5);
    CHECK (dsp::parseNumber ("2,25 ms", 0.0) == 2.25);
    CHECK (dsp::parseNumber ("abc", 7.0) == 7.0);
}

TEST_CASE ("Default settings dither the output (D5) and report the latency")
{
    dsp::MasteringEngine e;
    e.prepare (48000.0);
    CHECK (e.getLatencySamples() == 64);
    std::vector<double> x (4096, 0.0);
    const auto y = runEngine (e, { x })[0];
    double energy = 0.0;
    for (double v : y)
        energy += v * v;
    CHECK (energy > 0.0); // Type 1 dither + Normal shaping at 24 bit
}
