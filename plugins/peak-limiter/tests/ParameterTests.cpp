// Spec r1 section 2: parameter mappings, resolution, defaults and display formats.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Parameters.h"

using namespace craft::limiter;
using Catch::Approx;

TEST_CASE ("Threshold / Ceiling mapping", "[params]")
{
    CHECK (param::Threshold::fromNorm (0.0) == -30.0);
    CHECK (param::Threshold::fromNorm (1.0) == 0.0);
    CHECK (param::Threshold::fromNorm (0.6) == -12.0);
    CHECK (param::Threshold::fromNorm (0.5) == -15.0);
    CHECK (param::Ceiling::fromNorm (0.9) == -3.0);
    CHECK (param::Threshold::defaultValue == 0.0);
    CHECK (param::Ceiling::defaultValue == 0.0);
    // 0.1 dB resolution
    CHECK (param::Threshold::fromNorm (0.123456) == Approx (-26.3).margin (1e-12));
    for (int step = 0; step <= 300; ++step)
    {
        const double v = -30.0 + step / 10.0;
        REQUIRE (param::Threshold::fromNorm (param::Threshold::toNorm (v)) == Approx (v).margin (1e-12));
    }
}

TEST_CASE ("Release mapping: 0.01 * 10^(5 r) ms, 2-decimal internal value", "[params]")
{
    CHECK (param::Release::fromNorm (0.0) == 0.01);
    CHECK (param::Release::fromNorm (1.0) == 1000.0);
    CHECK (param::Release::fromNorm (0.4) == 1.0); // default
    CHECK (param::Release::toNorm (param::Release::defaultValue) == Approx (0.4).margin (1e-12));
    CHECK (param::Release::fromNorm (0.7) == Approx (31.62).margin (1e-12));
    CHECK (param::Release::fromNorm (0.9) == Approx (316.23).margin (1e-12));
    CHECK (param::Release::fromNorm (0.1) == Approx (0.03).margin (1e-12));
    // every value is a multiple of 0.01 ms
    for (int i = 0; i <= 1000; ++i)
    {
        const double v = param::Release::fromNorm (i / 1000.0);
        REQUIRE (std::abs (v * 100.0 - std::round (v * 100.0)) < 1e-9);
        REQUIRE (v >= 0.01);
        REQUIRE (v <= 1000.0);
    }
}

TEST_CASE ("Display formats", "[params]")
{
    CHECK (param::formatDb (-12.0) == "-12.0");
    CHECK (param::formatDb (0.0) == "0.0");
    CHECK (param::formatDb (-0.04) == "0.0");
    CHECK (param::formatReleaseMs (0.03) == "0.03");
    CHECK (param::formatReleaseMs (316.23) == "316.23");
    CHECK (param::formatReleaseMs (1000.0) == "1000");
    CHECK (param::formatReleaseMs (1.0) == "1.00");
}
