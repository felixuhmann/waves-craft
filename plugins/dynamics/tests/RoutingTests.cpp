// Signal flow, key modes, latency, neutral, variants, smoothing (spec 1, 3, 8, 10.7).
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

TEST_CASE ("Neutral: bit-exact passthrough (bypassed modules, and default settings)", "[neutral]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const int n = int (0.5 * fs);
        auto l = noise (n, 1u, 0.3), r = noise (n, 2u, 0.3);
        Settings bypassed;
        bypassed.compBypass = bypassed.gateBypass = true;
        bypassed.compThresholdDb = -50.0;
        bypassed.ratioPos = 0.7;
        bypassed.gateOpenDb = -20.0;
        for (const Settings& s : { bypassed, Settings {} })
            for (auto v : { Variant::Full, Variant::Compressor, Variant::Gate, Variant::CompressorFilter })
            {
                auto e = makeEngine (s, fs, v);
                auto out = runStereo (e, l, r, 173);
                CHECK (out.l == l);
                CHECK (out.r == r);
            }
        // float processing is bit-exact as well
        DynamicsEngine e (Variant::Full);
        e.setSettings (Settings {});
        e.prepare (fs);
        std::vector<float> fl (l.begin(), l.end()), fr (r.begin(), r.end());
        auto fl0 = fl, fr0 = fr;
        e.process (fl.data(), fr.data(), n);
        CHECK (fl == fl0);
        CHECK (fr == fr0);
    }
}

TEST_CASE ("Lookahead: audio delayed by exactly 340 samples at every rate; detectors undelayed", "[latency]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Settings s;
        s.lookahead = true;
        auto e = makeEngine (s, fs);
        CHECK (e.getLatencySamples() == 340);
        const int n = 4000;
        auto l = noise (n, 3u, 0.2), r = noise (n, 4u, 0.2);
        auto out = runStereo (e, l, r);
        for (int i = 0; i < n; ++i)
        {
            const double expL = i >= 340 ? l[size_t (i - 340)] : 0.0;
            if (out.l[size_t (i)] != expL || out.r[size_t (i)] != (i >= 340 ? r[size_t (i - 340)] : 0.0))
            {
                FAIL ("sample " << i << " not delayed by 340");
            }
        }
        s.lookahead = false;
        auto e0 = makeEngine (s, fs);
        CHECK (e0.getLatencySamples() == 0);
        // variants without lookahead never report latency
        s.lookahead = true;
        CHECK (makeEngine (s, fs, Variant::Compressor).getLatencySamples() == 0);
        CHECK (makeEngine (s, fs, Variant::Gate).getLatencySamples() == 0);
        CHECK (makeEngine (s, fs, Variant::CompressorFilter).getLatencySamples() == 340);
    }

    // the gain reduction leads the delayed audio: a burst is already compressed at its first sample
    const double fs = 48000.0;
    Settings s;
    s.lookahead = true;
    s.ratioPos = 0.8;
    s.compThresholdDb = -40.0;
    s.compAttackMs = 0.5;
    const int pre = 4800, n = 9600;
    auto in = sine (n, fs, 1000.0, [&] (int i) { return i >= pre ? dbToGain (-6.0) : dbToGain (-60.0); });
    auto e = makeEngine (s, fs);
    auto out = runStereo (e, in, in);
    // the first 1 ms of the burst at the output (input index pre) is already reduced by > 10 dB
    double num = 0.0, den = 0.0;
    for (int i = pre + 340; i < pre + 340 + 48; ++i)
        num += out.l[size_t (i)] * out.l[size_t (i)], den += in[size_t (i - 340)] * in[size_t (i - 340)];
    CHECK (10.0 * std::log10 (num / den) < -10.0);
}

TEST_CASE ("10.7 key modes", "[key]")
{
    const double fs = 48000.0;
    const int n = int (0.4 * fs);
    Settings s;
    s.ratioPos = 0.8;
    s.compThresholdDb = -40.0;
    auto loud = sine (n, fs, 1000.0, -10.0);
    auto quiet = sine (n, fs, 1000.0, -40.0);
    const double gLoud = steadyGainDb (s, fs, -10.0);
    const double gQuiet = steadyGainDb (s, fs, -40.0);

    s.key = KeyMode::LeftToRight;
    {
        auto e = makeEngine (s, fs);
        auto out = runStereo (e, loud, quiet);
        CHECK (rms (out.l, 0, n) == 0.0);                               // L output silent
        CHECK (ratioDb (out.r, quiet, n / 2, n) == Approx (gLoud).margin (0.02)); // R gets L's gain
    }
    {
        auto e = makeEngine (s, fs);
        auto out = runStereo (e, quiet, loud);
        // only L drives the detector: the loud R is (almost) not reduced
        CHECK (ratioDb (out.r, loud, n / 2, n) == Approx (gQuiet).margin (0.05));
    }
    s.key = KeyMode::RightToLeft;
    {
        auto e = makeEngine (s, fs);
        auto out = runStereo (e, quiet, loud);
        CHECK (rms (out.r, 0, n) == 0.0);
        CHECK (ratioDb (out.l, quiet, n / 2, n) == Approx (gLoud).margin (0.02));
    }
    // sidechain monitor in a key mode: the key signal on the output channel
    s.key = KeyMode::LeftToRight;
    s.monitor = Monitor::Sidechain;
    {
        auto e = makeEngine (s, fs);
        auto out = runStereo (e, loud, quiet);
        CHECK (out.r == loud);
        CHECK (rms (out.l, 0, n) == 0.0);
    }
}

TEST_CASE ("Variants: identical to the Full engine with the absent module bypassed", "[variants]")
{
    const double fs = 48000.0;
    const int n = int (0.3 * fs);
    auto l = sine (n, fs, 440.0, [] (int i) { return 0.05 + 0.6 * std::abs (std::sin (i * 0.0007)); });
    auto r = noise (n, 9u, 0.1);
    Settings s;
    s.ratioPos = 0.55;
    s.compThresholdDb = -30.0;
    s.makeupDb = 4.0;
    s.pdrMs = 47.0;
    s.peakRef = true;
    s.gateOpenDb = -25.0;
    s.floorValue = -20.0;
    s.holdMs = 20.0;
    s.outputGainDb = -1.5;

    auto full = s;
    full.gateBypass = true;
    auto eFull = makeEngine (full, fs, Variant::Full);
    auto eComp = makeEngine (s, fs, Variant::Compressor);
    CHECK (runStereo (eFull, l, r).l == runStereo (eComp, l, r).l);

    full = s;
    full.compBypass = true;
    auto eFull2 = makeEngine (full, fs, Variant::Full);
    auto eGate = makeEngine (s, fs, Variant::Gate);
    CHECK (runStereo (eFull2, l, r).r == runStereo (eGate, l, r).r);

    full = s;
    full.gateBypass = true;
    full.compEq = EqMode::Sidechain;
    full.filterType = FilterType::HighPass;
    full.filterFreqHz = 300.0;
    auto sc = full;
    sc.gateBypass = false;
    auto eFull3 = makeEngine (full, fs, Variant::Full);
    auto eSc = makeEngine (sc, fs, Variant::CompressorFilter);
    CHECK (runStereo (eFull3, l, r).l == runStereo (eSc, l, r).l);

    // mono: the same engine on one channel equals the left channel of a dual-mono stereo run
    auto eMono = makeEngine (s, fs, Variant::Full);
    auto m = l;
    run (eMono, m, nullptr);
    auto eStereo = makeEngine (s, fs, Variant::Full);
    CHECK (runStereo (eStereo, l, l).l == m);
}

TEST_CASE ("Smoothing and crossfades are click-free and settle within 20 ms", "[smoothing]")
{
    const double fs = 48000.0;
    const int n = int (0.6 * fs);
    auto in = sine (n, fs, 100.0, -6.0); // low frequency: small sample-to-sample steps
    const double maxStepIn = 2.0 * pi * 100.0 / fs * dbToGain (-6.0);

    auto maxStep = [] (const std::vector<double>& v, int from, int to) {
        double m = 0.0;
        for (int i = from + 1; i < to; ++i)
            m = std::max (m, std::abs (v[size_t (i)] - v[size_t (i - 1)]));
        return m;
    };

    // toggles (crossfade) and big parameter jumps, applied between blocks
    Settings s;
    s.ratioPos = 0.8;
    s.compThresholdDb = -40.0;
    DynamicsEngine e (Variant::Full);
    e.setSettings (s);
    e.prepare (fs);
    std::vector<double> l = in, r = in;
    const int block = 480;
    int k = 0;
    for (int i = 0; i < n; i += block, ++k)
    {
        if (k == 20) s.compBypass = true;
        if (k == 30) s.compBypass = false;
        if (k == 40) s.makeupDb = 20.0;
        if (k == 45) s.compThresholdDb = -10.0;
        if (k == 50) s.lookahead = true;
        if (k == 55) s.compEq = EqMode::Split;
        if (k == 60) s.outputGainDb = -20.0;
        e.setSettings (s);
        e.process (l.data() + i, r.data() + i, std::min (block, n - i));
    }
    // no step much larger than the steepest step of the unprocessed sine scaled by the largest gain in play
    CHECK (maxStep (l, 1000, n) < maxStepIn * dbToGain (20.0) * 1.5);

    // a makeup change settles within 20 ms
    Settings a;
    a.ratioPos = ratio::neutralPos;
    DynamicsEngine e2 (Variant::Full);
    e2.setSettings (a);
    e2.prepare (fs);
    auto x = sine (int (0.2 * fs), fs, 1000.0, -20.0), y = x;
    e2.process (x.data(), y.data(), 4800);
    a.makeupDb = 10.0;
    e2.setSettings (a);
    e2.process (x.data() + 4800, y.data() + 4800, int (x.size()) - 4800);
    const auto in2 = sine (int (0.2 * fs), fs, 1000.0, -20.0);
    CHECK (ratioDb (x, in2, 4800 + 960, 4800 + 960 + 480) == Approx (10.0).margin (0.05));
}

TEST_CASE ("Output limit catches onsets from silence at extreme gains (no runaway)", "[limit]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        Settings s;
        s.ratioPos = 0.8;
        s.compThresholdDb = -80.0;
        s.peakRef = true;        // +40 dB offset
        s.makeupDb = 40.0;
        s.compAttackMs = 1000.0; // the detector catches up only slowly
        s.outputGainDb = 18.0;
        s.gateOpenDb = -10.0;    // gate closed at low level ...
        s.floorValue = 12.0;     // ... with a +12 dB floor
        const int n = int (0.6 * fs), start = int (0.3 * fs);
        std::vector<double> in (static_cast<size_t> (n), 0.0);
        for (int i = start; i < n; ++i)
            in[size_t (i)] = ((i / 40) % 2 == 0) ? 1.0 : -1.0; // full-scale square
        auto e = makeEngine (s, fs);
        auto out = runStereo (e, in, in);
        double peak = 0.0;
        for (double v : out.l)
            peak = std::max (peak, std::abs (v));
        INFO ("fs " << fs << " peak " << 20.0 * std::log10 (peak) << " dBFS");
        // limited module output <= +2.83 dBFS, then the gate floor +12 dB and the output gain +18 dB
        CHECK (20.0 * std::log10 (peak) <= 2.9 + 12.0 + 18.0);
    }
}
