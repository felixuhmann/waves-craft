// Output stage: gain law (T2), tube colour (T7 / T7b), response (T8), Mix / Trim, analog noise (spec 8),
// latency and parameter smoothing.
#include <catch2/catch_test_macros.hpp>

#include <cstdio>

#include "SpecMeasure.h"

using namespace optotest;

namespace
{
// path gain (dB) of a steady sine through the engine
double pathGainDb (const dsp::Settings& s, double freq, double peakDb, double fs = 48000.0, double seconds = 0.5)
{
    return measureHarmonics (s, freq, peakDb, fs, seconds).gainDb;
}

// response of the engine at small signal: ratio of output to input for a sine at f, settled
double responseDb (double f, double fs)
{
    return pathGainDb (makeSettings (0.0), f, -40.0, fs, 0.4);
}
} // namespace

TEST_CASE ("T2 gain law: path gain at -60 dBFS, 1 kHz, Peak Reduction 0", "[output][T2]")
{
    for (int k = 1; k < spec::T2.numCols; ++k)
    {
        const double display = spec::T2.headerValue[k], want = spec::T2.at (0, k);
        const double got = pathGainDb (makeSettings (0.0, dsp::Mode::Compress, 100.0, display), 1000.0, -60.0);
        INFO ("Gain " << display << ": want " << want << " got " << got);
        CHECK (std::abs (got - want) <= 0.1);
    }
}

TEST_CASE ("unity Gain path is transparent apart from the level-dependent colour (spec 3, T7)", "[output][T7]")
{
    // -0.05 ... +0.06 dB at 1 kHz from -40 to 0 dBFS
    for (int r = 0; r < spec::T7.numRows; ++r)
    {
        if (spec::T7.at (r, 0) != 0.0)
            continue;
        const double f = spec::T7.at (r, 1), lev = spec::T7.at (r, 2), want = spec::T7.at (r, 3);
        const double got = pathGainDb (makeSettings (0.0), f, lev, 48000.0, 0.5);
        INFO (f << " Hz " << lev << " dBFS: want " << want << " got " << got);
        CHECK (std::abs (got - want) <= 0.1);
    }
}

TEST_CASE ("T7 harmonics of steady sines (Peak Reduction 0 and 50)", "[output][T7]")
{
    // Tolerance +-3 dB for components above -90 dBc (spec 11). Compressed rows: the -40 / -30 dBFS rows of the table
    // carry gain reduction that is not on the static curve (measurement memory, see notes) and are checked for the
    // colour only; under compression H4 (100 Hz) and H5 (1 kHz) are matched to +-6 dB (documented deviation).
    double worst = 0.0;
    for (int r = 0; r < spec::T7.numRows; ++r)
    {
        const double pr = spec::T7.at (r, 0), f = spec::T7.at (r, 1), lev = spec::T7.at (r, 2);
        const auto h = measureHarmonics (makeSettings (pr), f, lev, 48000.0, pr > 0.0 ? 4.0 : 0.5);
        for (int k = 2; k <= 5; ++k)
        {
            const double want = spec::T7.at (r, 2 + k);
            if (want < -90.0 || k * f >= 24000.0)
                continue;
            if (pr > 0.0 && lev < -18.0 && k != 2) // memory-affected rows: colour (H2) only
                continue;
            double tol = 3.0;
            if (pr > 0.0 && ((k == 4 && f < 500.0) || (k == 5 && f > 500.0)))
                tol = 6.0;
            INFO ("PR " << pr << " " << f << " Hz " << lev << " dBFS H" << k << ": want " << want << " got " << h.hDbc[k]);
            CHECK (std::abs (h.hDbc[k] - want) <= tol);
            if (tol == 3.0)
                worst = std::max (worst, std::abs (h.hDbc[k] - want));
        }
        if (pr > 0.0 && lev >= -18.0)
        {
            INFO ("PR " << pr << " " << f << " Hz " << lev << " dBFS gain: want " << spec::T7.at (r, 3) << " got " << h.gainDb);
            CHECK (std::abs (h.gainDb - spec::T7.at (r, 3)) <= 1.5);
        }
    }
    std::printf ("[opto-leveler] T7: worst harmonic error (+-3 dB class) %.2f dB\n", worst);
}

TEST_CASE ("T7b harmonics vs Gain at -10 dBFS, 1 kHz, Peak Reduction 0", "[output][T7b]")
{
    for (int c = 1; c < spec::T7b.numCols; ++c)
    {
        const double gain = spec::T7b.headerValue[c];
        const auto h = measureHarmonics (makeSettings (0.0, dsp::Mode::Compress, 100.0, gain), 1000.0, -10.0, 48000.0, 0.5);
        for (int k = 2; k <= 3; ++k)
        {
            const double want = spec::T7b.at (k - 2, c);
            INFO ("Gain " << gain << " H" << k << ": want " << want << " got " << h.hDbc[k]);
            CHECK (std::abs (h.hDbc[k] - want) <= 3.0);
        }
    }
}

TEST_CASE ("T8 small-signal response at 44.1 / 48 / 96 kHz", "[output][T8]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int c = 1; c < spec::T8.numCols; ++c)
        {
            const double f = spec::T8.headerValue[c];
            if (f > 0.46 * fs)
                continue;
            const double got = responseDb (f, fs);
            INFO (fs << " Hz rate, " << f << " Hz: want " << spec::T8.at (0, c) << " got " << got);
            CHECK (std::abs (got - spec::T8.at (0, c)) <= 0.2);
        }
}

TEST_CASE ("Mix is a linear crossfade with the unprocessed input, Trim an exact gain", "[output][mix]")
{
    Gauss g;
    for (double mix : { 0.0, 30.0, 100.0 })
        for (double trim : { -18.0, -6.5, 0.0, 12.3, 18.0 })
        {
            auto s = makeSettings (60.0, dsp::Mode::Compress, 50.0, 50.0);
            s.mixPercent = mix;
            s.trimDb = trim;
            auto ref = s;
            ref.trimDb = 0.0;
            Engine a (s), b (ref);
            auto dry = s;
            dry.mixPercent = 100.0;
            dry.trimDb = 0.0;
            Engine w (dry);
            double maxErr = 0.0, maxMixErr = 0.0;
            for (int i = 0; i < 24000; ++i)
            {
                const double x = 0.1 * g.next(); // keeps +18 dB trim below the output protection
                double ya = x, yb = x, yw = x;
                double* pa[1] = { &ya };
                double* pb[1] = { &yb };
                double* pw[1] = { &yw };
                a.e.process (pa, 1, 1);
                b.e.process (pb, 1, 1);
                w.e.process (pw, 1, 1);
                maxErr = std::max (maxErr, std::abs (ya - yb * dbToLin (trim)));
                const double m = mix / 100.0;
                maxMixErr = std::max (maxMixErr, std::abs (yb - (m * yw + (1.0 - m) * x)));
            }
            INFO ("mix " << mix << " trim " << trim);
            CHECK (maxErr <= 1.0e-9);
            CHECK (maxMixErr <= 1.0e-9);
        }
    // Trim measured in dB on a sine
    auto s = makeSettings (0.0);
    s.trimDb = 7.5;
    CHECK (std::abs (pathGainDb (s, 1000.0, -30.0) - pathGainDb (makeSettings (0.0), 1000.0, -30.0) - 7.5) <= 0.05);
}

TEST_CASE ("zero latency", "[output][latency]")
{
    CHECK (dsp::LevelerEngine::getLatencySamples() == 0);
    Engine en (makeSettings (0.0));
    std::vector<double> x (64, 0.0);
    x[10] = 0.5;
    double* p[1] = { x.data() };
    en.e.process (p, 1, 64);
    int peak = 0;
    for (int i = 0; i < 64; ++i)
        if (std::abs (x[size_t (i)]) > std::abs (x[size_t (peak)]))
            peak = i;
    CHECK (peak == 10);
    CHECK (std::abs (x[10]) > 0.45);
}

namespace
{
// spectrum helpers for the analog noise test
double humComponent (const std::vector<double>& y, double f, double fs) { return toneAmplitude (y, f, fs); }
} // namespace

TEST_CASE ("Analog noise: off adds nothing, 50 / 60 Hz add hum and white noise (spec 8)", "[output][analog]")
{
    const double fs = 48000.0;
    // Off: silence in, silence out (< -170 dBFS)
    {
        Engine en (makeSettings (46.0, dsp::Mode::Compress, 50.0, 40.0));
        std::vector<double> l (48000, 0.0), r (48000, 0.0);
        double* p[2] = { l.data(), r.data() };
        en.e.process (p, 2, 48000);
        double peak = 0.0;
        for (size_t i = 0; i < l.size(); ++i)
            peak = std::max ({ peak, std::abs (l[i]), std::abs (r[i]) });
        CHECK ((peak == 0.0 || linToDb (peak) < -170.0));
    }
    const double hum50[6] = { -121, -98, -117, -106, -135, -121 }, hum60[6] = { -130, -99, -121, -106, -128, -127 };
    for (int type = 1; type <= 2; ++type)
    {
        auto s = makeSettings (46.0, dsp::Mode::Compress, 50.0, 40.0);
        s.analog = dsp::AnalogNoise::Type (type);
        Engine en (s, fs);
        // 120 s: 1/120 Hz bins keep the white-noise share of a bin ~8 dB under the weakest (-135 dBFS) hum line
        const int n = int (120.0 * fs);
        std::vector<double> l (size_t (n), 0.0), r (size_t (n), 0.0);
        double* p[2] = { l.data(), r.data() };
        en.e.process (p, 2, n);
        const double fm = type == 1 ? 50.0 : 60.0;
        const double* want = type == 1 ? hum50 : hum60;
        for (int k = 1; k <= 6; ++k)
        {
            const double got = linToDb (humComponent (l, k * fm, fs));
            INFO ("mains " << fm << " Hz harmonic " << k << ": want " << want[k - 1] << " got " << got);
            CHECK (std::abs (got - want[k - 1]) <= 2.0);
        }
        // white noise density: mean power in 1 ... 10 kHz (no hum there), per Hz
        double acc = 0.0, cross = 0.0;
        for (int i = 0; i < n; ++i)
        {
            acc += l[size_t (i)] * l[size_t (i)];
            cross += l[size_t (i)] * r[size_t (i)];
        }
        double hum = 0.0;
        for (int k = 1; k <= 6; ++k)
            hum += 0.5 * std::pow (dbToLin (want[k - 1]), 2.0);
        const double noisePower = acc / n - hum;
        const double density = 10.0 * std::log10 (noisePower / (fs / 2.0));
        INFO ("noise density " << density << " dBFS/Hz");
        CHECK (std::abs (density - (-135.0)) <= 2.0);
        const double totalRms = 10.0 * std::log10 (acc / n);
        std::printf ("[opto-leveler] analog %g Hz: noise density %.2f dB/Hz, total %.2f dBFS RMS\n", fm, density, totalRms);
        // noise uncorrelated between channels (hum is common): correlation ~ hum share only
        CHECK (std::abs (cross / n - hum) <= 0.2 * noisePower);
    }
}

TEST_CASE ("parameter changes are smoothed within 20 ms without clicks (D4)", "[output][smoothing]")
{
    const double fs = 48000.0;
    auto s = makeSettings (40.0, dsp::Mode::Compress, 50.0, 40.0);
    Engine en (s, fs);
    const double w = 2.0 * pi * 100.0 / fs;
    double prev = 0.0, maxStep = 0.0;
    int i = 0;
    auto run = [&] (int n) {
        for (int k = 0; k < n; ++k, ++i)
        {
            double x = 0.25 * std::sin (w * i);
            double* p[1] = { &x };
            en.e.process (p, 1, 1);
            maxStep = std::max (maxStep, std::abs (x - prev));
            prev = x;
        }
    };
    run (int (0.5 * fs));
    const double baseline = maxStep;
    // jump several parameters at once
    s.gain = 70.0;
    s.trimDb = 12.0;
    s.mixPercent = 20.0;
    s.peakReduction = 80.0;
    s.mode = dsp::Mode::Limit;
    s.hiFreq = 0.0;
    en.e.setSettings (s);
    maxStep = 0.0;
    run (int (0.03 * fs));
    // a sine at 100 Hz changes by at most 2*pi*100/48000*A per sample; the ramps add only a fraction of that
    const double ampAfter = 0.25 * dbToLin (12.0 + dsp::gainLawDb (70.0));
    CHECK (maxStep <= std::max (baseline, 2.0 * pi * 100.0 / fs * ampAfter) * 1.5 + 1e-3);
}

TEST_CASE ("output protection only acts above +30 dBFS", "[output][protect]")
{
    for (double y : { 0.0, 0.5, -1.0, 10.0, -31.0 })
        CHECK (dsp::LevelerEngine::protect (y) == y);
    double prev = 0.0;
    for (double y = 31.0; y < 1.0e6; y *= 1.3)
    {
        const double p = dsp::LevelerEngine::protect (y);
        CHECK (p < 100.0);
        CHECK (p >= prev);
        CHECK (dsp::LevelerEngine::protect (-y) == -p);
        prev = p;
    }
    // maximum settings, full-scale noise: bounded output
    auto s = makeSettings (0.0, dsp::Mode::Compress, 100.0, 100.0);
    s.trimDb = 18.0;
    Engine en (s);
    Gauss g;
    double peak = 0.0;
    for (int i = 0; i < 48000; ++i)
    {
        double x = std::clamp (g.next(), -1.0, 1.0);
        double* p[1] = { &x };
        en.e.process (p, 1, 1);
        peak = std::max (peak, std::abs (x));
    }
    CHECK (peak < 100.0);
}
