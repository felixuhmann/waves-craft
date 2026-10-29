// Spec section 11.1: deterministic paths (direct, voice delay/pan/routing, feedback), any sample rate.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
// -135 dB re peak (signals have peak <= ~1.5)
constexpr double tol = 3.0e-7;

std::vector<double> delayed (const std::vector<double>& x, int d)
{
    std::vector<double> y (x.size(), 0.0);
    for (size_t i = size_t (d); i < x.size(); ++i)
        y[i] = x[i - size_t (d)];
    return y;
}

std::vector<double> lin (double a, const std::vector<double>& x, double b, const std::vector<double>& y)
{
    std::vector<double> o (x.size());
    for (size_t i = 0; i < x.size(); ++i)
        o[i] = a * x[i] + b * y[i];
    return o;
}

int lr (double fs, bool range20) { return int (std::lround ((range20 ? 1088.0 : 320.0) / 44100.0 * fs)); }
} // namespace

TEST_CASE ("Direct path: identity, rotation, gain", "[paths]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const int n = int (0.2 * fs);
    const auto L = noise (n, 11), R = noise (n, 12);

    SECTION ("pan 0 is the identity")
    {
        auto e = makeEngine (directSettings(), fs);
        const auto o = run (e, L, R);
        CHECK (maxAbsDiff (o.l, L) <= tol);
        CHECK (maxAbsDiff (o.r, R) <= tol);
    }
    SECTION ("pan -22.5 deg rotates the image")
    {
        auto s = directSettings();
        s.directPanDeg = -22.5;
        auto e = makeEngine (s, fs);
        const auto o = run (e, L, R);
        CHECK (maxAbsDiff (o.l, lin (0.9238795325, L, 0.3826834324, R)) <= tol);
        CHECK (maxAbsDiff (o.r, lin (-0.3826834324, L, 0.9238795325, R)) <= tol);
    }
    SECTION ("pan -45 deg folds the image left")
    {
        auto s = directSettings();
        s.directPanDeg = -45.0;
        auto e = makeEngine (s, fs);
        const auto o = run (e, L, R);
        CHECK (maxAbsDiff (o.l, lin (0.7071067812, L, 0.7071067812, R)) <= tol);
        CHECK (maxAbsDiff (o.r, lin (-0.7071067812, L, 0.7071067812, R)) <= tol);
    }
    SECTION ("direct gain -12 dB and master -12 dB")
    {
        for (int which = 0; which < 2; ++which)
        {
            auto s = directSettings();
            (which == 0 ? s.directGainDb : s.masterDb) = -12.0;
            auto e = makeEngine (s, fs);
            const auto o = run (e, L, R);
            CHECK (maxAbsDiff (o.l, lin (0.2511886432, L, 0.0, R)) <= tol);
            CHECK (maxAbsDiff (o.r, lin (0.0, L, 0.2511886432, R)) <= tol);
        }
    }
    SECTION ("align delays the direct by the inherent latency")
    {
        for (bool r20 : { false, true })
        {
            auto s = directSettings();
            s.alignDirect = true;
            s.range20 = r20;
            auto e = makeEngine (s, fs);
            const auto o = run (e, L, R);
            CHECK (maxAbsDiff (o.l, delayed (L, lr (fs, r20))) <= tol);
            CHECK (maxAbsDiff (o.r, delayed (R, lr (fs, r20))) <= tol);
        }
        CHECK (lr (44100.0, false) == 320);
        CHECK (lr (44100.0, true) == 1088);
    }
}

TEST_CASE ("Voice path: delay, constant-power pan", "[paths]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const int n = int (0.25 * fs);
    const auto L = noise (n, 21), R = noise (n, 22);
    const auto zero = std::vector<double> (size_t (n), 0.0);

    SECTION ("pan 0, delay 20 ms")
    {
        auto e = makeEngine (soloSettings(), fs);
        const auto o = run (e, L, R);
        const auto expect = lin (0.7071067812, delayed (L, int (0.02 * fs)), 0.0, zero);
        CHECK (maxAbsDiff (o.l, expect) <= tol);
        CHECK (maxAbsDiff (o.r, expect) <= tol);
    }
    SECTION ("pans -45 / +45 / -22.5")
    {
        const double pans[3] = { -45.0, 45.0, -22.5 };
        const double gl[3] = { 1.0, 0.0, 0.9238795325 }, gr[3] = { 0.0, 1.0, 0.3826834324 };
        for (int k = 0; k < 3; ++k)
        {
            auto s = soloSettings();
            s.voices[0].panDeg = pans[k];
            auto e = makeEngine (s, fs);
            const auto o = run (e, L, R);
            const auto d = delayed (L, int (0.02 * fs));
            CHECK (maxAbsDiff (o.l, lin (gl[k], d, 0.0, zero)) <= tol);
            CHECK (maxAbsDiff (o.r, lin (gr[k], d, 0.0, zero)) <= tol);
        }
    }
}

TEST_CASE ("Voice delay clamping and alignment (spec section 5)", "[paths]")
{
    const double fs = GENERATE (44100.0, 48000.0);
    const int n = int (0.2 * fs);
    const auto L = noise (n, 31), R = noise (n, 32);

    struct Case
    {
        double delayMs;
        bool range20, align;
        double expectMs;
    };
    const double lr80 = 320.0 / 44100.0 * 1000.0, lr20 = 1088.0 / 44100.0 * 1000.0;
    const Case cases[] = {
        { 0.0, false, false, lr80 },  { 5.0, false, false, lr80 },  { 10.0, false, false, 10.0 },
        { 50.0, false, false, 50.0 }, { 0.0, true, false, lr20 },   { 5.0, true, false, lr20 },
        { 10.0, true, false, lr20 },  { 50.0, true, false, 50.0 },  { 0.0, false, true, lr80 },
        { 50.0, false, true, 50.0 + lr80 },
    };
    for (const auto& c : cases)
    {
        auto s = soloSettings();
        s.voices[0].panDeg = -45.0;
        s.voices[0].delayMs = c.delayMs;
        s.range20 = c.range20;
        s.alignDirect = c.align;
        auto e = makeEngine (s, fs);
        const auto o = run (e, L, R);
        // expected delay in samples: Lr is rounded to whole samples, delays are exact multiples here
        const double lrS = double (lr (fs, c.range20));
        const double d = c.align ? c.delayMs * fs / 1000.0 + lrS
                                 : std::max (c.delayMs * fs / 1000.0, lrS);
        INFO ("delay " << c.delayMs << " ms, range20 " << c.range20 << ", align " << c.align);
        CHECK (d / fs * 1000.0 == Approx (c.expectMs).margin (0.5 / fs * 1000.0));
        CHECK (e.getVoice (0).getNominalDelay() == Approx (d));
        if (d == std::floor (d))
            CHECK (maxAbsDiff (o.l, delayed (L, int (d))) <= tol);
        else
        {
            // fractional delay: linear interpolation between neighbouring samples
            const double mu = d - std::floor (d);
            const auto a = delayed (L, int (std::floor (d))), b = delayed (L, int (std::floor (d)) + 1);
            CHECK (maxAbsDiff (o.l, lin (1.0 - mu, a, mu, b)) <= tol);
        }
        CHECK (peakAbs (o.r) <= tol);
    }
}

TEST_CASE ("Input routing", "[paths]")
{
    const double fs = 48000.0;
    const int n = int (0.1 * fs);
    const auto L = noise (n, 41), R = noise (n, 42);
    const int d = int (0.02 * fs);

    auto voiceOut = [&] (int voice, Routing a, Routing b) {
        auto s = soloSettings();
        s.voices[0].on = false;
        auto& v = s.voices[size_t (voice)];
        v = soloSettings().voices[0];
        v.panDeg = -45.0;
        s.routingA = a;
        s.routingB = b;
        auto e = makeEngine (s, fs);
        return run (e, L, R).l;
    };
    CHECK (maxAbsDiff (voiceOut (0, Routing::Direct, Routing::Direct), delayed (L, d)) <= tol);
    CHECK (maxAbsDiff (voiceOut (0, Routing::Sum, Routing::Direct), delayed (lin (0.5, L, 0.5, R), d)) <= tol);
    CHECK (maxAbsDiff (voiceOut (0, Routing::Difference, Routing::Direct), delayed (lin (0.5, L, -0.5, R), d)) <= tol);
    CHECK (maxAbsDiff (voiceOut (1, Routing::Direct, Routing::Direct), delayed (R, d)) <= tol);
    CHECK (maxAbsDiff (voiceOut (3, Routing::Direct, Routing::Direct), delayed (R, d)) <= tol);
    CHECK (maxAbsDiff (voiceOut (2, Routing::Direct, Routing::Direct), delayed (L, d)) <= tol);
    CHECK (maxAbsDiff (voiceOut (1, Routing::Direct, Routing::Sum), delayed (lin (0.5, L, 0.5, R), d)) <= tol);
    CHECK (maxAbsDiff (voiceOut (1, Routing::Direct, Routing::Difference), delayed (lin (-0.5, L, 0.5, R), d)) <= tol);
    CHECK (maxAbsDiff (voiceOut (2, Routing::Difference, Routing::Direct), delayed (lin (0.5, L, -0.5, R), d)) <= tol);
}

TEST_CASE ("Feedback echoes (spec section 9)", "[paths]")
{
    const double fs = 44100.0;
    const int n = int (0.2 * fs);
    std::vector<double> L (size_t (n), 0.0), R (size_t (n), 0.0);
    L[0] = 1.0;

    SECTION ("60 %, HP 16 Hz: echoes at 20 ms + k (20 ms + 1 sample)")
    {
        auto s = soloSettings();
        s.voices[0].panDeg = -45.0;
        s.voices[0].feedbackPct = 60.0;
        s.fbHighPassHz = 16.0;
        auto e = makeEngine (s, fs);
        const auto o = run (e, L, R);
        const double expected[4] = { 1.0, 0.5994, 0.3591, 0.2153 };
        for (int k = 0; k < 4; ++k)
        {
            const size_t at = size_t (882 + k * 883);
            INFO ("echo " << k);
            CHECK (o.l[at] == Approx (expected[k]).margin (5e-4));
            // the echo is the peak of its neighbourhood
            for (size_t j = at - 100; j < at + 100; ++j)
                CHECK (std::abs (o.l[j]) <= o.l[at]);
        }
    }

    SECTION ("100 % feedback stays bounded")
    {
        auto s = soloSettings();
        s.voices[0].feedbackPct = 100.0;
        s.voices[0].detuneCents = 25.0;
        auto e = makeEngine (s, fs);
        auto big = noise (int (3 * fs), 5);
        const auto o = run (e, big, big);
        CHECK (peakAbs (o.l) < 20.0);
        CHECK (std::isfinite (o.l.back()));
    }
}

TEST_CASE ("Feedback high-pass: 2nd / 1st echo = 0.6 H_HP(f)", "[paths]")
{
    const double fs = GENERATE (44100.0, 48000.0);
    const int d = int (0.02 * fs);
    const int n = 4 * d;
    std::vector<double> L (size_t (n), 0.0), R (size_t (n), 0.0);
    L[0] = 1.0;
    auto s = soloSettings();
    s.voices[0].panDeg = -45.0;
    s.voices[0].feedbackPct = 60.0;
    s.fbHighPassHz = 578.0;
    auto e = makeEngine (s, fs);
    const auto o = run (e, L, R);

    // echo windows: [d, 2d+1) and [2d+1, 3d+2)
    const std::vector<double> first (o.l.begin() + d, o.l.begin() + 2 * d + 1);
    const std::vector<double> second (o.l.begin() + 2 * d + 1, o.l.begin() + 3 * d + 2);
    for (double f : { 50.0, 200.0, 578.0, 1000.0, 3000.0, 10000.0, 18000.0 })
    {
        cd a = 0.0, b = 0.0;
        for (size_t i = 0; i < first.size(); ++i)
        {
            const auto z = std::polar (1.0, -2.0 * pi * f * double (i) / fs);
            a += first[i] * z;
            b += second[i] * z;
        }
        const cd expected = 0.6 * FeedbackHighPass::response (578.0, f, fs);
        INFO ("f = " << f);
        CHECK (20.0 * std::log10 (std::abs (b / a) / std::abs (expected)) == Approx (0.0).margin (0.05));
    }
}

TEST_CASE ("Fractional voice delays: interpolation weights exact (double-precision delay arithmetic)", "[paths]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);

    SECTION ("10.2 samples beyond Lr: impulse taps 0.8 / 0.2 to 1e-7, noise path <= -135 dB")
    {
        auto s = soloSettings();
        s.voices[0].panDeg = -45.0;
        s.alignDirect = true; // Dn = Delay + Lr, so a short fractional delay is not clamped
        s.voices[0].delayMs = 10.2 / fs * 1000.0;
        const int d = lr (fs, false) + 10;
        const int n = int (0.1 * fs);

        auto e = makeEngine (s, fs);
        std::vector<double> L (size_t (n), 0.0), R (size_t (n), 0.0);
        L[0] = 1.0;
        const auto o = run (e, L, R);
        CHECK (std::abs (o.l[size_t (d)] - 0.8) <= 1e-7);
        CHECK (std::abs (o.l[size_t (d + 1)] - 0.2) <= 1e-7);

        auto e2 = makeEngine (s, fs);
        const auto x = noise (n, 51);
        const auto y = run (e2, x, x).l;
        CHECK (maxAbsDiff (y, lin (0.8, delayed (x, d), 0.2, delayed (x, d + 1))) <= tol);
    }

    SECTION ("host-normalised values pass through float without leaving the parameter grid")
    {
        // The host stores r in single precision; the plug-in maps it with the double-precision spec mapping
        // (PluginProcessor::readSettings), which must land exactly on the step value.
        namespace pm = craft::doubler::param;
        for (int k = 0; k <= 1000; ++k)
        {
            const double v = k / 10.0;
            const float r = float (pm::DelayMs::toNorm (v));
            REQUIRE (pm::DelayMs::fromNorm (double (r)) == quantise (v, 10.0));
        }
        auto s = soloSettings();
        s.voices[0].panDeg = -45.0;
        s.voices[0].delayMs = pm::DelayMs::fromNorm (double (float (pm::DelayMs::toNorm (9.4))));
        auto e = makeEngine (s, fs);
        const double dn = 9.4 * fs / 1000.0, mu = dn - std::floor (dn);
        std::vector<double> L (size_t (fs * 0.05), 0.0), R (L.size(), 0.0);
        L[0] = 1.0;
        const auto o = run (e, L, R);
        CHECK (std::abs (o.l[size_t (std::floor (dn))] - (1.0 - mu)) <= 1e-7);
        CHECK (std::abs (o.l[size_t (std::floor (dn)) + 1] - mu) <= 1e-7);
    }
}
