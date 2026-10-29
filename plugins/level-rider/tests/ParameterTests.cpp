// Spec r1 section 2: parameter mappings, defaults and the Range rule.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Parameters.h"

using namespace craft::levelrider::dsp;
using Catch::Approx;

TEST_CASE ("Section 2 mappings and defaults", "[params]")
{
    namespace pm = param;
    CHECK (pm::Target::fromNorm (0.0) == Approx (-42.0));
    CHECK (pm::Target::fromNorm (1.0) == Approx (0.0));
    CHECK (pm::Target::fromNorm (0.5) == Approx (-21.0));
    CHECK (pm::Target::defaultValue == -21.0);

    CHECK (pm::VocalSens::fromNorm (0.0) == Approx (-12.0));
    CHECK (pm::VocalSens::fromNorm (1.0) == Approx (12.0));
    CHECK (pm::VocalSens::defaultValue == 0.0);

    CHECK (pm::MusicSens::isOff (0.0));
    CHECK_FALSE (pm::MusicSens::isOff (0.001));
    CHECK (pm::MusicSens::fromNorm (0.5) == Approx (0.0));
    CHECK (pm::MusicSens::fromNorm (1.0) == Approx (12.0));

    CHECK (pm::RangeMax::fromNorm (0.0) == Approx (-11.0));
    CHECK (pm::RangeMax::fromNorm (1.0) == Approx (12.0));
    CHECK (pm::RangeMax::toNorm (6.0) == Approx (0.739).margin (0.0005));
    CHECK (pm::RangeMin::fromNorm (0.0) == Approx (-12.0));
    CHECK (pm::RangeMin::fromNorm (1.0) == Approx (11.0));
    CHECK (pm::RangeMin::toNorm (-6.0) == Approx (0.261).margin (0.0005));

    CHECK (pm::Idle::fromNorm (0.0) == Approx (-11.5));
    CHECK (pm::Idle::fromNorm (1.0) == Approx (11.5));
    CHECK (pm::Idle::fromNorm (0.5) == Approx (0.0));
    CHECK (pm::Rider::fromNorm (0.0) == Approx (-12.0));
    CHECK (pm::Rider::fromNorm (1.0) == Approx (12.0));
    CHECK (pm::Output::fromNorm (0.75) == Approx (6.0));
    CHECK (pm::Spill::fromNorm (0.0) == Approx (-48.0));
    CHECK (pm::Spill::fromNorm (1.0) == Approx (-12.0));

    CHECK_FALSE (pm::attackIsFast (0.49));
    CHECK (pm::attackIsFast (0.5));

    // 0.1 dB display resolution
    CHECK (pm::Target::fromNorm (0.123456) == Approx (-36.8));
}

TEST_CASE ("Range Min stays 1 dB below Range Max; the last changed control wins", "[params]")
{
    auto r = resolveRange (-6.0, 6.0, true);
    CHECK (r.minDb == -6.0);
    CHECK (r.maxDb == 6.0);

    r = resolveRange (3.0, 2.0, true); // Max moved below Min: Min is pushed down
    CHECK (r.maxDb == Approx (2.0));
    CHECK (r.minDb == Approx (1.0));

    r = resolveRange (3.0, 2.0, false); // Min moved above Max: Max is pushed up
    CHECK (r.minDb == Approx (3.0));
    CHECK (r.maxDb == Approx (4.0));

    r = resolveRange (11.0, -11.0, false); // extremes stay reachable
    CHECK (r.maxDb == Approx (12.0));
    r = resolveRange (11.0, -11.0, true);
    CHECK (r.minDb == Approx (-12.0));

    r = resolveRange (0.5, 1.0, true); // closer than 1 dB
    CHECK (r.maxDb - r.minDb == Approx (1.0));
}
