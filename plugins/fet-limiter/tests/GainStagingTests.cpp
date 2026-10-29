// Spec r1 section 4: Input / Output gain laws per revision (tables/gain_input.csv, gain_output.csv).
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TestHelpers.h"

using namespace fettest;

namespace
{
// Small-signal gain (dB) at 1 kHz, -60 dBFS, ratio 4 (no GR at these levels), Analog Off.
double measureGainDb (Revision rev, double inputDisplay, double outputDisplay, double fs = 48000.0)
{
    Settings s = neutralSettings();
    s.revision = rev;
    s.inputDisplay = inputDisplay;
    s.outputDisplay = outputDisplay;
    Rig rig (s, fs);
    auto x = sine (1000.0, -60.0, 0.5, fs);
    rig.run (x);
    const size_t n = wholeCycles (1000.0, fs, size_t (0.1 * fs));
    return toneDb (x, 1000.0, fs, x.size() - n, n) - (-60.0);
}
} // namespace

TEST_CASE ("Input gain law matches tables/gain_input.csv (+-0.2 dB)", "[gain]")
{
    const auto& t = specTables();
    const auto csv = parseCsv (craft::fet_limiter::spec_tables::gainInputCsv);
    for (const auto& row : csv.rows)
    {
        const Revision rev = row[0] == "A" ? Revision::A : Revision::B;
        const double display = std::stod (row[1]);
        const double expected = parseValue (row[2]);
        // the response filters' absolute gain at 1 kHz is part of the measured gain law
        if (! std::isfinite (expected))
        {
            Settings s = neutralSettings();
            s.revision = rev;
            s.inputDisplay = display;
            s.outputDisplay = 0.0;
            Rig rig (s);
            auto x = sine (1000.0, -6.0, 0.2, 48000.0);
            rig.run (x);
            double peak = 0.0;
            for (double v : x)
                peak = std::max (peak, std::abs (v));
            INFO (row[0] << " display " << display);
            CHECK (peak == 0.0); // -inf: silence
            continue;
        }
        const double g = measureGainDb (rev, display, 0.0);
        INFO (row[0] << " display " << display << " expected " << expected << " measured " << g);
        CHECK (std::abs (g - expected) <= 0.2);
    }
    (void) t;
}

TEST_CASE ("Input gain law interpolates linearly in display dB between rows", "[gain]")
{
    // halfway between A rows -24 (24.74) and -21 (29.04)
    const double g = measureGainDb (Revision::A, -22.5, 0.0);
    CHECK (std::abs (g - 0.5 * (24.74 + 29.04)) <= 0.2);
    const double gb = measureGainDb (Revision::B, -33.0, 0.0); // between -36 (14.19) and -30 (21.58)
    CHECK (std::abs (gb - 0.5 * (14.19 + 21.58)) <= 0.2);
}

TEST_CASE ("Output gain law matches tables/gain_output.csv (+-0.2 dB)", "[gain]")
{
    const auto csv = parseCsv (craft::fet_limiter::spec_tables::gainOutputCsv);
    const double ref[2] = { measureGainDb (Revision::A, -30.0, 0.0), measureGainDb (Revision::B, -30.0, 0.0) };
    for (const auto& row : csv.rows)
    {
        const Revision rev = row[0] == "A" ? Revision::A : Revision::B;
        const double display = std::stod (row[1]);
        if (display <= -60.0)
            continue; // display -60 is the "-inf" end of the host range (tested below)
        const double expected = std::stod (row[2]);
        const double g = measureGainDb (rev, -30.0, display) - ref[revIndex (rev)];
        INFO (row[0] << " display " << display << " expected " << expected << " measured " << g);
        CHECK (std::abs (g - expected) <= 0.2);
    }
}

TEST_CASE ("Output -inf (host value 0) is silence; -59.9 follows the bottom table row", "[gain]")
{
    CHECK (outputGain (Revision::A, -60.0) == 0.0);
    CHECK (std::abs (outputGainDb (Revision::A, -59.9) - (-73.98 + 0.1 / 3.0 * (73.98 - 70.45))) < 0.01);
}

TEST_CASE ("All ratio adds +1.2 dB (A) / +0.6 dB (B) small-signal gain", "[gain]")
{
    for (auto rev : { Revision::A, Revision::B })
    {
        Settings s = neutralSettings();
        s.revision = rev;
        s.outputDisplay = 0.0;
        double g[2];
        for (int i = 0; i < 2; ++i)
        {
            s.ratio = i == 0 ? Ratio::R4 : Ratio::All;
            Rig rig (s);
            auto x = sine (1000.0, -60.0, 0.5, 48000.0);
            rig.run (x);
            const size_t n = 4800;
            g[i] = toneDb (x, 1000.0, 48000.0, x.size() - n, n);
        }
        const double expected = rev == Revision::A ? 1.2 : 0.6;
        INFO ("revision " << revisionLabel (revIndex (rev) == 0 ? 1 : 0));
        CHECK (std::abs ((g[1] - g[0]) - expected) <= 0.2);
    }
}

TEST_CASE ("Gain staging is independent of the sample rate", "[gain][samplerate]")
{
    for (double fs : { 44100.0, 96000.0 })
        for (double d : { -48.0, -30.0, -12.0, 0.0 })
        {
            const double g48 = measureGainDb (Revision::A, d, -6.0, 48000.0);
            const double g = measureGainDb (Revision::A, d, -6.0, fs);
            INFO ("fs " << fs << " display " << d);
            CHECK (std::abs (g - g48) <= 0.07);
        }
}
