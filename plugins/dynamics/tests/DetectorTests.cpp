// Detector (spec 4) and dynamic envelopes (spec 10.5).
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cstdio>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
Settings limiterSettings (double thresholdDb = -40.0)
{
    Settings s;
    s.ratioPos = 0.8; // 50:1
    s.compThresholdDb = thresholdDb;
    return s;
}

double steadyGain (const Settings& s, std::vector<double> l, std::vector<double> r, double fs)
{
    auto e = makeEngine (s, fs);
    const auto inL = l, inR = r;
    auto out = runStereo (e, std::move (l), std::move (r));
    const int n = int (inL.size());
    const double num = rms (out.l, n / 2, n) + rms (out.r, n / 2, n);
    const double den = rms (inL, n / 2, n) + rms (inR, n / 2, n);
    return 20.0 * std::log10 (num / den);
}

// Per-1-ms gain change envelope (fraction of the total change), mirroring how 10.5 was measured.
struct Timing
{
    double t10, t63, t90;
};

// Reference envelope through (0,0), (t10,.1), (t63,.63), (t90,.9) and an exponential tail (monotone, piecewise
// linear in time) - used to evaluate the spec 9 per-1-ms tolerance against the 10.5 points.
double referenceFraction (const Timing& t, double ms)
{
    if (ms <= 0.0) return 0.0;
    if (ms <= t.t10) return 0.1 * ms / t.t10;
    if (ms <= t.t63) return 0.1 + 0.53 * (ms - t.t10) / (t.t63 - t.t10);
    if (ms <= t.t90) return 0.63 + 0.27 * (ms - t.t63) / (t.t90 - t.t63);
    const double tau = (t.t90 - t.t63) / std::log (0.37 / 0.1);
    return 1.0 - 0.1 * std::exp (-(ms - t.t90) / tau);
}

struct EnvelopeError
{
    double rms, p99;
};

EnvelopeError envelopeError (const std::vector<double>& g, double from, double to, const Timing& ref, int frames)
{
    std::vector<double> err;
    for (int k = 0; k < frames && k < int (g.size()); ++k)
    {
        const double expected = from + referenceFraction (ref, k + 0.5) * (to - from);
        err.push_back (std::abs (g[size_t (k)] - expected));
    }
    double s = 0.0;
    for (double v : err)
        s += v * v;
    std::sort (err.begin(), err.end());
    return { std::sqrt (s / double (err.size())), err[size_t (0.99 * double (err.size() - 1))] };
}
} // namespace

TEST_CASE ("Detector reading of a steady sine: x_det = peak + 1.277 dB at 2/50 ms", "[detector]")
{
    // seen through the 50:1 curve: the 10.1 values sit exactly on Appendix A rows
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        auto e = makeEngine (limiterSettings(), fs);
        auto in = sine (int (0.3 * fs), fs, 1000.0, -20.0);
        auto out = runStereo (e, in, in);
        CHECK (e.getMeters().compLevelDb == Approx (-20.0 + 1.277).margin (0.03));
    }
}

TEST_CASE ("Detector: waveform dependence (spec 4 consequences)", "[detector]")
{
    const double fs = 48000.0;
    const int n = int (0.6 * fs);
    const auto s = limiterSettings();
    const double sineRmsDb = -23.0; // 1 kHz sine of -20 dB peak
    auto sineOf = [&] (double f) { return sine (n, fs, f, sineRmsDb + 3.0103); };
    const double g1k = steadyGain (s, sineOf (1000.0), sineOf (1000.0), fs);
    const double g100 = steadyGain (s, sineOf (100.0), sineOf (100.0), fs);
    const double g10k = steadyGain (s, sineOf (10000.0), sineOf (10000.0), fs);
    auto nz = noise (n, 7u, dbToGain (sineRmsDb));
    const double gNoise = steadyGain (s, nz, nz, fs);
    INFO ("1k " << g1k << " 100 " << g100 << " 10k " << g10k << " noise " << gNoise);
    // The spec quotes ~0.7 dB less reduction for equal-RMS white noise. The spec 4 detector model itself gives
    // about -0.4 dB (Gaussian) / +0.2 dB (uniform) here, so only the bounded difference is checked
    // (IMPLEMENTATION_NOTES.md, questions/assumptions).
    CHECK (gNoise - g1k > -0.6);
    CHECK (gNoise - g1k < 1.0);
    CHECK (g100 - g1k == Approx (-1.0).margin (0.4));    // ~1 dB more
    CHECK (g10k - g1k == Approx (1.0).margin (0.4));     // ~1 dB less
}

TEST_CASE ("Stereo linking: max of the channel powers", "[detector][key]")
{
    const double fs = 48000.0;
    const int n = int (0.4 * fs);
    const auto s = limiterSettings();
    auto loud = sine (n, fs, 1000.0, -10.0);
    auto quiet = sine (n, fs, 1000.0, -20.0);
    std::vector<double> silent (size_t (n), 0.0);

    auto gainOf = [&] (std::vector<double> l, std::vector<double> r, bool useLeft) {
        auto e = makeEngine (s, fs);
        const auto ref = useLeft ? l : r;
        auto out = runStereo (e, std::move (l), std::move (r));
        return ratioDb (useLeft ? out.l : out.r, ref, n / 2, n);
    };
    const double both = gainOf (loud, loud, true);
    CHECK (gainOf (loud, silent, true) == Approx (both).margin (1.0e-6));
    CHECK (gainOf (loud, quiet, true) == Approx (both).margin (1.0e-6));
    CHECK (gainOf (quiet, loud, false) == Approx (both).margin (1.0e-6));
    CHECK (gainOf (loud, quiet, false) == Approx (both).margin (1.0e-6)); // the quiet channel gets the same gain
}

TEST_CASE ("10.5 compressor envelopes (50:1, threshold -30, -40 -> -10 -> -40 dBFS)", "[detector][envelope]")
{
    const double fs = 48000.0;
    struct Case
    {
        double att, rel, pdr, burstMs;
        Timing attack, release;
        bool hasAttack;
    };
    const Case cases[] = {
        { 1, 50, 0, 600, { 1.5, 3.5, 6.5 }, { 2.5, 25.5, 74.5 }, true },
        { 10, 50, 0, 600, { 3.5, 16.5, 32.5 }, { 4.5, 28.5, 80.5 }, true },
        { 100, 50, 0, 1500, { 23.5, 164.5, 312.5 }, { 8.5, 33.5, 82.5 }, true },
        { 1, 10, 0, 600, { 1.5, 2.5, 5.5 }, { 1.5, 6.5, 16.5 }, true },
        { 1, 200, 0, 600, { 1.5, 3.5, 6.5 }, { 7.5, 95.5, 290.5 }, true },
        { 1, 1000, 0, 600, { 1.5, 3.5, 6.5 }, { 30.5, 417.5, 1144.5 }, true },
        { 1, 1000, 22, 3, {}, { 1.5, 15.5, 769.5 }, false },
        { 1, 1000, 102, 30, {}, { 1.5, 11.5, 521.5 }, false },
    };
    double worstRms = 0.0, worstP99 = 0.0;
    for (const auto& c : cases)
    {
        auto s = limiterSettings (-30.0);
        s.compAttackMs = c.att;
        s.compReleaseMs = c.rel;
        s.pdrMs = c.pdr;
        const int pre = int (0.5 * fs), burst = int (c.burstMs * 0.001 * fs), post = int (2.5 * fs);
        const double lo = dbToGain (-40.0), hi = dbToGain (-10.0);
        auto in = sine (pre + burst + post, fs, 1000.0, [&] (int i) { return (i >= pre && i < pre + burst) ? hi : lo; });
        auto e = makeEngine (s, fs);
        auto out = runStereo (e, in, in);

        const double gLo = ratioDb (out.l, in, pre - int (0.1 * fs), pre);
        const double gHi = curves::compressorGainDb (0.8, -10.0 + 1.277, -30.0);
        if (c.hasAttack)
        {
            const auto g = frameGainDb (out.l, in, fs, pre, int (c.burstMs) - 1);
            const auto err = envelopeError (g, gLo, gHi, c.attack, int (std::min (c.burstMs - 1, 2.0 * c.attack.t90 + 20)));
            std::printf ("att %g rel %g: attack t10/63/90 %.1f/%.1f/%.1f (ref %.1f/%.1f/%.1f) err rms %.2f p99 %.2f\n", c.att, c.rel,
                         crossingMs (g, gLo, gHi, 0.1), crossingMs (g, gLo, gHi, 0.63), crossingMs (g, gLo, gHi, 0.9), c.attack.t10,
                         c.attack.t63, c.attack.t90, err.rms, err.p99);
            worstRms = std::max (worstRms, err.rms);
            worstP99 = std::max (worstP99, err.p99);
        }
        const auto g = frameGainDb (out.l, in, fs, pre + burst, 2400);
        const double gPeak = ratioDb (out.l, in, pre + burst - int (0.001 * fs), pre + burst);
        const auto err = envelopeError (g, gPeak, gLo, c.release, int (std::min (2400.0, 2.0 * c.release.t90 + 20)));
        std::printf ("att %g rel %g pdr %g: release t10/63/90 %.1f/%.1f/%.1f (ref %.1f/%.1f/%.1f) err rms %.2f p99 %.2f\n", c.att,
                     c.rel, c.pdr, crossingMs (g, gPeak, gLo, 0.1), crossingMs (g, gPeak, gLo, 0.63), crossingMs (g, gPeak, gLo, 0.9),
                     c.release.t10, c.release.t63, c.release.t90, err.rms, err.p99);
        worstRms = std::max (worstRms, err.rms);
        worstP99 = std::max (worstP99, err.p99);
        // documented limits of the spec 4 dual-detector model (IMPLEMENTATION_NOTES.md, "Envelope conformance")
        CHECK (err.rms <= 1.6);
        CHECK (err.p99 <= 4.5);
    }
    std::printf ("envelopes: worst rms %.2f dB, worst p99 %.2f dB\n", worstRms, worstP99);
}

TEST_CASE ("PDR: short bursts release fast, then slowly", "[detector][pdr]")
{
    const double fs = 48000.0;
    auto s = limiterSettings (-30.0);
    s.compAttackMs = 1.0;
    s.compReleaseMs = 1000.0;
    s.pdrMs = 22.0;
    const int pre = int (0.3 * fs), burst = int (0.003 * fs), post = int (1.0 * fs);
    auto in = sine (pre + burst + post, fs, 1000.0, [&] (int i) { return (i >= pre && i < pre + burst) ? dbToGain (-10.0) : dbToGain (-40.0); });
    auto e = makeEngine (s, fs);
    auto out = runStereo (e, in, in);
    const auto g = frameGainDb (out.l, in, fs, pre + burst, 900);
    // fast part: most of the reduction is gone within ~20 ms
    CHECK (g[30] > -4.0);
    // slow part: a residual reduction remains for hundreds of ms (release 1000 ms)
    CHECK (g[300] < -0.2);
    // without PDR the whole release is slow
    s.pdrMs = 0.0;
    auto e2 = makeEngine (s, fs);
    auto out2 = runStereo (e2, in, in);
    const auto g2 = frameGainDb (out2.l, in, fs, pre + burst, 900);
    CHECK (g2[30] < g[30] - 2.0);
}
