// Engine-level behaviour: latency (section 1 / D1), direct path, channel independence (6.3), aliasing (7 / D1),
// small-signal linearity (5), parameter smoothing (D2), robustness.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <random>

#include "SpecTables.h"
#include "TestHelpers.h"

using namespace ddtest;
using Catch::Approx;

TEST_CASE ("Latency is reported, constant and <= 64 samples", "[latency]")
{
    for (double fs : { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        DynamicDistortion e;
        e.prepare (fs);
        CHECK (e.getLatencySamples() > 0);
        CHECK (e.getLatencySamples() <= 64);
        CHECK (e.getLatencySamples() == 37);
    }
}

TEST_CASE ("Direct path is the latency-aligned input after Input gain, bit-exact", "[direct][latency]")
{
    const double fs = 48000.0;
    std::mt19937 rng (7);
    std::uniform_real_distribution<double> dist (-1.0, 1.0);
    std::vector<double> x (4000);
    for (auto& v : x)
        v = dist (rng);
    Settings s = neutralSettings();
    s.distPercent = 0.0;
    DynamicDistortion e;
    e.setSettings (s);
    e.prepare (fs);
    const auto y = run (e, x, 333);
    const size_t lat = size_t (e.getLatencySamples());
    for (size_t i = 0; i + lat < x.size(); ++i)
        REQUIRE (y[i + lat] == x[i]);
    for (size_t i = 0; i < lat; ++i)
        REQUIRE (y[i] == 0.0);
}

TEST_CASE ("Distortion path is time-aligned with the direct path", "[latency]")
{
    // the distortion path's impulse response peaks within a sample of the reported latency (Drive 0 %, flat EQ)
    const double fs = 48000.0;
    auto s = distortionOnly (0.0);
    s.lowPassHz = 21000.0;
    const auto ir = impulseResponse (s, fs, 0.05);
    size_t peak = 0;
    for (size_t i = 0; i < ir.size(); ++i)
        if (std::abs (ir[i]) > std::abs (ir[peak]))
            peak = i;
    CHECK (std::abs (double (peak) - 37.0) <= 1.0);
}

TEST_CASE ("Channels are processed independently (section 6.3)", "[channels]")
{
    const double fs = 48000.0;
    Settings s; // defaults: both paths active
    s.drivePercent = 80.0;
    const size_t n = 20000;
    std::vector<double> l (n), r (n, 0.0);
    for (size_t i = 0; i < n; ++i)
        l[i] = 0.5 * std::sin (2.0 * pi * 220.0 * double (i) / fs);
    DynamicDistortion e;
    e.setSettings (s);
    e.prepare (fs);
    double* ptr[2] = { l.data(), r.data() };
    e.process (ptr, 2, int (n));
    for (double v : r)
        REQUIRE (v == 0.0);

    // the right channel's output depends only on its own signal; mono processing equals either channel
    std::vector<double> a (n), b (n), mono (n);
    std::mt19937 rng (3);
    std::normal_distribution<double> nd (0.0, 0.2);
    for (size_t i = 0; i < n; ++i)
    {
        a[i] = nd (rng);
        b[i] = 0.3 * std::sin (2.0 * pi * 90.0 * double (i) / fs);
        mono[i] = b[i];
    }
    DynamicDistortion st, mo;
    st.setSettings (s);
    st.prepare (fs);
    mo.setSettings (s);
    mo.prepare (fs);
    double* sp[2] = { a.data(), b.data() };
    st.process (sp, 2, int (n));
    double* mp[1] = { mono.data() };
    mo.process (mp, 1, int (n));
    for (size_t i = 0; i < n; ++i)
        REQUIRE (b[i] == mono[i]);
}

TEST_CASE ("Anti-aliased clipper: 15 kHz, -6 dBFS, Drive 50 % aliases <= -40 dBc (D1)", "[aliasing]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (fs);
    const double amp = sinePeak (-6.0), f = 15000.0;
    const size_t settle = size_t (0.3 * fs), n = size_t (0.5 * fs);
    auto y = render (distortionOnly (50.0), fs, settle + n, [&] (size_t i) { return amp * std::sin (2.0 * pi * f * double (i) / fs); });
    const double fund = toneAmplitude (y, settle, n, f, fs);
    CHECK (db (toneAmplitude (y, settle, n, 3000.0, fs) / fund) <= -40.0);
    double worst = -300.0;
    for (double g = 20.0; g < 0.5 * fs - 20.0; g += 10.0)
    {
        const double m = std::fmod (g, f);
        if (m < 30.0 || f - m < 30.0)
            continue; // the tone and its harmonics
        worst = std::max (worst, db (toneAmplitude (y, settle, n, g, fs) / fund));
    }
    CHECK (worst <= -40.0);
}

TEST_CASE ("Small signals are linear (section 5)", "[linear]")
{
    // harmonics at -50 dBFS <= -50 dBc for every Drive / Attack setting
    for (double drive : { 0.0, 50.0, 100.0 })
        for (double attack : { 0.1, 1.0, 100.0 })
            for (double f : { 50.0, 1000.0 })
            {
                const auto r = measureTone (distortionOnly (drive, attack, attack < 1.0 ? 5.0 : 31.8), 48000.0, f, -50.0, 0.4, 0.2);
                CAPTURE (drive, attack, f);
                CHECK (r.h3Dbc <= -50.0);
                CHECK (r.h2Dbc <= -50.0);
            }
    // gain = Drive table x EQ x HP x LP x Distortion law, direct path = Direct law
    Settings s;
    s.directPercent = 0.0;
    s.distPercent = 75.0;
    s.drivePercent = 30.0;
    s.bass = s.mid = s.treble = 0.0;
    s.lowPassHz = 21000.0;
    CHECK (smallSignalDb (s, 48000.0, 1000.0) == Approx (20.30 - 9.0).margin (0.1));
}

TEST_CASE ("Parameter changes are smoothed and settle within 20 ms (D2)", "[smoothing]")
{
    const double fs = 48000.0;
    struct Change
    {
        const char* name;
        std::function<void (Settings&)> apply;
    };
    const std::vector<Change> changes {
        { "input", [] (Settings& s) { s.inputDb = 12.0; } },
        { "direct", [] (Settings& s) { s.directPercent = 10.0; } },
        { "drive", [] (Settings& s) { s.drivePercent = 100.0; } },
        { "distortion", [] (Settings& s) { s.distPercent = 100.0; } },
        { "bass", [] (Settings& s) { s.bass = 50.0; } },
        { "mid", [] (Settings& s) { s.mid = -50.0; } },
        { "midfreq", [] (Settings& s) { s.midFreq = 300.0; } },
        { "treble", [] (Settings& s) { s.treble = 50.0; } },
        { "lowpass", [] (Settings& s) { s.lowPassHz = 600.0; } },
        { "output", [] (Settings& s) { s.outputDb = -18.0; } },
        { "bypass", [] (Settings& s) { s.bypass = true; } },
    };
    for (const auto& c : changes)
    {
        CAPTURE (c.name);
        Settings a;
        a.drivePercent = 20.0;
        Settings b = a;
        c.apply (b);
        const size_t n0 = size_t (0.3 * fs), n1 = size_t (0.2 * fs);
        auto input = [&] (size_t i) { return 0.05 * std::sin (2.0 * pi * 440.0 * double (i) / fs); };

        // switched engine: a for n0 samples, then b
        DynamicDistortion e;
        e.setSettings (a);
        e.prepare (fs);
        std::vector<double> x (n0 + n1);
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = input (i);
        std::vector<double> y (x);
        double* p0[1] = { y.data() };
        e.process (p0, 1, int (n0));
        e.setSettings (b);
        double* p1[1] = { y.data() + n0 };
        e.process (p1, 1, int (n1));

        // reference engine running b all along
        DynamicDistortion ref;
        ref.setSettings (b);
        ref.prepare (fs);
        const auto yr = run (ref, x);

        // no click: the largest sample step after the change stays within a small multiple of the largest steady step
        double steadyStep = 0.0, changeStep = 0.0;
        for (size_t i = n0 - 2000; i < n0; ++i)
            steadyStep = std::max (steadyStep, std::abs (y[i] - y[i - 1]));
        for (size_t i = n0 + 1; i < n0 + n1; ++i)
            changeStep = std::max (changeStep, std::abs (y[i] - y[i - 1]));
        const double refStep = [&] {
            double m = 0.0;
            for (size_t i = n0 + 1; i < n0 + n1; ++i)
                m = std::max (m, std::abs (yr[i] - yr[i - 1]));
            return m;
        }();
        CHECK (changeStep <= 1.5 * std::max (steadyStep, refStep) + 1e-6);

        // settled: 20 ms after the change the level matches the target settings' output within 0.25 dB
        const size_t from = n0 + size_t (0.02 * fs), len = size_t (0.05 * fs);
        CHECK (db (rms (y, from, len)) == Approx (db (rms (yr, from, len))).margin (0.25));
    }
}

TEST_CASE ("Silence in gives silence out; extreme settings stay finite", "[robustness]")
{
    const double fs = 48000.0;
    Settings s;
    s.drivePercent = 100.0;
    s.inputDb = 18.0;
    s.outputDb = 18.0;
    s.bass = s.mid = s.treble = 50.0;
    s.attackMs = 0.1;
    s.releaseMs = 5.0;
    DynamicDistortion e;
    e.setSettings (s);
    e.prepare (fs);
    std::vector<double> zero (20000, 0.0);
    for (double v : run (e, zero))
        REQUIRE (v == 0.0);

    std::mt19937 rng (11);
    std::uniform_real_distribution<double> d (-1.0, 1.0);
    std::vector<double> loud (48000);
    for (auto& v : loud)
        v = 4.0 * d (rng);
    loud[100] = 1.0e6;
    loud[200] = 1.0e-30;
    double peak = 0.0;
    for (double v : run (e, loud))
    {
        REQUIRE (std::isfinite (v));
        peak = std::max (peak, std::abs (v));
    }
    CHECK (peak < 1.0e6 * dbToGain (36.0) * 1.01); // the direct path passes the 1e6 spike with +18 dB in / out
}

TEST_CASE ("Bypass passes the latency-aligned input", "[bypass]")
{
    const double fs = 48000.0;
    Settings s;
    s.bypass = true;
    DynamicDistortion e;
    e.setSettings (s);
    e.prepare (fs);
    std::vector<double> x (3000);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 0.7 * std::sin (0.01 * double (i));
    const auto y = run (e, x);
    for (size_t i = 0; i + 37 < x.size(); ++i)
        REQUIRE (y[i + 37] == x[i]);
}

TEST_CASE ("Float and double processing agree", "[engine]")
{
    const double fs = 48000.0;
    Settings s;
    std::vector<double> xd (4096);
    std::vector<float> xf (4096);
    for (size_t i = 0; i < xd.size(); ++i)
    {
        xf[i] = float (0.3 * std::sin (2.0 * pi * 330.0 * double (i) / fs));
        xd[i] = double (xf[i]);
    }
    DynamicDistortion a, b;
    a.setSettings (s);
    a.prepare (fs);
    b.setSettings (s);
    b.prepare (fs);
    double* pd[1] = { xd.data() };
    float* pf[1] = { xf.data() };
    a.process (pd, 1, 4096);
    b.process (pf, 1, 4096);
    for (size_t i = 0; i < xd.size(); ++i)
        REQUIRE (double (xf[i]) == Approx (xd[i]).margin (1e-6));
}
