// Parameter mappings and display strings (spec 2).
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

TEST_CASE ("Reference mappings and defaults", "[params]")
{
    namespace pm = param;
    CHECK (pm::CompThreshold::fromNorm (0.0) == -100.0);
    CHECK (pm::CompThreshold::fromNorm (0.6) == Approx (-40.0));
    CHECK (pm::Attack::fromNorm (0.0) == Approx (0.01));
    CHECK (pm::Attack::fromNorm (1.0) == Approx (1000.0));
    CHECK (pm::Attack::fromNorm (pm::Attack::toNorm (2.0)) == Approx (2.0));
    CHECK (pm::CompRelease::fromNorm (0.5) == Approx (100.0));
    CHECK (pm::Hold::fromNorm (1.0) == Approx (5000.0));
    CHECK (pm::Makeup::fromNorm (0.75) == Approx (20.0));
    CHECK (pm::OutputGain::fromNorm (1.0) == Approx (18.0));
    CHECK (pm::FilterFreq::fromNorm (0.0) == Approx (16.0));
    CHECK (pm::FilterFreq::fromNorm (1.0) == Approx (21357.0).margin (1.0));
    CHECK (pm::FilterFreq::fromNorm (pm::FilterFreq::toNorm (7179.0)) == 7179.0);
    CHECK (pm::FilterQ::fromNorm (0.5) == Approx (0.35));
    const double pdr[11] = { 0, 1, 2, 5, 10, 22, 47, 102, 218, 467, 1000 };
    for (int i = 0; i <= 10; ++i)
        CHECK (pm::Pdr::fromNorm (i / 10.0) == Approx (pdr[i]));
    CHECK (pm::Pdr::fromNorm (pm::Pdr::toNorm (22.0)) == Approx (22.0));
}

TEST_CASE ("Display strings", "[params]")
{
    CHECK (formatDisplay (Quantity::GateThreshold, -100.0) == "-inf");
    CHECK (formatDisplay (Quantity::GateThreshold, -40.0) == "-40.0 dB");
    CHECK (formatDisplay (Quantity::Ratio, 0.5) == "4.58:1");
    CHECK (formatDisplay (Quantity::Ratio, 1.0) == "-5.00:1");
    CHECK (formatDisplay (Quantity::Ratio, double (0.8f)) == "50.00:1"); // r2 2: r = 0.8 itself is 50:1
    CHECK (formatDisplay (Quantity::Ratio, 0.8) == "50.00:1");
    CHECK (formatDisplay (Quantity::Time, 2.0) == "2.00 ms");
    CHECK (formatDisplay (Quantity::Time, 50.0) == "50.0 ms");
    CHECK (formatDisplay (Quantity::Time, 2500.0) == "2.50 s");
    CHECK (formatDisplay (Quantity::Pdr, 0.0) == "Off");
    CHECK (formatDisplay (Quantity::Floor, floorvalue::offValue) == "-inf");
    CHECK (formatDisplay (Quantity::Floor, -27.4) == "-27.4 dB");
    CHECK (formatDisplay (Quantity::Floor, -200.0) == "-10 N");
    CHECK (formatDisplay (Quantity::Floor, -110.0) == "-100 N");
    CHECK (formatDisplay (Quantity::Frequency, 7179.0) == "7179 Hz");
    CHECK (parseDisplay (Quantity::Floor, "-30 N") == Approx (-180.0));
    CHECK (parseDisplay (Quantity::Floor, "-inf") == floorvalue::offValue);
    CHECK (parseDisplay (Quantity::Time, "1.5 s") == Approx (1500.0));
    CHECK (parseDisplay (Quantity::Ratio, "4.58:1") == Approx (0.5).margin (1.0e-4));
    CHECK (parseDisplay (Quantity::Ratio, "-5") == Approx (1.0).margin (1.0e-4));
}

TEST_CASE ("Floor values", "[params][floor]")
{
    CHECK (floorvalue::linear (floorvalue::offValue, true) == 0.0);
    CHECK (floorvalue::linear (-20.0, false) == Approx (0.1));
    CHECK (floorvalue::linear (-200.0, true) == Approx (-std::pow (10.0, -0.5)));
    CHECK (floorvalue::linear (-200.0, false) == 0.0);
    namespace pm = param;
    CHECK (pm::Floor::fromNorm (0.175) == floorvalue::offValue);
    CHECK (pm::Floor::fromNorm (1.0) == Approx (12.0));
    CHECK (pm::Floor::fromNorm (0.0) == Approx (-200.0));
    for (double v : { -200.0, -150.0, -110.0, -100.0, -27.4, 0.0, 12.0 })
        CHECK (pm::Floor::fromNorm (pm::Floor::toNorm (v)) == Approx (v).margin (0.05));
}
