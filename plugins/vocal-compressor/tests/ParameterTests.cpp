#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/Parameters.h"

using namespace craft::vocal;
using Catch::Matchers::WithinAbs;

TEST_CASE ("Compression and Gain map -36 + 36 r dB in 0.1 dB steps", "[parameters]")
{
    CHECK (param::Compression::fromNorm (0.0) == -36.0);
    CHECK (param::Compression::fromNorm (1.0) == 0.0);
    CHECK (param::Compression::fromNorm (0.5) == -18.0);
    CHECK_THAT (param::Compression::fromNorm (0.123), WithinAbs (-31.6, 1e-12));
    CHECK (param::Gain::fromNorm (2.0 / 3.0) == -12.0);
    CHECK (param::Compression::defaultValue == 0.0);
    CHECK (param::Gain::defaultValue == 0.0);
    for (double v = -36.0; v <= 0.0; v += 0.1)
        REQUIRE_THAT (param::snap<param::Compression> (v), WithinAbs (std::round (v * 10.0) / 10.0, 1e-9));
}

TEST_CASE ("Gate: r = 0 is off, otherwise -80 + 80 r dB", "[parameters]")
{
    CHECK (param::Gate::isOff (param::Gate::fromNorm (0.0)));
    CHECK (param::Gate::defaultValue == param::Gate::offValue);
    CHECK (param::Gate::fromNorm (1.0) == 0.0);
    CHECK (param::Gate::fromNorm (0.5) == -40.0);
    CHECK (param::Gate::fromNorm (0.025) == -78.0);
    CHECK_FALSE (param::Gate::isOff (param::Gate::fromNorm (0.0001)));
    CHECK (param::Gate::toNorm (param::Gate::offValue) == 0.0);
}

TEST_CASE ("Display formats", "[parameters]")
{
    CHECK (param::formatDb (-12.34) == "-12.3");
    CHECK (param::formatDb (-0.01) == "0.0");
    CHECK (param::formatDb (0.0) == "0.0");
    CHECK (param::formatGate (param::Gate::offValue) == "-Inf");
    CHECK (param::formatGate (-40.0) == "-40.0");
    CHECK (param::parseGate ("-inf") == param::Gate::offValue);
    CHECK (param::parseGate ("off") == param::Gate::offValue);
    CHECK (param::parseGate (" -42.04 ") == -42.0);
    CHECK (param::parseGate ("-200") == param::Gate::offValue);
}
