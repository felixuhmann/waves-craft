// Static gain reduction through the complete engine: T1 (Compress / Limit), T4 (Hi Freq vs frequency),
// T5 (Hi Freq vs level) and sample-rate independence (spec 3, 11).
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>

#include "SpecMeasure.h"

using namespace optotest;

namespace
{
void checkStaticTable (const spec::Table& t, dsp::Mode mode)
{
    double worst = 0.0;
    for (int r = 0; r < t.numRows; ++r)
        for (int c = 1; c < t.numCols; ++c)
        {
            const double pr = t.at (r, 0), lev = t.headerValue[c], want = t.at (r, c);
            const double got = steadyGrSine (makeSettings (pr, mode), 1000.0, lev, 48000.0, 5.0);
            INFO (t.name << " PR " << pr << " level " << lev << ": want " << want << " got " << got);
            CHECK (std::abs (got - want) <= 0.5); // spec 11: +-0.5 dB per point
            worst = std::max (worst, std::abs (got - want));
        }
    std::printf ("[opto-leveler] %s: worst static error %.3f dB\n", t.name, worst);
}
} // namespace

TEST_CASE ("T1C static curve, Compress", "[static][T1]") { checkStaticTable (spec::T1C, dsp::Mode::Compress); }

TEST_CASE ("T1L static curve, Limit", "[static][T1]") { checkStaticTable (spec::T1L, dsp::Mode::Limit); }

TEST_CASE ("T4 sidechain weighting vs frequency (Hi Freq)", "[static][T4]")
{
    double worst = 0.0;
    for (int r = 0; r < spec::T4.numRows; ++r)
    {
        const std::string id = spec::T4.textAt (r, 0);
        const bool limit = id.find ("Limit") != std::string::npos;
        const double hf = spec::T4.at (r, 0) == spec::T4.at (r, 0) ? spec::T4.at (r, 0) : 100.0;
        for (int c = 1; c < spec::T4.numCols; ++c)
        {
            const double f = spec::T4.headerValue[c], want = spec::T4.at (r, c);
            const double got = steadyGrSine (makeSettings (50.0, limit ? dsp::Mode::Limit : dsp::Mode::Compress, hf), f, -10.0,
                                             48000.0, 5.0);
            INFO ("Hi Freq " << id << " at " << f << " Hz: want " << want << " got " << got);
            CHECK (std::abs (got - want) <= 1.0); // spec 11
            worst = std::max (worst, std::abs (got - want));
        }
    }
    std::printf ("[opto-leveler] T4: worst error %.3f dB\n", worst);
}

TEST_CASE ("T5 static curve vs Hi Freq at 1 kHz", "[static][T5]")
{
    double worst = 0.0;
    for (int r = 0; r < spec::T5.numRows; ++r)
        for (int c = 1; c < spec::T5.numCols; ++c)
        {
            const double hf = spec::T5.at (r, 0), lev = spec::T5.headerValue[c], want = spec::T5.at (r, c);
            const double got = steadyGrSine (makeSettings (60.0, dsp::Mode::Compress, hf), 1000.0, lev, 48000.0, 5.0);
            INFO ("Hi Freq " << hf << " level " << lev << ": want " << want << " got " << got);
            CHECK (std::abs (got - want) <= 0.5);
            worst = std::max (worst, std::abs (got - want));
        }
    std::printf ("[opto-leveler] T5: worst error %.3f dB\n", worst);
}

TEST_CASE ("Hi Freq interpolates linearly between the tabulated settings", "[static][T4]")
{
    // halfway between two tabulated Hi Freq settings the reduction lies between the two (spec 5)
    for (double f : { 200.0, 1000.0, 5000.0 })
    {
        const double a = steadyGrSine (makeSettings (50.0, dsp::Mode::Compress, 25.0), f, -10.0, 48000.0, 4.0);
        const double b = steadyGrSine (makeSettings (50.0, dsp::Mode::Compress, 50.0), f, -10.0, 48000.0, 4.0);
        const double m = steadyGrSine (makeSettings (50.0, dsp::Mode::Compress, 37.5), f, -10.0, 48000.0, 4.0);
        CHECK (m >= std::min (a, b) - 0.05);
        CHECK (m <= std::max (a, b) + 0.05);
        CHECK (std::abs (m - 0.5 * (a + b)) <= 0.5);
    }
}

TEST_CASE ("static gain reduction is sample-rate independent", "[static][samplerate]")
{
    double worst = 0.0;
    for (auto mode : { dsp::Mode::Compress, dsp::Mode::Limit })
        for (double pr : { 46.0, 60.0, 80.0 })
            for (double lev : { -30.0, -16.0, -6.0, 0.0 })
            {
                const auto s = makeSettings (pr, mode, 100.0);
                const double ref = steadyGrSine (s, 1000.0, lev, 48000.0, 4.0);
                for (double fs : { 44100.0, 96000.0 })
                {
                    const double g = steadyGrSine (s, 1000.0, lev, fs, 4.0);
                    INFO ("PR " << pr << " level " << lev << " fs " << fs << ": " << g << " vs " << ref);
                    CHECK (std::abs (g - ref) <= 0.03); // spec 3
                    worst = std::max (worst, std::abs (g - ref));
                }
            }
    std::printf ("[opto-leveler] static sample-rate dependence: worst %.4f dB\n", worst);
}
