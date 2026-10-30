// Gate / expander (spec 6, 10.2, 10.5, Appendix B).
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cstdio>

#include "SpecTables.h"
#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
Settings gateSettings (double openDb)
{
    Settings s;
    s.compBypass = true;
    s.gateOpenDb = openDb;
    s.gateAttackMs = 2.0;
    s.gateReleaseMs = 30.0;
    return s;
}

// Staircase of 0.25 dB steps (each `stepMs` long); returns the level at which the gate state first changed.
double stairSwitchLevel (const Settings& s, double fromDb, double toDb, double fs = 48000.0, double stepMs = 60.0)
{
    auto e = makeEngine (s, fs);
    const double dir = toDb > fromDb ? 0.25 : -0.25;
    const int stepN = int (stepMs * 0.001 * fs);
    // settle at the start level
    {
        auto in = sine (int (0.3 * fs), fs, 1000.0, fromDb);
        auto r = in;
        e.process (in.data(), r.data(), int (in.size()));
    }
    const bool initial = e.getMeters().gateOpen;
    long phase = 0;
    for (double lvl = fromDb; dir > 0 ? lvl <= toDb + 1e-9 : lvl >= toDb - 1e-9; lvl += dir)
    {
        std::vector<double> l (static_cast<size_t> (stepN)), r;
        for (int i = 0; i < stepN; ++i, ++phase)
            l[size_t (i)] = dbToGain (lvl) * std::sin (2.0 * pi * 1000.0 * double (phase) / fs);
        r = l;
        e.process (l.data(), r.data(), stepN);
        if (e.getMeters().gateOpen != initial)
            return lvl;
    }
    return 1.0e9;
}
} // namespace

TEST_CASE ("10.2 gate open/close levels (0.25 dB stair)", "[gate]")
{
    auto s = gateSettings (-40.0);
    const double open = stairSwitchLevel (s, -45.0, -35.0);
    INFO ("opened at " << open);
    CHECK (open >= -39.25 - 0.3);
    CHECK (open <= -39.0 + 0.3);

    s.gateAttackMs = 0.1;
    const double openFast = stairSwitchLevel (s, -45.0, -35.0);
    INFO ("opened (attack 0.1 ms) at " << openFast);
    CHECK (openFast >= -40.0 - 0.3);
    CHECK (openFast <= -39.75 + 0.3);

    s.gateAttackMs = 2.0;
    s.gateCloseDb = -43.0;
    const double close = stairSwitchLevel (s, -36.0, -48.0);
    INFO ("closed at " << close);
    CHECK (close <= -42.25 + 0.3);
    CHECK (close >= -42.5 - 0.3);
}

TEST_CASE ("Floor law: closed gate settles exactly at the floor", "[gate][floor]")
{
    const double fs = 48000.0;
    for (double floorDb : { -27.42, 12.04, -6.0 })
    {
        auto s = gateSettings (-20.0);
        s.floorValue = floorDb;
        CHECK (steadyGainDb (s, fs, -50.0, 1000.0, 1.0) == Approx (floorDb).margin (0.05));
    }
    // open gate: unity regardless of the floor
    auto s = gateSettings (-40.0);
    s.floorValue = 12.0;
    CHECK (steadyGainDb (s, fs, -10.0) == Approx (0.0).margin (1.0e-9));
    // -inf floor: fully closed
    s = gateSettings (-20.0);
    CHECK (steadyGainDb (s, fs, -50.0, 1000.0, 1.5) < -100.0);
    // negative-polarity floors act as -inf in gate mode
    s.floorValue = -200.0; // "-10 N"
    CHECK (steadyGainDb (s, fs, -50.0, 1000.0, 1.5) < -100.0);
    // law G = F + (1 - F) g
    CHECK (curves::applyFloor (0.5, 0.25) == Approx (0.25 + 0.75 * 0.5));
    CHECK (curves::applyFloor (1.0, 3.98) == 1.0);
}

TEST_CASE ("Appendix B: expander static gain", "[expander]")
{
    const double fs = 48000.0;
    const double open = -40.0;
    // x_det = sine peak - GateOpen + 1.13 dB at Gate Attack 2 / Release 30 ms
    int checked = 0;
    for (int i = 0; i < spectables::expRows; ++i)
    {
        const double x = spectables::expX[i];
        const double peak = x - 1.13 + open;
        if (peak > 0.0 || peak < -100.0)
            continue;
        auto s = gateSettings (open);
        s.expander = true;
        INFO ("x " << x);
        CHECK (steadyGainDb (s, fs, peak, 1000.0, 0.3) == Approx (spectables::expH[i]).margin (0.1));
        ++checked;
    }
    CHECK (checked > 40);
}

TEST_CASE ("Expander floors: positive floor boost and negative-polarity notch", "[expander][floor]")
{
    const double fs = 48000.0;
    auto s = gateSettings (-20.0);
    s.expander = true;
    s.floorValue = 12.0;
    // far below the threshold g -> 0, the module gain tends to the floor (+12 dB low-level boost)
    CHECK (steadyGainDb (s, fs, -90.0) == Approx (12.0).margin (0.1));
    // negative polarity "-20 N": a cancellation notch where g = |F| / (1 + |F|)
    s.floorValue = -190.0; // "-20 N": v = y - 210
    REQUIRE (floorvalue::linear (s.floorValue, true) == Approx (-0.1));
    double minGain = 0.0, minLevel = 0.0;
    for (double lvl = -60.0; lvl <= -20.0; lvl += 0.5)
    {
        const double g = steadyGainDb (s, fs, lvl, 1000.0, 0.2);
        if (g < minGain)
            minGain = g, minLevel = lvl;
    }
    INFO ("notch " << minGain << " dB at " << minLevel);
    CHECK (minGain < -30.0);
    // expected where h = 20 log10 (0.1 / 1.1) = -20.8 dB -> x ~ -18.7, peak ~ -20 - 18.7 - 1.13
    CHECK (minLevel == Approx (-39.8).margin (1.5));
}

namespace
{
struct GateRun
{
    std::vector<double> g; // per-1-ms linear gain
};

// Gate (open -30): sine -40 -> -10 for `burstMs` -> back to -40; frames from the burst start.
GateRun gateBurst (double att, double rel, double holdMs, double burstMs, double fs = 48000.0)
{
    auto s = gateSettings (-30.0);
    s.gateAttackMs = att;
    s.gateReleaseMs = rel;
    s.holdMs = holdMs;
    const int pre = int (0.5 * fs), burst = int (burstMs * 0.001 * fs), post = int (1.0 * fs);
    auto in = sine (pre + burst + post, fs, 1000.0, [&] (int i) { return (i >= pre && i < pre + burst) ? dbToGain (-10.0) : dbToGain (-40.0); });
    auto e = makeEngine (s, fs);
    auto out = runStereo (e, in, in);
    return { frameGainLin (out.l, in, fs, pre, int (burstMs) + 900) };
}

double firstAbove (const std::vector<double>& g, double v, int from = 0)
{
    for (size_t k = size_t (from); k < g.size(); ++k)
        if (g[k] >= v)
            return double (k) + 0.5;
    return 1.0e9;
}
double firstBelow (const std::vector<double>& g, double v, int from)
{
    for (size_t k = size_t (from); k < g.size(); ++k)
        if (g[k] <= v)
            return double (k) + 0.5 - from;
    return 1.0e9;
}

// RMS distance between the measured close envelope and the one-pole through the reference t10/t63/t90
double closeRmsError (const std::vector<double>& g, int from, double t10, double t63, double t90)
{
    const double tau = (t90 - t10) / std::log (9.0); // 10 % -> 90 %
    const double t0 = t10 - tau * std::log (1.0 / 0.9);
    (void) t63;
    double s = 0.0;
    int n = 0;
    for (int k = 0; k < int (t90 * 1.5) && from + k < int (g.size()); ++k)
    {
        const double t = k + 0.5;
        const double ref = t < t0 ? 1.0 : std::exp (-(t - t0) / tau);
        s += (g[size_t (from + k)] - ref) * (g[size_t (from + k)] - ref);
        ++n;
    }
    return std::sqrt (s / n);
}
} // namespace

TEST_CASE ("10.5 gate envelopes (linear gain)", "[gate][envelope]")
{
    const double burst = 500.0;
    {
        const auto r = gateBurst (1.0, 100.0, 0.01, burst);
        const double open63 = firstAbove (r.g, 0.63);
        const double c10 = firstBelow (r.g, 0.9, int (burst)), c63 = firstBelow (r.g, 0.37, int (burst)),
                     c90 = firstBelow (r.g, 0.1, int (burst));
        const double err = closeRmsError (r.g, int (burst), 93.5, 182.5, 313.5);
        std::printf ("gate att 1 rel 100: open t63 %.1f (ref 2.5), close %.1f/%.1f/%.1f (ref 93.5/182.5/313.5) rms %.3f\n", open63, c10,
                     c63, c90, err);
        CHECK (open63 == Approx (2.5).margin (1.5));
        CHECK (c63 == Approx (182.5).margin (18.0));
        CHECK (err <= 0.06);
    }
    {
        const auto r = gateBurst (1.0, 10.0, 0.01, burst);
        const double c10 = firstBelow (r.g, 0.9, int (burst)), c63 = firstBelow (r.g, 0.37, int (burst)),
                     c90 = firstBelow (r.g, 0.1, int (burst));
        const double err = closeRmsError (r.g, int (burst), 9.5, 18.5, 31.5);
        std::printf ("gate rel 10: close %.1f/%.1f/%.1f (ref 9.5/18.5/31.5) rms %.3f\n", c10, c63, c90, err);
        CHECK (c63 == Approx (18.5).margin (2.0));
        CHECK (err <= 0.06);
    }
    {
        const auto r = gateBurst (10.0, 100.0, 0.01, burst);
        const double open63 = firstAbove (r.g, 0.63);
        std::printf ("gate att 10: open t63 %.1f (ref 11.5)\n", open63);
        CHECK (open63 == Approx (11.5).margin (1.5));
    }
    {
        // Hold 97.6 ms after a 30 ms burst: close t10 162.5 ms after the burst ends
        const auto r = gateBurst (1.0, 100.0, 97.6, 30.0);
        const double c10 = firstBelow (r.g, 0.9, 30);
        std::printf ("gate hold 97.6: close t10 %.1f (ref 162.5)\n", c10);
        CHECK (c10 == Approx (162.5).margin (16.0));
        // without hold the gate closes much earlier
        const auto r0 = gateBurst (1.0, 100.0, 0.01, 30.0);
        CHECK (firstBelow (r0.g, 0.9, 30) < c10 - 50.0);
    }
}
