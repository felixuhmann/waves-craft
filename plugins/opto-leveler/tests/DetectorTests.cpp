// Level detector (T6), feed-forward topology, stereo linking (D6) and the mono variant (D3).
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>

#include "SpecMeasure.h"

using namespace optotest;

TEST_CASE ("T6 detector type: equal-RMS white noise vs 1 kHz sine", "[detector][T6]")
{
    for (int r = 0; r < spec::T6.numRows; ++r)
    {
        const bool limit = std::string (spec::T6.textAt (r, 0)) == "Limit";
        const auto s = makeSettings (50.0, limit ? dsp::Mode::Limit : dsp::Mode::Compress);
        const double rms = spec::T6.at (r, 1);
        const double sine = steadyGrRms (s, 0, rms), noise = steadyGrRms (s, 1, rms);
        std::printf ("[opto-leveler] T6 %s %+.0f dBFS: sine %.2f (%.2f)  noise %.2f (%.2f)\n", spec::T6.textAt (r, 0), rms,
                     sine, spec::T6.at (r, 2), noise, spec::T6.at (r, 3));
        INFO (spec::T6.textAt (r, 0) << " " << rms << " dBFS RMS");
        CHECK (std::abs (sine - spec::T6.at (r, 2)) <= 1.0);
        CHECK (std::abs (noise - spec::T6.at (r, 3)) <= 1.0);
        CHECK (noise < sine); // averaging detector: noise is reduced less than a sine of equal RMS
    }
}

namespace
{
// Runs a stereo engine on two independent sines and returns the final cell gain.
double stereoGain (const dsp::Settings& s, double levelL, double levelR, double fs = 48000.0)
{
    Engine en (s, fs);
    const int n = int (3.0 * fs);
    double l, r;
    for (int i = 0; i < n; ++i)
    {
        l = dbToLin (levelL) * std::sin (2.0 * pi * 1000.0 * i / fs);
        r = dbToLin (levelR) * std::sin (2.0 * pi * 700.0 * i / fs);
        double* p[2] = { &l, &r };
        en.e.process (p, 2, 1);
    }
    return en.e.getCellGain();
}
} // namespace

TEST_CASE ("stereo: one linked detector, identical gain on both channels (D6: max of the channel levels)", "[detector][stereo]")
{
    const auto s = makeSettings (50.0);
    const double loudLeft = stereoGain (s, -10.0, -50.0);
    const double loudRight = stereoGain (s, -50.0, -10.0);
    const double bothLoud = stereoGain (s, -10.0, -10.0);
    const double mono = 1.0 / dbToLin (steadyGrSine (s, 1000.0, -10.0, 48000.0, 3.0));
    CHECK (std::abs (linToDb (loudLeft) - linToDb (mono)) <= 0.1);
    CHECK (std::abs (linToDb (loudRight) - linToDb (loudLeft)) <= 0.2); // 700 vs 1000 Hz weighting differs slightly
    CHECK (std::abs (linToDb (bothLoud) - linToDb (loudLeft)) <= 0.2);   // max, not sum

    // both output channels receive the same gain: a quiet channel is reduced like the loud one
    Engine en (s);
    const double fs = 48000.0;
    double sumInR = 0.0, sumOutR = 0.0;
    for (int i = 0; i < int (3.0 * fs); ++i)
    {
        double l = dbToLin (-10.0) * std::sin (2.0 * pi * 1000.0 * i / fs);
        const double rin = dbToLin (-50.0) * std::sin (2.0 * pi * 1000.0 * i / fs);
        double r = rin;
        double* p[2] = { &l, &r };
        en.e.process (p, 2, 1);
        if (i > int (2.5 * fs))
        {
            sumInR += rin * rin;
            sumOutR += r * r;
        }
    }
    const double grRight = -10.0 * std::log10 (sumOutR / sumInR) - (-dsp::gainLawDb (32.28)) - 0.0;
    CHECK (std::abs (grRight - (-linToDb (en.e.getCellGain()))) <= 0.3);
}

TEST_CASE ("mono variant equals one channel of the stereo engine (D3)", "[detector][mono]")
{
    const auto s = makeSettings (60.0, dsp::Mode::Compress, 50.0, 45.0);
    Engine mono (s), stereo (s);
    Gauss g;
    double maxDiff = 0.0;
    for (int i = 0; i < 48000; ++i)
    {
        const double x = 0.3 * g.next() * (0.5 + 0.5 * std::sin (i * 0.0003));
        double m = x, l = x, r = x;
        double* pm[1] = { &m };
        double* ps[2] = { &l, &r };
        mono.e.process (pm, 1, 1);
        stereo.e.process (ps, 2, 1);
        maxDiff = std::max ({ maxDiff, std::abs (m - l), std::abs (m - r) });
    }
    CHECK (maxDiff <= 1.0e-12);
}

TEST_CASE ("the detector is fed from the input (feed-forward) and Hi Freq does not touch the audio path", "[detector]")
{
    // Gain, Mix and Trim change the output but not the gain reduction
    const double ref = steadyGrSine (makeSettings (60.0), 1000.0, -12.0, 48000.0, 3.0);
    auto s = makeSettings (60.0, dsp::Mode::Compress, 100.0, 80.0);
    s.trimDb = -12.0;
    s.mixPercent = 40.0;
    CHECK (std::abs (steadyGrSine (s, 1000.0, -12.0, 48000.0, 3.0) - ref) <= 1.0e-9);

    // with Peak Reduction 0 the output does not depend on Hi Freq
    Engine a (makeSettings (0.0, dsp::Mode::Compress, 0.0)), b (makeSettings (0.0, dsp::Mode::Compress, 100.0));
    Gauss g;
    double maxDiff = 0.0;
    for (int i = 0; i < 24000; ++i)
    {
        double x1 = 0.5 * g.next(), x2 = x1;
        double* p1[1] = { &x1 };
        double* p2[1] = { &x2 };
        a.e.process (p1, 1, 1);
        b.e.process (p2, 1, 1);
        maxDiff = std::max (maxDiff, std::abs (x1 - x2));
    }
    CHECK (maxDiff == 0.0);
}
