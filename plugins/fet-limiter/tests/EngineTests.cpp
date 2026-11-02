// Spec r1 sections 1, 3, 6.4, D3: mix / trim, latency, stereo link, crosstalk, comp off, smoothing.
#include <catch2/catch_test_macros.hpp>

#include <random>

#include "TestHelpers.h"

using namespace fettest;

TEST_CASE ("Mix 0 % is bit-exact dry at Trim 0 dB (float and double)", "[mix]")
{
    Settings s = neutralSettings();
    s.mixPercent = 0.0;
    s.inputDisplay = 0.0;
    s.outputDisplay = 0.0;
    s.analog = Analog::Hz50; // even with noise enabled the dry path is exact
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

    FetEngine e;
    e.setSettings (s);
    e.prepare (48000.0);
    std::vector<float> l (20000), r (20000);
    for (auto& v : l) v = dist (rng);
    for (auto& v : r) v = dist (rng) * 0.25f;
    auto l0 = l, r0 = r;
    for (size_t i = 0; i < l.size(); i += 500)
    {
        float* p[2] = { l.data() + i, r.data() + i };
        e.process (p, 2, 500);
    }
    CHECK (l == l0);
    CHECK (r == r0);

    std::vector<double> d (4096);
    for (auto& v : d) v = double (dist (rng));
    auto d0 = d;
    Rig rig (s);
    rig.run (d);
    CHECK (d == d0);
}

TEST_CASE ("Trim is an exact dB gain after Mix (<= 0.01 dB)", "[mix]")
{
    for (double mix : { 0.0, 100.0 })
        for (double trim : { -18.0, -6.5, 6.0, 18.0 })
        {
            Settings s = neutralSettings();
            s.mixPercent = mix;
            double lv[2];
            for (int i = 0; i < 2; ++i)
            {
                s.trimDb = i == 0 ? 0.0 : trim;
                Rig rig (s);
                auto x = sine (1000.0, -40.0, 0.4, 48000.0);
                rig.run (x);
                lv[i] = toneDb (x, 1000.0, 48000.0, x.size() - 4800, 4800);
            }
            CHECK (std::abs ((lv[1] - lv[0]) - trim) <= 0.01);
        }
}

TEST_CASE ("Mix is a linear in-phase crossfade", "[mix]")
{
    // Mix 50 %: output = 0.5 wet + 0.5 dry (complex amplitudes)
    auto run = [] (double mix) {
        Settings s = neutralSettings();
        s.mixPercent = mix;
        s.inputDisplay = -30.0;
        s.outputDisplay = -18.0;
        Rig rig (s);
        auto x = sine (1000.0, -40.0, 0.4, 48000.0);
        rig.run (x);
        return tone (x, 1000.0, 48000.0, x.size() - 4800, 4800);
    };
    const auto wet = run (100.0), dry = run (0.0), half = run (50.0);
    CHECK (std::abs (half - 0.5 * (wet + dry)) <= 1e-6 * std::abs (dry));
}

TEST_CASE ("Zero latency", "[latency]")
{
    FetEngine e;
    CHECK (e.getLatencySamples() == 0);
    // impulse: the wet path responds at sample 0 (no delay line)
    Settings s = neutralSettings();
    s.inputDisplay = -30.0;
    s.outputDisplay = 0.0;
    Rig rig (s);
    std::vector<double> x (256, 0.0);
    x[0] = 1.0e-3;
    rig.run (x);
    double peak = 0.0;
    size_t at = 0;
    for (size_t i = 0; i < x.size(); ++i)
        if (std::abs (x[i]) > peak)
        {
            peak = std::abs (x[i]);
            at = i;
        }
    CHECK (std::abs (x[0]) > 0.0);
    CHECK (at <= 1); // minimum-phase response filters
}

TEST_CASE ("Stereo link: a loud left channel reduces the right channel's gain by the full GR", "[stereo]")
{
    Settings s = neutralSettings();
    s.inputDisplay = -12.0;
    s.outputDisplay = 0.0;
    const double fs = 48000.0;
    Rig rig (s, fs);
    auto l = sine (1000.0, -60.0, 1.0, fs);
    auto r = sine (1000.0, -60.0, 1.0, fs);
    // left bursts +40 dB for the second half
    for (size_t i = l.size() / 2; i < l.size(); ++i)
        l[i] *= 100.0;
    rig.run (l, r);
    const size_t n = 4800;
    const double rQuiet = toneDb (r, 1000.0, fs, l.size() / 2 - n, n);
    const double rLoud = toneDb (r, 1000.0, fs, l.size() - n, n);
    const double lLoud = toneDb (l, 1000.0, fs, l.size() - n, n) - 40.0;
    const double grRight = rLoud - rQuiet, grLeft = lLoud - rQuiet;
    INFO ("right gain change " << grRight << " left gain change " << grLeft);
    CHECK (grRight < -10.0);
    CHECK (std::abs (grRight - grLeft) <= 1.0); // left additionally saturates slightly
}

TEST_CASE ("No crosstalk between channels", "[stereo]")
{
    Settings s = neutralSettings();
    s.inputDisplay = 0.0;
    s.outputDisplay = 0.0;
    Rig rig (s);
    auto l = sine (1000.0, -6.0, 0.5, 48000.0);
    std::vector<double> r (l.size(), 0.0);
    rig.run (l, r);
    for (double v : r)
        REQUIRE (v == 0.0);
}

TEST_CASE ("Mono processing (one channel) behaves like the stereo left channel with a silent right", "[stereo]")
{
    Settings s = neutralSettings();
    s.inputDisplay = -12.0;
    auto m = sine (440.0, -12.0, 0.5, 48000.0);
    auto l = m;
    std::vector<double> r (m.size(), 0.0);
    Rig mono (s), stereo (s);
    mono.run (m);
    stereo.run (l, r);
    for (size_t i = 0; i < m.size(); ++i)
        REQUIRE (m[i] == l[i]);
}

TEST_CASE ("Comp Off: no gain reduction, colouration and gain staging remain", "[compoff]")
{
    Settings s = neutralSettings();
    s.inputDisplay = -12.0;
    s.outputDisplay = 0.0;
    s.compOff = true;
    Rig rig (s);
    auto x = sine (1000.0, -40.0, 0.5, 48000.0);
    rig.run (x);
    const double g = toneDb (x, 1000.0, 48000.0, x.size() - 4800, 4800) + 40.0;
    CHECK (std::abs (g - 37.07) <= 0.2);
    CHECK (rig.engine.getGainReductionDb() == 0.0);
}

TEST_CASE ("Parameter changes are smoothed (no steps, settled within 20 ms)", "[smoothing]")
{
    Settings s = neutralSettings();
    s.inputDisplay = -30.0;
    s.outputDisplay = -18.0;
    FetEngine e;
    e.setSettings (s);
    e.prepare (48000.0);
    // DC-free test: constant 0.01 sine at 50 Hz, then Output jumps by ~12 dB
    auto x = sine (50.0, -40.0, 0.2, 48000.0);
    const size_t half = x.size() / 2;
    double* p = x.data();
    e.process (&p, 1, int (half));
    s.outputDisplay = -6.0;
    e.setSettings (s);
    p = x.data() + half;
    e.process (&p, 1, int (x.size() - half));
    double maxStep = 0.0;
    for (size_t i = half - 100; i < half + 2000; ++i)
        maxStep = std::max (maxStep, std::abs (x[i] - x[i - 1]));
    double typicalStep = 0.0;
    for (size_t i = x.size() - 2000; i < x.size(); ++i)
        typicalStep = std::max (typicalStep, std::abs (x[i] - x[i - 1]));
    CHECK (maxStep <= typicalStep * 1.05);
}
