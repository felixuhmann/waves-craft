// Neutral behaviour, static curve (9.1), look-ahead ramp (9.2), latency, linking.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <random>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;

TEST_CASE ("Latency is H = 64 / 128 / 256 samples", "[latency]")
{
    CHECK (VocalCompressorEngine::latencyForSampleRate (44100.0) == 64);
    CHECK (VocalCompressorEngine::latencyForSampleRate (48000.0) == 64);
    CHECK (VocalCompressorEngine::latencyForSampleRate (88200.0) == 128);
    CHECK (VocalCompressorEngine::latencyForSampleRate (96000.0) == 128);
    CHECK (VocalCompressorEngine::latencyForSampleRate (192000.0) == 256);

    // an impulse comes out exactly H samples later (neutral settings)
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        VocalCompressorEngine e;
        e.prepare (fs, 1);
        e.setSettings (settings (0.0));
        e.reset();
        std::vector<float> x (1024, 0.0f);
        x[10] = 0.5f;
        float* p[1] = { x.data() };
        e.process (p, int (x.size()));
        const int H = e.getLatencySamples();
        for (int i = 0; i < 1024; ++i)
            CHECK (x[std::size_t (i)] == (i == 10 + H ? float (0.5 * 0.99) : 0.0f));
    }
}

TEST_CASE ("Neutral settings: y = 0.99 x, also above full scale", "[neutral]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    std::mt19937 rng (1);
    std::uniform_real_distribution<float> dist (-2.0f, 2.0f); // up to +6 dBFS (D1: no limiter)
    VocalCompressorEngine e;
    e.prepare (fs, 2);
    e.setSettings (settings (0.0));
    e.reset();
    const int H = e.getLatencySamples();
    const int n = 20000;
    std::vector<float> l (std::size_t (n + H), 0.0f), r (std::size_t (n + H), 0.0f);
    for (int i = 0; i < n; ++i)
    {
        l[std::size_t (i)] = dist (rng);
        r[std::size_t (i)] = dist (rng);
    }
    const auto l0 = l, r0 = r;
    for (int pos = 0; pos < n + H; pos += 333)
    {
        const int len = std::min (333, n + H - pos);
        float* p[2] = { l.data() + pos, r.data() + pos };
        e.process (p, len);
    }
    double worst = 0.0;
    for (int i = 0; i < n; ++i)
    {
        worst = std::max (worst, std::abs (double (l[std::size_t (i + H)]) - 0.99 * l0[std::size_t (i)]));
        worst = std::max (worst, std::abs (double (r[std::size_t (i + H)]) - 0.99 * r0[std::size_t (i)]));
    }
    CHECK (worst <= 1.0e-6);
}

TEST_CASE ("Static gain matches 9.1 within 0.05 dB", "[static]")
{
    struct Row
    {
        double comp;
        double values[7];
    };
    const double levels[7] = { -90, -40, -30, -20, -12, -4, 0 };
    const Row rows[] = { { 0, { -0.09, -0.09, -0.09, -0.09, -0.09, -0.09, -0.09 } },
                         { -6, { 5.91, 5.83, 5.64, 5.09, 3.97, 1.68, -0.09 } },
                         { -18, { 17.91, 17.33, 16.19, 13.33, 9.14, 3.30, -0.09 } },
                         { -36, { 35.90, 31.72, 26.48, 18.75, 11.51, 3.83, -0.09 } } };
    const double fs = 48000.0;
    for (const auto& row : rows)
        for (int j = 0; j < 7; ++j)
            for (double gainDbSetting : { 0.0, -10.0 })
            {
                // the table is rounded to 0.01 dB; also compare against the exact law
                const auto x = sineSchedule ({ { levels[j], ms (1500, fs) } }, fs);
                const auto r = run (x, fs, settings (row.comp, param::Gate::offValue, gainDbSetting));
                const long at = ms (1400, fs);
                const double measured = linToDb (r.gain[std::size_t (at)]) - gainDbSetting;
                INFO ("comp " << row.comp << " level " << levels[j] << " gain " << gainDbSetting);
                CHECK_THAT (measured, WithinAbs (row.values[j], 0.05));
                CHECK_THAT (measured, WithinAbs (staticGainDb (row.comp, levels[j]) + linToDb (0.99), 0.005));
                // output sample peak follows the same law
                double peak = 0.0;
                for (long i = at; i < at + 480; ++i)
                    peak = std::max (peak, std::abs (r.y[std::size_t (i)]));
                CHECK_THAT (linToDb (peak) - levels[j] - gainDbSetting, WithinAbs (row.values[j], 0.05));
            }
}

TEST_CASE ("Single isolated samples obey the static law (9.1)", "[static]")
{
    const double fs = 48000.0;
    const double amps[] = { 0.01, 0.1, 0.5, 1.0, 2.0 };
    const double expected[] = { 0.385, 0.866, 0.975, 0.990, 0.998 };
    for (int i = 0; i < 5; ++i)
    {
        std::vector<double> x (std::size_t (ms (200, fs)), 0.0);
        const long at = ms (100, fs);
        x[std::size_t (at)] = amps[i];
        const auto r = run (x, fs, settings (-36.0));
        INFO ("amplitude " << amps[i]);
        CHECK_THAT (r.y[std::size_t (at)], WithinAbs (expected[i], 0.0015));
    }
}

TEST_CASE ("Look-ahead attack ramp matches 9.2 at 44.1 / 48 / 96 kHz", "[attack]")
{
    const int offsets[] = { -70, -64, -60, -56, -48, -40, -32, -24, -16, -8, -4, -1, 0, 8 };
    const double t36[] = { 31.80, 31.80, 31.73, 31.52, 30.65, 28.98, 26.15, 21.88, 15.92, 7.81, 3.22, 0.65, 0.25, 0.00 };
    const double t18[] = { 17.42, 17.42, 17.37, 17.21, 16.51, 15.16, 12.93, 9.80, 6.03, 2.35, 0.95, 0.30, 0.18, 0.00 };

    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const int scale = fs > 50000.0 ? 2 : 1; // "doubled lengths at 96 kHz"
    for (double comp : { -36.0, -18.0 })
    {
        const auto* ref = comp < -20.0 ? t36 : t18;
        // 1 kHz step from -40 to 0 dBFS, phase-continuous sine starting at phase 0 (the first loud sample is the
        // zero crossing at which the level switches)
        const long pre = ms (1500, fs);
        const auto x = sineSchedule ({ { -40.0, pre }, { 0.0, ms (100, fs) } }, fs);
        const auto r = run (x, fs, settings (comp));
        for (int j = 0; j < 14; ++j)
        {
            INFO ("fs " << fs << " comp " << comp << " n " << offsets[j]);
            CHECK_THAT (gainDb (r, pre + long (offsets[j]) * scale), WithinAbs (ref[j], 0.25));
        }
    }
}

TEST_CASE ("Stereo detection is linked: both channels get the same gain", "[link]")
{
    const double fs = 48000.0;
    VocalCompressorEngine e;
    e.prepare (fs, 2);
    e.setSettings (settings (-24.0, -50.0, -3.0));
    e.reset();
    const int n = int (fs);
    std::vector<double> l (static_cast<std::size_t> (n)), r (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
    {
        l[std::size_t (i)] = 0.8 * std::sin (2.0 * pi * 220.0 * i / fs) * (i > n / 2 ? 0.05 : 1.0);
        r[std::size_t (i)] = 0.01 * std::sin (2.0 * pi * 3000.0 * i / fs);
    }
    auto lo = l, ro = r;
    double* p[2] = { lo.data(), ro.data() };
    e.process (p, n);
    const int H = e.getLatencySamples();
    for (int i = H; i < n; i += 7)
        if (std::abs (l[std::size_t (i - H)]) > 1e-3 && std::abs (r[std::size_t (i - H)]) > 1e-4)
        {
            const double gl = lo[std::size_t (i)] / l[std::size_t (i - H)];
            const double gr = ro[std::size_t (i)] / r[std::size_t (i - H)];
            REQUIRE_THAT (gl, WithinAbs (gr, 1e-9 * std::max (1.0, std::abs (gl))));
        }
}

TEST_CASE ("Detection is frequency independent (50 Hz - 12 kHz within 0.1 dB)", "[static]")
{
    const double fs = 48000.0;
    for (double f : { 50.0, 1000.0, 12000.0 })
    {
        const auto x = sineSchedule ({ { -12.0, ms (2000, fs) } }, fs, f);
        const auto r = run (x, fs, settings (-18.0));
        // a 12 kHz sine is not sampled at its crest; compare against the actual sampled peak level
        double peak = 0.0;
        for (long i = ms (1000, fs); i < ms (1100, fs); ++i)
            peak = std::max (peak, std::abs (x[std::size_t (i)]));
        const double expected = staticGainDb (-18.0, linToDb (peak));
        // gain applied at the signal crests (low-frequency tones may ripple between crests, section 9.8)
        double crestGain = 1e9;
        for (long i = ms (1880, fs); i < ms (1900, fs); ++i)
            crestGain = std::min (crestGain, gainDb (r, i));
        INFO ("f " << f);
        CHECK_THAT (crestGain, WithinAbs (expected, 0.1));
        CHECK_THAT (crestGain, WithinAbs (staticGainDb (-18.0, -12.0), 0.1));
    }
}

TEST_CASE ("Parameter changes are smooth (no gain steps)", "[smoothing]")
{
    const double fs = 48000.0;
    VocalCompressorEngine e;
    e.prepare (fs, 2);
    e.setSettings (settings (0.0));
    e.reset();
    std::vector<double> l (480, 0.0), r (480, 0.0);
    double last = -1.0, worstStep = 0.0;
    for (int block = 0; block < 400; ++block)
    {
        if (block == 50)
            e.setSettings (settings (-36.0, -30.0, -20.0));
        if (block == 200)
            e.setSettings (settings (-6.0, param::Gate::offValue, 0.0));
        for (int i = 0; i < 480; ++i)
        {
            l[std::size_t (i)] = 0.3 * std::sin (2.0 * pi * 1000.0 * (block * 480 + i) / fs);
            r[std::size_t (i)] = probe;
        }
        double* p[2] = { l.data(), r.data() };
        e.process (p, 480);
        for (int i = 0; i < 480; ++i)
        {
            const double g = linToDb (r[std::size_t (i)] / probe);
            if (last > -500.0 && block > 1)
                worstStep = std::max (worstStep, std::abs (g - last));
            last = g;
        }
    }
    CHECK (worstStep < 0.1); // dB per sample
}
