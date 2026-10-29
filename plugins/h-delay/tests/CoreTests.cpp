// Spec r1 sections 1, 3, 4, 5, 9 (glide), 10: deterministic core, timing, ping pong, output stage.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE ("Spec 4 example: impulse 0.5, 100 ms, feedback 60 %", "[core][echoes]")
{
    auto e = makeEngine (coreSettings (100.0, 60.0), 48000.0);
    auto y = impulse (e, 33000, 0.5, 0.5); // 6 echoes

    // first echo at exactly D = 4800, then every D + 1 samples
    const int at[] = { 4800, 9601, 14402, 19203, 24004, 28805 };
    const auto model = modelEchoes (0.5, 0.6, 6);
    for (int k = 0; k < 6; ++k)
    {
        CHECK_THAT (y.l[size_t (at[k])], WithinAbs (model[size_t (k)], 1e-12));
        CHECK_THAT (y.r[size_t (at[k])], WithinAbs (model[size_t (k)], 1e-12));
    }
    // the spec's printed values (rounded to 3 digits)
    const double spec[] = { 0.333, 0.166, 0.090, 0.051 };
    for (int k = 0; k < 4; ++k)
        CHECK (std::abs (db (y.l[size_t (at[k])] / spec[k])) < 0.15);

    // nothing between the echoes (echoes land on exact samples, review decision 11.1)
    for (size_t n = 0; n < y.l.size(); ++n)
        if (std::find (std::begin (at), std::end (at), int (n)) == std::end (at))
            REQUIRE (y.l[n] == 0.0);
}

TEST_CASE ("Echo timing is exact at 44.1 / 48 / 96 kHz, ms and note modes", "[core][timing]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        for (double ms : { 1.0, 37.0, 250.0, 1000.0 })
        {
            auto e = makeEngine (coreSettings (ms, 50.0), fs);
            const int D = int (std::round (ms * fs / 1000.0));
            auto y = impulse (e, 2 * D + 10, 0.25, 0.25);
            CHECK (y.l[size_t (D)] != 0.0);
            CHECK (y.l[size_t (2 * D + 1)] != 0.0);
            CHECK (std::count_if (y.l.begin(), y.l.end(), [] (double v) { return v != 0.0; }) == 2);
        }
        for (int note : { 0, 6, 9, 12, 13, 17 })
        {
            auto s = coreSettings();
            s.sync = param::Sync::Bpm;
            s.bpm = 128.0;
            s.delayNote = note;
            auto e = makeEngine (s, fs);
            const double exact = param::noteBeats (note) * 60.0 / 128.0 * fs;
            const int D = int (std::round (exact));
            auto y = impulse (e, D + 10, 0.25);
            CHECK (y.l[size_t (D)] != 0.0);
            CHECK (std::abs (D - exact) <= 0.5);
        }
    }
}

TEST_CASE ("Host sync uses the host tempo; Dual uses separate L/R times", "[core][timing]")
{
    auto s = coreSettings (100.0, 0.0);
    s.sync = param::Sync::Host;
    s.hostBpm = 90.0;
    s.delayNote = 13; // 1/4 = 666.67 ms at 90 BPM
    auto e = makeEngine (s, 48000.0);
    auto y = impulse (e, 40000, 0.3, 0.3);
    CHECK (y.l[32000] != 0.0);
    CHECK (y.r[32000] != 0.0);

    s.sync = param::Sync::Ms;
    s.dual = true;
    s.delayMs = 100.0;
    s.delayRMs = 250.0;
    auto e2 = makeEngine (s, 48000.0);
    auto y2 = impulse (e2, 13000, 0.3, 0.3);
    CHECK (y2.l[4800] != 0.0);
    CHECK (y2.r[4800] == 0.0);
    CHECK (y2.r[12000] != 0.0);

    // Delay R is ignored when Dual is off
    s.dual = false;
    auto e3 = makeEngine (s, 48000.0);
    auto y3 = impulse (e3, 13000, 0.3, 0.3);
    CHECK (y3.r[4800] != 0.0);
}

TEST_CASE ("Feedback above 100 % self-oscillates but stays bounded by the saturation", "[core]")
{
    auto e = makeEngine (coreSettings (10.0, 200.0), 48000.0);
    auto y = impulse (e, 48000 * 3, 0.5);
    double peakLate = 0.0;
    for (size_t n = 0; n < y.l.size(); ++n)
    {
        REQUIRE (std::abs (y.l[n]) < 1.0);
        if (n > y.l.size() - 4800)
            peakLate = std::max (peakLate, std::abs (y.l[n]));
    }
    CHECK (peakLate > 0.4); // sustained: fixed point of v = 2 sat(v) is |w| = 0.5
}

TEST_CASE ("Ping Pong: spec 5 examples, start Left and start Right", "[core][pingpong]")
{
    for (bool startRight : { false, true })
    {
        auto s = coreSettings (100.0, 60.0);
        s.pingPong = true;
        s.pingPongStartRight = startRight;
        auto e = makeEngine (s, 48000.0);
        auto y = impulse (e, 30000, 0.5, 0.0);

        // model of spec 5
        const double g = 0.6, xm = 0.5 / std::sqrt (2.0);
        auto sat = [] (double v) { return v / (1.0 + std::abs (v)); };
        double first = sat (xm);
        std::vector<double> amp { first };
        for (int k = 1; k < 5; ++k)
        {
            // hop from the side that sounded last: L->R unity, R->L g^2
            const bool lastWasLeft = (k % 2 == 1) != startRight;
            amp.push_back (sat ((lastWasLeft ? 1.0 : g * g) * amp.back()));
        }
        const int at[] = { 4800, 9601, 14402, 19203, 24004 };
        for (int k = 0; k < 5; ++k)
        {
            const bool left = (k % 2 == 0) != startRight;
            const auto& on = left ? y.l : y.r;
            const auto& off = left ? y.r : y.l;
            CHECK_THAT (on[size_t (at[k])], WithinAbs (amp[size_t (k)], 1e-12));
            CHECK (off[size_t (at[k])] == 0.0);
        }

        // the spec's printed amplitudes; tolerance +-0.2 dB for the first three echoes. Later echoes are
        // compared at +-0.4 dB: the printed reference values include the reference's small per-pass
        // interpolation loss, which review decision 11.1 deliberately does not reproduce.
        const double specL[] = { 0.261, 0.206, 0.068, 0.063, 0.022 };
        const double specR[] = { 0.261, 0.085, 0.078, 0.027 };
        const int n = startRight ? 4 : 5;
        for (int k = 0; k < n; ++k)
        {
            const bool left = (k % 2 == 0) != startRight;
            const double got = (left ? y.l : y.r)[size_t (at[k])];
            const double want = startRight ? specR[k] : specL[k];
            CHECK (std::abs (db (got / want)) < (k < 3 ? 0.2 : 0.4));
        }
    }
}

TEST_CASE ("Ping Pong uses the mono sum of a stereo input", "[core][pingpong]")
{
    auto s = coreSettings (50.0, 0.0);
    s.pingPong = true;
    auto e = makeEngine (s, 48000.0);
    auto y = impulse (e, 3000, 0.2, 0.1);
    const double xm = 0.3 / std::sqrt (2.0);
    CHECK_THAT (y.l[2400], WithinAbs (xm / (1 + xm), 1e-12));
    CHECK (y.r[2400] == 0.0);
}

TEST_CASE ("Mix 0 is bit-transparent; output gain and equal-power mix", "[core][output]")
{
    auto s = coreSettings (5.0, 80.0);
    s.analog = 2; // even with an analog mode (noise) the dry path is exact
    s.mixPct = 0.0;
    auto e = makeEngine (s, 48000.0);
    std::vector<float> l (4096), r (4096);
    for (size_t n = 0; n < l.size(); ++n)
    {
        l[n] = float (std::sin (0.01 * double (n)) * 0.9);
        r[n] = float (std::cos (0.013 * double (n)) * 0.5);
    }
    auto lo = l, ro = r;
    e.process (lo.data(), ro.data(), int (lo.size()));
    CHECK (lo == l);
    CHECK (ro == r);

    // Output +6 dB scales the dry signal
    s.analog = 0;
    s.outputDb = 6.0;
    auto e2 = makeEngine (s, 48000.0);
    auto y = impulse (e2, 10, 0.25);
    CHECK_THAT (y.l[0], WithinRel (0.25 * std::pow (10.0, 6.0 / 20.0), 1e-12));

    // Mix 50: dry cos(pi/4), wet sin(pi/4)
    s.outputDb = 0.0;
    s.mixPct = 50.0;
    s.feedbackPct = 0.0;
    auto e3 = makeEngine (s, 48000.0);
    auto y3 = impulse (e3, 400, 0.25);
    CHECK_THAT (y3.l[0], WithinAbs (0.25 * std::cos (pi / 4), 1e-12));
    CHECK_THAT (y3.l[240], WithinAbs (0.2 * std::sin (pi / 4), 1e-12));

    // Mix 100: no dry at all
    s.mixPct = 100.0;
    auto e4 = makeEngine (s, 48000.0);
    auto y4 = impulse (e4, 10, 0.25);
    CHECK (y4.l[0] == 0.0);
}

TEST_CASE ("Latency is zero", "[core]")
{
    CHECK (HybridDelayEngine::getLatencySamples() == 0);
    auto s = coreSettings();
    s.mixPct = 30.0;
    auto e = makeEngine (s, 48000.0);
    auto y = impulse (e, 4, 1.0);
    CHECK (y.l[0] != 0.0);
    CHECK (y.l[1] == 0.0);
}

TEST_CASE ("Phase L / Phase R invert the wet signal of that channel", "[core]")
{
    auto s = coreSettings (10.0, 0.0);
    s.phaseR = true;
    auto e = makeEngine (s, 48000.0);
    auto y = impulse (e, 600, 0.3, 0.3);
    CHECK (y.l[480] > 0.0);
    CHECK_THAT (y.r[480], WithinAbs (-y.l[480], 1e-15));
}

TEST_CASE ("Mono to stereo: the mono input feeds both lines", "[core][mono]")
{
    auto s = coreSettings (10.0, 0.0);
    s.phaseL = true; // Phase R is linked to Phase L
    auto e = makeEngine (s, 48000.0, InputMode::MonoToStereo);
    std::vector<double> l (600, 0.0), r (600, 0.0);
    l[0] = 0.3;
    r[0] = 0.9; // ignored in mono-to-stereo
    auto y = run (e, l, r);
    CHECK_THAT (y.l[480], WithinAbs (-0.3 / 1.3, 1e-12));
    CHECK_THAT (y.r[480], WithinAbs (-0.3 / 1.3, 1e-12));
}

TEST_CASE ("Saturation r2 4.2: compression vs frequency", "[core][sat]")
{
    const double freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000 };
    const double at30[] = { -0.23, -0.24, -0.24, -0.25, -0.26, -0.27, -0.27, -0.27 };
    const double at12[] = { -1.66, -1.71, -1.74, -1.82, -1.88, -1.91, -1.92, -1.92 };
    for (int i = 0; i < 8; ++i)
    {
        for (int lvl = 0; lvl < 2; ++lvl)
        {
            const double a = lvl == 0 ? 0.0316 : 0.251;
            const auto h = toneHarmonics (0, a, freqs[i], 48000.0, 0.3);
            const double got = db (h.h1 / a), want = lvl == 0 ? at30[i] : at12[i];
            INFO (freqs[i] << " Hz, a " << a << ": " << got << " dB (target " << want << ")");
            CHECK (std::abs (got - want) <= 0.15);
        }
    }
}

TEST_CASE ("Saturation r2 4.3: 100 Hz tone through Analog Off", "[core][sat]")
{
    const double amps[] = { 0.03, 0.1, 0.3, 0.95 };
    const double fund[] = { -0.2, -0.8, -2.1, -5.3 };
    const double h3[] = { -52.9, -42.6, -33.5, -24.7 };
    for (int i = 0; i < 4; ++i)
    {
        const auto h = toneHarmonics (0, amps[i], 100.0, 48000.0, 0.3);
        INFO ("a " << amps[i] << ": fund " << db (h.h1 / amps[i]) << " H2 " << db (h.h2 / h.h1) << " H3 " << db (h.h3 / h.h1));
        CHECK (std::abs (db (h.h1 / amps[i]) - fund[i]) <= 0.2);
        CHECK (std::abs (db (h.h3 / h.h1) - h3[i]) <= (i == 3 ? 4.0 : 2.5));
        CHECK (db (h.h2 / h.h1 + 1e-300) <= -150.0);
    }
}

TEST_CASE ("Saturation r2 4.4: per-pass echo ratio at short delays", "[core][sat]")
{
    const double delays[] = { 1, 2, 3, 5, 10, 20 };
    const double table[6][4] = { { 0.419, 0.437, 0.452, 0.467 }, { 0.436, 0.466, 0.491, 0.511 },
                                 { 0.452, 0.490, 0.519, 0.540 }, { 0.479, 0.526, 0.555, 0.573 },
                                 { 0.500, 0.546, 0.569, 0.582 }, { 0.500, 0.546, 0.569, 0.582 } };
    for (double fs : { 44100.0, 48000.0 })
        for (int d = 0; d < 6; ++d)
        {
            auto e = makeEngine (coreSettings (delays[d], 60.0), fs);
            const int D = int (std::round (delays[d] * fs / 1000.0));
            auto y = impulse (e, 6 * (D + 1) + 10, 0.5);
            double peak[5];
            for (int k = 0; k < 5; ++k)
                peak[k] = std::abs (y.l[size_t (D + k * (D + 1))]);
            for (int k = 0; k < 4; ++k)
            {
                const double ratio = peak[k + 1] / peak[k];
                INFO (fs << " Hz, " << delays[d] << " ms, pass " << k + 1 << ": " << ratio << " (target " << table[d][k] << ")");
                CHECK (std::abs (db (ratio / table[d][k])) <= 0.3);
            }
        }
}

TEST_CASE ("Delay-time change glides with tau ~0.2 s and is click-free", "[core][glide]")
{
    const double fs = 48000.0;
    auto s = coreSettings (100.0, 30.0);
    auto e = makeEngine (s, fs);
    const size_t block = 480;
    std::vector<double> l (block), r (block);
    double phase = 0.0;
    double prevOut = 0.0, maxStep = 0.0;
    auto runBlock = [&] {
        for (size_t n = 0; n < block; ++n)
        {
            l[n] = r[n] = 0.5 * std::sin (phase);
            phase += 2.0 * pi * 220.0 / fs;
        }
        e.process (l.data(), r.data(), int (block));
        for (size_t n = 0; n < block; ++n)
        {
            maxStep = std::max (maxStep, std::abs (l[n] - prevOut));
            prevOut = l[n];
        }
    };
    for (int b = 0; b < 100; ++b)
        runBlock();
    const double stepBefore = maxStep;

    s.delayMs = s.delayRMs = 200.0;
    e.setSettings (s);
    std::vector<double> traj;
    for (int b = 0; b < 100; ++b) // 1 s, one value per 10 ms
    {
        runBlock();
        traj.push_back (e.getCurrentDelaySamples (0));
    }
    // 63 % after 0.2 s, 92 % after 0.5 s (tau 0.15 .. 0.25 s)
    auto frac = [&] (double t) { return (traj[size_t (t * 100.0) - 1] - 4800.0) / 4800.0; };
    CHECK (frac (0.2) > 1.0 - std::exp (-0.2 / 0.25));
    CHECK (frac (0.2) < 1.0 - std::exp (-0.2 / 0.15));
    CHECK (frac (0.5) > 0.86);
    CHECK (frac (0.5) < 0.97);
    // no clicks: the largest sample-to-sample step during the glide stays of the order of the signal's own
    CHECK (maxStep < 2.0 * stepBefore + 0.02);

    // settles exactly on the new integer delay
    for (int b = 0; b < 600; ++b)
        runBlock();
    CHECK (e.getCurrentDelaySamples (0) == 9600.0);
}

TEST_CASE ("Mix and Output changes complete within 5 ms", "[core][output]")
{
    const double fs = 48000.0;
    auto s = coreSettings (10.0, 0.0);
    s.mixPct = 0.0;
    auto e = makeEngine (s, fs);
    std::vector<double> l (480, 0.5), r (480, 0.5);
    e.process (l.data(), r.data(), 480);
    s.mixPct = 0.0;
    s.outputDb = -6.0;
    e.setSettings (s);
    std::vector<double> l2 (480, 0.5), r2 (480, 0.5);
    e.process (l2.data(), r2.data(), 480);
    const double target = 0.5 * std::pow (10.0, -6.0 / 20.0);
    CHECK (std::abs (l2[0] - target) > 1e-3);       // a short ramp, not a jump ...
    CHECK_THAT (l2[240], WithinAbs (target, 1e-6)); // ... finished after 5 ms
}

TEST_CASE ("Engine is stable and finite with every option enabled", "[core]")
{
    auto s = coreSettings (3.0, 200.0);
    s.pingPong = true;
    s.dual = true;
    s.delayRMs = 7.0;
    s.analog = 3;
    s.lofi = true;
    s.modDepthPct = 100.0;
    s.modRateHz = 6000.0;
    s.hiPassHz = 200.0;
    s.loPassHz = 3000.0;
    for (double fs : { 44100.0, 96000.0 })
    {
        auto e = makeEngine (s, fs);
        auto x = sine (0.9, 333.0, fs, size_t (fs));
        auto y = run (e, x, x);
        for (size_t n = 0; n < y.l.size(); ++n)
        {
            REQUIRE (std::isfinite (y.l[n]));
            REQUIRE (std::abs (y.l[n]) < 4.0);
        }
    }
}
