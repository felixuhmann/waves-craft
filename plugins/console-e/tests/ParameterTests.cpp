// Spec r1 section 2: control tapers, ratio lookup and defaults.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::consolee::test;
using Catch::Approx;

TEST_CASE ("Tapers hit the section 2 knots and are linear in between", "[parameters]")
{
    const double r[] = { 0.0, 0.2, 0.5, 0.8, 1.0 };
    struct T
    {
        const param::Taper& t;
        double v[5];
    };
    const T tapers[] = { { param::inputTrim, { -18, -10.8, 0, 10.8, 18 } },
                         { param::outputFader, { -24, -16.8, -6, 4.8, 12 } },
                         { param::compThreshold, { 10, 4, -5, -14, -20 } },
                         { param::compRelease, { 0.1, 0.15, 0.4, 2.0, 4.0 } },
                         { param::expThreshold, { -30, -25, -12, 5, 10 } },
                         { param::expRange, { 0, 5, 20, 33.3, 40 } },
                         { param::expRelease, { 0.1, 0.15, 0.4, 2.0, 4.0 } },
                         { param::bandGain, { -15, -9, 0, 9, 15 } },
                         { param::lfFreq, { 30, 50, 200, 300, 450 } },
                         { param::lmfFreq, { 0.2, 0.3, 1.0, 2.0, 2.5 } },
                         { param::hmfFreq, { 0.6, 0.8, 3.0, 6.0, 7.0 } },
                         { param::hfFreq, { 1.5, 2.0, 8.0, 14.0, 16.0 } },
                         { param::midQ, { 3.5, 2.5, 1.5, 0.5, 0.1 } } };
    for (const auto& t : tapers)
    {
        for (int i = 0; i < 5; ++i)
        {
            CHECK (t.t (r[i]) == Approx (t.v[i]));
            CHECK (t.t.toNorm (t.v[i]) == Approx (r[i]).margin (1e-12));
        }
        CHECK (t.t (0.35) == Approx (0.5 * (t.v[1] + t.v[2])));
        CHECK (t.t.toNorm (t.t (0.63)) == Approx (0.63));
    }
}

TEST_CASE ("Ratio: 21-step lookup, infinity at the top", "[parameters]")
{
    const double steps[] = { 1.0, 1.3, 1.5, 1.8, 2.0, 2.3, 2.7, 3.0, 3.7, 4.3, 5.0, 6.0, 7.0, 8.0, 12, 16, 20, 40, 60, 80 };
    for (int k = 0; k < 20; ++k)
    {
        CHECK (param::ratioValue (k / 20.0) == Approx (steps[k]));
        CHECK (param::ratioToNorm (steps[k]) == Approx (k / 20.0));
    }
    CHECK (std::isinf (param::ratioValue (1.0)));
    CHECK (param::ratioValue (0.025) == Approx (1.15));
    CHECK (param::ratioToNorm (INFINITY) == 1.0);
    CHECK (param::ratioInverse (1.0) == 0.0);
}

TEST_CASE ("Filter labels and defaults (section 2)", "[parameters]")
{
    CHECK (param::hpLabel (0.0) == 0.0);
    CHECK (param::hpLabel (0.2) == Approx (20.0));
    CHECK (param::hpLabel (0.5) == Approx (120.0));
    CHECK (param::hpLabel (1.0) == Approx (350.0));
    CHECK (param::lpLabel (0.0) == 0.0);
    CHECK (param::lpLabel (0.2) == Approx (12.0));
    CHECK (param::lpLabel (0.8) == Approx (3.5));

    namespace d = param::defaults;
    CHECK (param::inputTrim (d::inputTrim) == Approx (0.0).margin (1e-12));
    CHECK (param::outputFader (d::outputFader) == Approx (0.0).margin (1e-12));
    CHECK (param::compThreshold (d::compThreshold) == Approx (0.0).margin (1e-12));
    CHECK (param::ratioValue (d::compRatio) == Approx (1.0));
    CHECK (param::compRelease (d::compRelease) == Approx (0.4));
    CHECK (param::expThreshold (d::expThreshold) == Approx (-30.0));
    CHECK (param::expRange (d::expRange) == Approx (0.0));
    CHECK (param::expRelease (d::expRelease) == Approx (0.2));
    CHECK (param::lfFreq (d::lfFreq) == Approx (60.0));
    CHECK (param::lmfFreq (d::lmfFreq) == Approx (0.5));
    CHECK (param::hmfFreq (d::hmfFreq) == Approx (3.5));
    CHECK (param::hfFreq (d::hfFreq) == Approx (8.0));
    CHECK (param::midQ (d::lmfQ) == Approx (2.5));
    CHECK (param::midQ (d::hmfQ) == Approx (1.5));
}
