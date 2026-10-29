// Sections 3, 8, 9, 11, 12 and decisions D2 / D5: neutral identity, latency, mix, output gain, output peak stage,
// parameter smoothing, robustness.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

TEST_CASE ("Latency H = 64 / 128 / 256 samples", "[engine]")
{
    CHECK (PunchEngine::latencyForSampleRate (44100.0) == 64);
    CHECK (PunchEngine::latencyForSampleRate (48000.0) == 64);
    CHECK (PunchEngine::latencyForSampleRate (88200.0) == 128);
    CHECK (PunchEngine::latencyForSampleRate (96000.0) == 128);
    CHECK (PunchEngine::latencyForSampleRate (176400.0) == 256);
    CHECK (PunchEngine::latencyForSampleRate (192000.0) == 256);
    // an impulse comes out exactly H samples later (neutral settings)
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        PunchEngine e;
        e.prepare (fs, 2);
        e.setSettings (neutralSettings());
        e.reset();
        const int H = e.getLatencySamples();
        std::vector<double> l (1024, 0.0), r (1024, 0.0);
        l[10] = 0.5;
        r[10] = -0.25;
        double* p[2] = { l.data(), r.data() };
        e.process (p, 1024);
        CHECK (l[size_t (10 + H)] == 0.5);
        CHECK (r[size_t (10 + H)] == -0.25);
    }
}

TEST_CASE ("Neutral identity: Ratio 1, Analog Off, Output 0, any Mix / Punch -> bit-exact", "[engine][neutral]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double mixPct : { 0.0, 37.5, 100.0 })
            for (double punch : { 0.0, 1.0, 30.0 })
                for (double T : { 0.0, -24.0, -48.0 })
                {
                    Settings s = neutralSettings();
                    s.mixPct = mixPct;
                    s.punch = punch;
                    s.thresholdDb = T;
                    s.attackMs = 0.5;
                    s.releaseMs = 3.0;
                    Signal in;
                    in.l = gaussianNoise (20000, 0.3, 7u);
                    in.r = gaussianNoise (20000, 0.2, 8u);
                    for (auto* v : { &in.l, &in.r })
                        for (auto& x : *v)
                            x = std::clamp (x, -0.999, 0.999);
                    in.l[100] = 1.0; // exactly 0 dBFS is still "below the ceiling"
                    const auto y = runAligned (in, fs, s, 333);
                    size_t bad = 0;
                    for (size_t i = 0; i < in.size(); ++i)
                        bad += (y.l[i] != in.l[i]) + (y.r[i] != in.r[i]);
                    INFO ("fs " << fs << " mix " << mixPct << " punch " << punch << " T " << T);
                    CHECK (bad == 0);
                }
    // mono variant: identical engine on one channel
    Settings s = neutralSettings();
    const auto x = gaussianNoise (10000, 0.25, 9u);
    const auto y = runAligned (mono (x), 48000.0, s).l;
    for (size_t i = 0; i < x.size(); ++i)
        REQUIRE (y[i] == x[i]);
}

TEST_CASE ("Float processing of the neutral setting is bit-exact too", "[engine][neutral]")
{
    PunchEngine e;
    e.prepare (48000.0, 2);
    e.setSettings (neutralSettings());
    e.reset();
    const int H = e.getLatencySamples();
    std::vector<float> l (4096), r (4096);
    std::mt19937 rng (5);
    std::uniform_real_distribution<float> d (-0.99f, 0.99f);
    for (size_t i = 0; i < l.size(); ++i)
    {
        l[i] = d (rng);
        r[i] = d (rng);
    }
    auto lo = l, ro = r;
    float* p[2] = { lo.data(), ro.data() };
    e.process (p, 4096);
    for (size_t i = size_t (H); i < l.size(); ++i)
    {
        REQUIRE (lo[i] == l[i - size_t (H)]);
        REQUIRE (ro[i] == r[i - size_t (H)]);
    }
}

TEST_CASE ("Mix is a linear, sample-aligned crossfade (+-0.01 dB)", "[engine][mix]")
{
    const double fs = 48000.0;
    // steady -6 dBFS 1 kHz at T -24, R 4: compressed gain c = -7.64 dB re input
    Settings full = trajectorySettings();
    const auto x = sine (fs, 1000.0, dbToLin (-6.0), size_t (1.0 * fs));
    const size_t a = size_t (0.5 * fs), b = x.size();
    const auto yc = runAligned (stereo (x), fs, full).l;
    for (double m : { 0.0, 25.0, 50.0, 75.0 })
    {
        Settings s = full;
        s.mixPct = m;
        const auto y = runAligned (stereo (x), fs, s).l;
        // expected: (1 - m) x + m c, sample by sample
        double err = 0.0;
        for (size_t i = a; i < b; ++i)
            err = std::max (err, std::abs (y[i] - ((1.0 - m / 100.0) * x[i] + m / 100.0 * yc[i])));
        CHECK (err < 1.0e-9);
        const double expected = linToDb ((1.0 - m / 100.0) + m / 100.0 * dbToLin (-7.64));
        CHECK (linToDb (rms (y, a, b) / rms (x, a, b)) == Approx (expected).margin (0.2 + 0.01));
    }
}

TEST_CASE ("Output gain is exact (+-0.01 dB)", "[engine][output]")
{
    const double fs = 48000.0;
    const auto x = gaussianNoise (48000, dbToLin (-45.0), 4u); // peaks stay below 0 dBFS at +18 dB
    for (double o : { -18.0, -6.5, 0.0, 3.0, 18.0 })
    {
        Settings s = neutralSettings();
        s.outputDb = o;
        const auto y = runAligned (stereo (x), fs, s).l;
        double maxErr = 0.0;
        for (size_t i = 4800; i < x.size(); ++i)
            maxErr = std::max (maxErr, std::abs (y[i] - x[i] * dbToLin (o)));
        CHECK (maxErr < 1.0e-12);
    }
}

TEST_CASE ("Output peak stage: never above 1.0, untouched below, S-shaped look-ahead ramp", "[engine][stage]")
{
    const double fs = 48000.0;
    SECTION ("ramp before a +12 dB step (section 11, +-1 dB)")
    {
        Settings s = neutralSettings();
        std::vector<double> x (9600, 0.0);
        const size_t step = 4800;
        for (size_t i = step; i < x.size(); ++i)
            x[i] = 4.0; // +12 dBFS DC step
        // a small probe signal before the step reveals the gain: use a constant 0.01 before
        for (size_t i = 0; i < step; ++i)
            x[i] = 0.01;
        const auto y = runAligned (mono (x), fs, s).l;
        const double expected[5][2] = { { 50, -0.4 }, { 40, -1.6 }, { 30, -3.7 }, { 20, -6.7 }, { 10, -9.8 } };
        for (const auto& e : expected)
        {
            const double g = linToDb (y[step - size_t (e[0])] / 0.01);
            INFO (e[0] << " samples before the step: " << g);
            CHECK (g == Approx (e[1]).margin (1.0));
        }
        CHECK (y[step] == Approx (1.0).margin (1.0e-9)); // -12.04 dB: exactly at the ceiling
        CHECK (y[step - 60] == 0.01); // nothing earlier than ~57 samples
        CHECK (maxAbs (y) <= 1.0);
    }
    SECTION ("steady over-ceiling sine -> clean sine of peak 1.0")
    {
        for (int mode : { 0, 2 })
        {
            Settings s = neutralSettings();
            s.analog = mode;
            s.outputDb = 6.0;
            const auto r = measureTone (fs, s, 1000.0, 0.9, 0.5, 0.5);
            const auto x = sine (fs, 1000.0, 0.9, size_t (fs));
            const auto y = runAligned (stereo (x), fs, s).l;
            const double peak = maxAbs (std::vector<double> (y.begin() + 24000, y.end()));
            INFO ("Analog " << mode << " peak " << peak << " H3 " << r.h3Dbc);
            CHECK (peak <= 1.0);
            CHECK (peak > 0.99);
            if (mode == 0)
                CHECK (r.h3Dbc < -150.0);
        }
    }
    SECTION ("never exceeds 1.0: bursts, all analog modes, both settings; Limiter == Clipper (D3)")
    {
        for (int mode = 0; mode <= 4; ++mode)
            for (bool lim : { false, true })
            {
                Settings s = trajectorySettings();
                s.analog = mode;
                s.outputDb = 18.0;
                s.limiter = lim;
                auto x = gaussianNoise (48000, 0.5, 11u);
                for (size_t i = 0; i < x.size(); ++i)
                    x[i] *= std::exp (-double (i % 12000) / 2000.0) * 4.0; // decaying bursts, +12 dB over
                const auto y = runAligned (stereo (x), fs, s);
                CHECK (maxAbs (y.l) <= 1.0);
                CHECK (maxAbs (y.r) <= 1.0);
            }
    }
    SECTION ("signals below 0 dBFS are untouched by the stage")
    {
        Settings s = neutralSettings();
        s.outputDb = -1.0;
        const auto x = gaussianNoise (48000, 0.3, 12u);
        auto xc = x;
        for (auto& v : xc)
            v = std::clamp (v, -1.1, 1.1);
        const auto y = runAligned (mono (xc), fs, s).l;
        for (size_t i = 0; i < xc.size(); ++i)
            REQUIRE (y[i] == xc[i] * dbToLin (-1.0));
    }
}

TEST_CASE ("Parameter changes are smoothed and settle within 20 ms (D5)", "[engine][smoothing]")
{
    const double fs = 48000.0;
    PunchEngine e;
    e.prepare (fs, 1);
    Settings s = trajectorySettings();
    e.setSettings (s);
    e.reset();
    // steady -6 dBFS sine, then change Output by 12 dB and Threshold by 12 dB at once
    const size_t n = size_t (0.6 * fs);
    std::vector<double> x = sine (fs, 1000.0, 0.25, n), y = x;
    const size_t change = size_t (0.3 * fs);
    double* p[1] = { y.data() };
    e.process (p, int (change));
    s.outputDb = -12.0;
    s.mixPct = 50.0;
    e.setSettings (s);
    p[0] = y.data() + change;
    e.process (p, int (n - change));
    const int H = e.getLatencySamples();
    // envelope (per-ms peak ratio) never jumps (smoothed: at most about 3 dB per ms for a 12 dB change)
    double prev = 0.0;
    for (size_t ms = 250; ms < 590; ++ms)
    {
        double pk = 0.0;
        for (size_t i = ms * 48; i < ms * 48 + 48; ++i)
            pk = std::max (pk, std::abs (y[i]));
        const double db = linToDb (pk);
        if (ms > 250)
            CHECK (std::abs (db - prev) < 4.0); // a 12 dB step would jump by 12 dB
        prev = db;
    }
    // settled: 20 ms after the change, within 0.1 dB of the final level
    auto level = [&] (size_t from) {
        double pk = 0.0;
        for (size_t i = from; i < from + 480; ++i)
            pk = std::max (pk, std::abs (y[i]));
        return linToDb (pk);
    };
    CHECK (level (change + size_t (H) + 960) == Approx (level (n - 480)).margin (0.1));
}

TEST_CASE ("Robustness: silence, denormals, full scale, huge levels, random settings", "[engine][robust]")
{
    std::mt19937 rng (77);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    for (int trial = 0; trial < 40; ++trial)
    {
        Settings s;
        s.thresholdDb = param::Threshold::fromNorm (u (rng));
        s.ratio = param::Ratio::fromNorm (u (rng));
        s.mixPct = param::Mix::fromNorm (u (rng));
        s.attackMs = param::Attack::fromNorm (u (rng));
        s.punch = param::Punch::fromNorm (u (rng));
        s.releaseMs = param::Release::fromNorm (u (rng));
        s.sync = param::syncFromNorm (u (rng));
        s.note = int (param::Note::fromNorm (u (rng)));
        s.manualBpm = param::Tempo::fromNorm (u (rng));
        s.analog = param::analogFromNorm (u (rng));
        s.outputDb = param::Output::fromNorm (u (rng));
        const double fs = trial % 3 == 0 ? 44100.0 : (trial % 3 == 1 ? 96000.0 : 192000.0);
        PunchEngine e;
        e.prepare (fs, 1 + trial % 2);
        e.setSettings (s);
        e.reset();
        std::vector<double> l (4096), r (4096);
        for (int block = 0; block < 12; ++block)
        {
            for (size_t i = 0; i < l.size(); ++i)
            {
                double v = 0.0;
                switch (block % 6)
                {
                    case 0: v = 0.0; break;
                    case 1: v = 1.0e-310 * (i % 2 ? 1 : -1); break;
                    case 2: v = (i % 2 ? 1.0 : -1.0); break;
                    case 3: v = 100.0 * (u (rng) * 2.0 - 1.0); break;
                    case 4: v = u (rng) * 2.0 - 1.0; break;
                    default: v = i == 7 ? 1.0 : 0.0; break;
                }
                l[i] = v;
                r[i] = -v;
            }
            double* p[2] = { l.data(), r.data() };
            e.process (p, int (l.size()));
            for (size_t i = 0; i < l.size(); ++i)
            {
                REQUIRE (std::isfinite (l[i]));
                REQUIRE (std::abs (l[i]) <= 1.0);
            }
            if (block == 6)
            {
                s.analog = (s.analog + 1) % 5;
                s.sync = param::Sync::Host;
                s.hostBpm = 87.0;
                e.setSettings (s);
            }
        }
    }
}
