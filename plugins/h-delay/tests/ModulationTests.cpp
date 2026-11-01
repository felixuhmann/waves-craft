// Spec r1 section 8: delay-time modulation (LFO rate, depth law, smoothing tables).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>

#include "TestHelpers.h"

using namespace testutil;

namespace
{
// Peak amplitude (half peak-to-peak, last full LFO cycle) of the smoothed delay offset relative to
// the nominal amplitude A = Depth/100 * D / 2, using the engine's LFO and smoother.
double amplitudeRatio (double rateHz, double delayMs, double depthPct, double fs = 48000.0)
{
    TriangleLfo lfo;
    lfo.prepare (fs);
    lfo.setRate (rateHz);
    ModulationSmoother sm;
    sm.prepare (fs);
    const double D = delayMs * fs / 1000.0;
    const double A = depthPct / 100.0 * D / 2.0;
    const double seconds = std::max (4.0, 3.0 / rateHz) + 12.0;
    const auto total = size_t (seconds * fs);
    const auto start = total - size_t (fs / rateHz) - 2;
    double mx = -1e30, mn = 1e30;
    for (size_t n = 0; n < total; ++n)
    {
        const double y = sm.process (A * lfo.next(), D);
        if (n >= start)
        {
            mx = std::max (mx, y);
            mn = std::min (mn, y);
        }
    }
    return (mx - mn) / 2.0 / A;
}
} // namespace

TEST_CASE ("Triangle LFO shape and rate", "[mod]")
{
    CHECK (triangle (0.25) == 0.0);
    CHECK (triangle (0.5) == 1.0);
    CHECK (triangle (0.0) == -1.0);
    CHECK (triangle (0.75) == 0.0);

    for (double rate : { 0.1, 0.5, 1.0, 2.0, 7.3 })
    {
        const double fs = 48000.0;
        TriangleLfo lfo;
        lfo.prepare (fs);
        lfo.setRate (rate);
        // count upward zero crossings over N periods
        const int periods = 3;
        const auto total = size_t (periods / rate * fs) + 10;
        double prev = lfo.next();
        std::vector<double> crossings;
        for (size_t n = 1; n < total; ++n)
        {
            const double v = lfo.next();
            if (prev < 0.0 && v >= 0.0)
                crossings.push_back (double (n) - v / (v - prev));
            prev = v;
        }
        REQUIRE (crossings.size() >= 2);
        const double period = (crossings.back() - crossings.front()) / double (crossings.size() - 1) / fs;
        CHECK (std::abs (period * rate - 1.0) < 0.002);
    }
}

TEST_CASE ("Modulation smoothing meets the r2 section 8 table within 15 %", "[mod]")
{
    struct Point
    {
        double delayMs, depth, rate, ratio;
    };
    const Point pts[] = {
        { 10, 10, 0.5, 1.02 }, { 10, 10, 2, 1.02 },  { 10, 10, 5, 0.90 },     { 10, 10, 10, 0.72 },
        { 10, 10, 20, 0.50 },  { 10, 10, 50, 0.03 }, { 50, 50, 0.5, 0.99 },   { 100, 10, 0.5, 0.95 },
        { 200, 10, 0.5, 0.73 }, { 500, 10, 0.5, 0.43 }, { 1000, 10, 0.5, 0.25 }, { 3500, 10, 0.5, 0.07 },
    };
    for (const auto& p : pts)
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            const double r = amplitudeRatio (p.rate, p.delayMs, p.depth, fs);
            INFO ("D " << p.delayMs << " ms, depth " << p.depth << " %, rate " << p.rate << " Hz, fs " << fs << ": ratio " << r);
            CHECK (std::abs (r / p.ratio - 1.0) <= 0.15);
        }
    // the verifier's large-excursion cases in ms: D 50 ms / Depth 50 % -> +-12.4 ms; D 1024 ms / 10 % -> +-12.8 ms
    CHECK (std::abs (amplitudeRatio (0.5, 50.0, 50.0) * 12.5 / 12.4 - 1.0) <= 0.15);
    CHECK (std::abs (amplitudeRatio (0.5, 1024.0, 10.0) * 51.2 / 12.8 - 1.0) <= 0.15);
}

TEST_CASE ("Modulation moves the echo; Depth 0 leaves it exact", "[mod]")
{
    const double fs = 48000.0;
    auto s = coreSettings (20.0, 0.0);
    s.modDepthPct = 50.0;
    s.modRateHz = 1.0;
    auto e = makeEngine (s, fs);
    // feed a click train and find where the echoes land
    std::vector<double> l (size_t (fs), 0.0), r;
    for (size_t n = 0; n < l.size(); n += 2400)
        l[n] = 0.1;
    r = l;
    auto y = run (e, l, r);
    // at t = 0.25 s the LFO (starting at its centre) is at its maximum: delay ~ D * (1 + 0.25) = 25 ms
    // (less the smoothing); the echo of the click at 0.25 s must land well after 20 ms
    size_t argmax = 12000;
    for (size_t n = 12000; n < 12000 + 1600; ++n)
        if (std::abs (y.l[n]) > std::abs (y.l[argmax]))
            argmax = n;
    const double lagMs = double (argmax - 12000) / fs * 1000.0;
    CHECK (lagMs > 22.0);
    CHECK (lagMs < 26.5);

    s.modDepthPct = 0.0;
    auto e0 = makeEngine (s, fs);
    auto y0 = impulse (e0, 2000, 0.1);
    CHECK_THAT (y0.l[960], Catch::Matchers::WithinAbs (0.1 / 1.1, 1e-15));
    CHECK (y0.l[959] == 0.0);
    CHECK (y0.l[961] == 0.0);
}

TEST_CASE ("Fractional reads interpolate band-limited", "[mod][interp]")
{
    DelayLine d;
    d.prepare (4096);
    const double fs = 48000.0, f = 1000.0;
    for (int n = 0; n < 4096; ++n)
        d.push (std::sin (2.0 * pi * f * n / fs));
    // reading 100.37 samples back reproduces the sine at that fractional position
    for (double delay : { 100.0, 100.25, 100.37, 100.5, 333.9 })
    {
        const double want = std::sin (2.0 * pi * f * (4096.0 - delay) / fs);
        CHECK (std::abs (d.read (delay) - want) < 1e-4);
    }
}
