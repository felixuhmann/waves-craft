// Spec r1 section 3 (parameter mappings), 4.1 (level laws) and 4.2 (drive table).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "SpecTables.h"
#include "TestHelpers.h"

using namespace ddtest;
using Catch::Approx;

TEST_CASE ("Parameter mappings follow section 3", "[params]")
{
    namespace pm = param;
    for (double r : { 0.0, 0.1, 0.25, 0.5, 0.77, 1.0 })
    {
        CHECK (pm::Input::fromNorm (r) == Approx (-18.0 + 36.0 * r));
        CHECK (pm::Output::fromNorm (r) == Approx (-18.0 + 36.0 * r));
        CHECK (pm::DirectLevel::fromNorm (r) == Approx (100.0 * r));
        CHECK (pm::Drive::fromNorm (r) == Approx (100.0 * r));
        CHECK (pm::DistLevel::fromNorm (r) == Approx (100.0 * r));
        CHECK (pm::Attack::fromNorm (r) == Approx (0.1 * std::pow (5000.0, r)));
        CHECK (pm::Release::fromNorm (r) == Approx (5.0 * std::pow (1000.0, r)));
        CHECK (pm::Bass::fromNorm (r) == Approx (-50.0 + 100.0 * r));
        CHECK (pm::Mid::fromNorm (r) == Approx (-50.0 + 100.0 * r));
        CHECK (pm::Treble::fromNorm (r) == Approx (-50.0 + 100.0 * r));
        CHECK (pm::MidFreq::fromNorm (r) == Approx (150.0 * std::pow (100.0 / 3.0, r)));
        CHECK (pm::LowPass::fromNorm (r) == Approx (500.0 * std::pow (42.0, r)));
        // round trip
        CHECK (pm::Attack::toNorm (pm::Attack::fromNorm (r)) == Approx (r).margin (1e-12));
        CHECK (pm::Release::toNorm (pm::Release::fromNorm (r)) == Approx (r).margin (1e-12));
        CHECK (pm::MidFreq::toNorm (pm::MidFreq::fromNorm (r)) == Approx (r).margin (1e-12));
        CHECK (pm::LowPass::toNorm (pm::LowPass::fromNorm (r)) == Approx (r).margin (1e-12));
        CHECK (pm::Input::toNorm (pm::Input::fromNorm (r)) == Approx (r).margin (1e-12));
    }
    // ranges
    CHECK (pm::Attack::fromNorm (1.0) == Approx (500.0));
    CHECK (pm::Release::fromNorm (1.0) == Approx (5000.0));
    CHECK (pm::MidFreq::fromNorm (1.0) == Approx (5000.0));
    CHECK (pm::LowPass::fromNorm (1.0) == Approx (21000.0));
}

TEST_CASE ("Parameter defaults follow section 3", "[params]")
{
    const Settings s;
    CHECK (s.inputDb == 0.0);
    CHECK (s.directPercent == 100.0);
    CHECK (s.drivePercent == 50.0);
    CHECK (s.distPercent == 62.0);
    CHECK (s.attackMs == Approx (1.0));
    CHECK (s.releaseMs == Approx (31.8));
    CHECK (s.bass == -50.0);
    CHECK (s.mid == 0.0);
    CHECK (s.midFreq == Approx (1252.0));
    CHECK (s.treble == -15.0);
    CHECK (s.lowPassHz == Approx (13015.0));
    CHECK (s.outputDb == 0.0);
    // the defaults lie inside the mapped ranges (host default positions)
    CHECK (param::Attack::toNorm (1.0) == Approx (std::log (10.0) / std::log (5000.0)));
    CHECK (param::Release::fromNorm (param::Release::toNorm (31.8)) == Approx (31.8));
}

TEST_CASE ("Direct and Distortion level laws match section 4.1", "[laws]")
{
    for (const auto& row : laws::directTable)
        CHECK (gainToDb (laws::directGain (row.x)) == Approx (row.db).margin (0.05));
    for (const auto& row : laws::distortionTable)
        CHECK (gainToDb (laws::distortionGain (row.x)) == Approx (row.db).margin (0.05));
    CHECK (laws::directGain (0.0) == 0.0);
    CHECK (laws::distortionGain (0.0) == 0.0);

    // exactly linear in dB between 50 % and 100 %: 0.24 dB/% and 0.36 dB/%
    for (double p = 50.0; p <= 100.0; p += 2.5)
    {
        CHECK (gainToDb (laws::directGain (p)) == Approx (-0.24 * (100.0 - p)).margin (0.05));
        CHECK (gainToDb (laws::distortionGain (p)) == Approx (-0.36 * (100.0 - p)).margin (0.05));
    }
    // linear interpolation in dB between rows
    CHECK (gainToDb (laws::directGain (15.0)) == Approx (-37.0).margin (0.05));
    CHECK (gainToDb (laws::distortionGain (7.5)) == Approx (-47.5).margin (0.05));
    // monotonic, including the fade-in below 5 %
    double prevD = 0.0, prevL = 0.0;
    for (double p = 0.0; p <= 100.0; p += 0.5)
    {
        CHECK (laws::directGain (p) >= prevD);
        CHECK (laws::distortionGain (p) >= prevL);
        prevD = laws::directGain (p);
        prevL = laws::distortionGain (p);
    }
}

TEST_CASE ("Direct level law measured through the processor", "[laws][engine]")
{
    // Direct path = input after Input gain times the Direct law; distortion path off.
    for (const auto& row : laws::directTable)
    {
        Settings s = neutralSettings();
        s.distPercent = 0.0;
        s.directPercent = row.x;
        CHECK (smallSignalDb (s, 48000.0, 1000.0, -20.0) == Approx (row.db).margin (0.05));
    }
    Settings s = neutralSettings();
    s.distPercent = 0.0;
    s.directPercent = 0.0;
    CHECK (smallSignalDb (s, 48000.0, 1000.0, -20.0) < -200.0);
}

TEST_CASE ("Distortion level law measured through the processor", "[laws][engine]")
{
    // Distortion path relative to Distortion Level 100 % (Drive 0 %, small signal).
    auto s = distortionOnly (0.0);
    const double ref = smallSignalDb (s, 48000.0, 1000.0);
    for (const auto& row : laws::distortionTable)
    {
        s.distPercent = row.x;
        CHECK (smallSignalDb (s, 48000.0, 1000.0) - ref == Approx (row.db).margin (0.05));
    }
}

TEST_CASE ("Drive table: small-signal gain at 1 kHz matches section 4.2", "[drive][engine]")
{
    for (const auto& row : laws::driveTable)
    {
        CAPTURE (row.x);
        CHECK (smallSignalDb (distortionOnly (row.x), 48000.0, 1000.0) == Approx (row.db).margin (0.1));
    }
    // interpolation between rows (linear in dB)
    CHECK (smallSignalDb (distortionOnly (72.5), 48000.0, 1000.0) == Approx (33.455).margin (0.1));
    CHECK (smallSignalDb (distortionOnly (2.5), 48000.0, 1000.0) == Approx (-1.7).margin (0.1));
}

TEST_CASE ("Input and Output gains are exact dB gains", "[laws][engine]")
{
    for (double g : { -18.0, -6.5, 0.0, 7.3, 18.0 })
    {
        Settings s = neutralSettings();
        s.distPercent = 0.0;
        s.inputDb = g;
        CHECK (smallSignalDb (s, 48000.0, 1000.0, -30.0) == Approx (g).margin (0.01));
        s.inputDb = 0.0;
        s.outputDb = g;
        CHECK (smallSignalDb (s, 48000.0, 1000.0, -30.0) == Approx (g).margin (0.01));
    }
}
