// Spec r1 section 2 (host value mappings) and section 8 (Auto Makeup linkage).
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "dsp/AutoMakeup.h"

using namespace fettest;
namespace pm = craft::fet_limiter::dsp::param;

TEST_CASE ("Input / Output display mapping", "[params]")
{
    CHECK (pm::Level::fromNorm (0.0) == -60.0); // -inf
    CHECK (pm::Level::fromNorm (0.375) == -30.0);
    CHECK (pm::Level::fromNorm (0.625) == -18.0);
    CHECK (pm::Level::fromNorm (0.25) == -36.0);
    CHECK (pm::Level::fromNorm (0.125) == -48.0);
    CHECK (pm::Level::fromNorm (1.0) == 0.0);
    CHECK (pm::Level::fromNorm (0.1) == -50.4);
    CHECK (pm::Level::fromNorm (0.5) == -24.0);
    for (double d = -59.9; d <= 0.0; d += 0.1)
    {
        const double snapped = pm::Level::snap (d);
        CHECK (std::abs (pm::Level::fromNorm (pm::Level::toNorm (snapped)) - snapped) < 1e-9);
    }
    CHECK (formatLevel (-60.0) == "-inf dB");
    CHECK (inputGain (Revision::A, -60.0) == 0.0);
}

TEST_CASE ("Attack / Release: 1 + 6 r, two decimals, defaults 3 / 4", "[params]")
{
    CHECK (pm::Time::fromNorm (0.0) == 1.0);
    CHECK (pm::Time::fromNorm (1.0) == 7.0);
    CHECK (pm::Time::fromNorm (1.0 / 3.0) == 3.0);
    CHECK (pm::Time::fromNorm (0.5) == 4.0);
    CHECK (pm::Time::fromNorm (0.123) == 1.74);
    Settings s;
    CHECK (s.attack == 3.0);
    CHECK (s.release == 4.0);
}

TEST_CASE ("Ratio / Analog / Revision thresholds", "[params]")
{
    CHECK (pm::ratioIndexFromNorm (0.0) == 0);
    CHECK (pm::ratioIndexFromNorm (0.1124) == 0);
    CHECK (pm::ratioIndexFromNorm (0.1125) == 1);
    CHECK (pm::ratioIndexFromNorm (0.3624) == 1);
    CHECK (pm::ratioIndexFromNorm (0.3625) == 2);
    CHECK (pm::ratioIndexFromNorm (0.6125) == 3);
    CHECK (pm::ratioIndexFromNorm (0.8624) == 3);
    CHECK (pm::ratioIndexFromNorm (0.8625) == 4);
    for (int i = 0; i < 5; ++i)
        CHECK (pm::ratioIndexFromNorm (pm::ratioNormFromIndex (i)) == i);
    CHECK (pm::analogIndexFromNorm (0.2374) == 0);
    CHECK (pm::analogIndexFromNorm (0.2375) == 1);
    CHECK (pm::analogIndexFromNorm (0.7374) == 1);
    CHECK (pm::analogIndexFromNorm (0.7375) == 2);
    CHECK (pm::revisionIndexFromNorm (0.4999) == 0); // B
    CHECK (pm::revisionIndexFromNorm (0.5) == 1);    // A
    Settings s;
    CHECK (s.ratio == Ratio::R4);
    CHECK (s.analog == Analog::Off);
    CHECK (s.revision == Revision::A);
    CHECK (s.inputDisplay == -30.0);
    CHECK (s.outputDisplay == -18.0);
    CHECK (s.mixPercent == 100.0);
    CHECK (s.trimDb == 0.0);
}

TEST_CASE ("Mix 100 r % (0.1 %), Trim -18 + 36 r dB (0.1 dB)", "[params]")
{
    CHECK (pm::Mix::fromNorm (0.0) == 0.0);
    CHECK (pm::Mix::fromNorm (0.12345) == 12.3);
    CHECK (pm::Mix::fromNorm (1.0) == 100.0);
    CHECK (pm::Trim::fromNorm (0.5) == 0.0);
    CHECK (pm::Trim::fromNorm (0.0) == -18.0);
    CHECK (pm::Trim::fromNorm (1.0) == 18.0);
    CHECK (pm::Trim::fromNorm (0.6) == 3.6);
}

namespace
{
double steadyOutDb (Revision rev, Ratio ratio, double input, double output, double levelDb)
{
    Settings s = neutralSettings();
    s.revision = rev;
    s.ratio = ratio;
    s.inputDisplay = input;
    s.outputDisplay = output;
    Rig rig (s);
    auto x = sine (1000.0, levelDb, 1.5, 48000.0);
    rig.run (x);
    return toneDb (x, 1000.0, 48000.0, x.size() - 4800, 4800);
}
} // namespace

TEST_CASE ("Auto Makeup keeps a -18 dBFS sine within +-2 dB while Input moves", "[automakeup]")
{
    for (auto rev : { Revision::A, Revision::B })
        for (auto ratio : { Ratio::R4, Ratio::R20, Ratio::All })
        {
            const double i0 = -30.0, o0 = -18.0;
            const double ref = steadyOutDb (rev, ratio, i0, o0, -18.0);
            for (double i1 : { -48.0, -36.0, -24.0, -18.0, -12.0, -6.0, 0.0 })
            {
                const double o1 = autoMakeupOutput (rev, ratio, false, i0, i1, o0);
                const double out = steadyOutDb (rev, ratio, i1, o1, -18.0);
                INFO ("rev " << int (rev) << " ratio " << ratioName (ratio) << " input " << i1 << " -> output " << o1 << ": "
                             << out << " dB vs " << ref);
                // Output saturates at its range ends ("stops following"); within range the level is kept
                if (o1 > pm::Level::minValue + 0.1 && o1 < pm::Level::maxValue)
                    CHECK (std::abs (out - ref) <= 2.0);
            }
        }
}

TEST_CASE ("Auto Makeup: no Input change, no Output move; Output never moves Input", "[automakeup]")
{
    CHECK (autoMakeupOutput (Revision::A, Ratio::R4, false, -30.0, -30.0, -18.0) == -18.0);
    CHECK (autoMakeupOutput (Revision::A, Ratio::R4, false, -60.0, -30.0, -18.0) == -18.0); // from -inf: undefined
}
