// Spec sections 1, 7, 8, 10, 12: baseline curve, analog noise, polarity, latency, routing, mono/stereo, smoothing.
#include <catch2/catch_test_macros.hpp>

#include <complex>

#include "TestHelpers.h"

using namespace testing;
using cg::TaperId;

namespace
{
constexpr double fsTest = 48000.0;

// Amplitude of harmonic h of a 1 kHz analysis over an integer number of periods.
double harmonic (const std::vector<double>& y, size_t from, size_t len, double f, double fs)
{
    std::complex<double> acc;
    for (size_t i = 0; i < len; ++i)
        acc += y[from + i] * std::polar (1.0, -2.0 * cg::pi * f * double (from + i) / fs);
    return 2.0 * std::abs (acc) / double (len);
}
} // namespace

TEST_CASE ("baseline curve y = x + 0.00316 x^3 is always active (spec 1.1)", "[strip]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        auto s = cg::Settings::defaults();
        s.eqBypass = true;
        s.dynBypass = true;
        auto e = makeEngine (fs, s);
        std::vector<double> x (4096);
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = std::sin (0.37 * double (i)) * (0.1 + 0.9 * double (i % 7) / 6.0);
        auto y = x;
        run (e, y);
        for (size_t i = 0; i < x.size(); ++i)
            REQUIRE (std::abs (y[i] - (x[i] + 0.00316 * x[i] * x[i] * x[i])) < 1e-12);
    }
    // harmonic test vectors: H3 = -98 / -74 / -62 dBc at -18 / -6 / 0 dBFS
    const double levels[] = { -18.0, -6.0, 0.0 }, h3ref[] = { -98.0, -74.0, -62.0 };
    for (int k = 0; k < 3; ++k)
    {
        auto e = makeEngine (fsTest);
        const double a = cg::dbToGain (levels[k]);
        std::vector<double> x (48000);
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = a * std::sin (2.0 * cg::pi * 1000.0 * double (i) / fsTest);
        auto y = x;
        run (e, y);
        const double h1 = harmonic (y, 24000, 24000, 1000.0, fsTest), h3 = harmonic (y, 24000, 24000, 3000.0, fsTest);
        const double h2 = harmonic (y, 24000, 24000, 2000.0, fsTest);
        INFO ("level " << levels[k] << " H3 " << cg::gainToDb (h3 / h1));
        CHECK (std::abs (cg::gainToDb (h3 / h1) - h3ref[k]) < 0.5);
        CHECK (cg::gainToDb (h2 / h1) < -94.0);
        // coefficient estimate: H3/H1 = c a^2 / 4 (1 + 3 c a^2 / 4)
        const double c = 4.0 * (h3 / h1) / (a * a) * (1.0 + 0.75 * 0.00316 * a * a);
        CHECK (std::abs (c / 0.00316 - 1.0) < 0.02);
    }
}

TEST_CASE ("analog switch adds uncorrelated white noise at -110.8 dBFS RMS, also on silence (spec 1.2)", "[strip]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        auto s = cg::Settings::defaults();
        s.analog = true;
        auto e = makeEngine (fs, s);
        std::vector<double> l (size_t (fs * 2.0), 0.0), r = l;
        run (e, l, &r);
        const size_t from = size_t (fs * 0.1);
        const double rl = rms (l, from, l.size()), rr = rms (r, from, r.size());
        INFO ("fs " << fs << " noise " << cg::gainToDb (rl));
        CHECK (std::abs (cg::gainToDb (rl) - -110.8) < 0.5);
        CHECK (std::abs (cg::gainToDb (rr) - -110.8) < 0.5);
        double cross = 0.0;
        for (size_t i = from; i < l.size(); ++i)
            cross += l[i] * r[i];
        cross /= double (l.size() - from) * rl * rr;
        CHECK (std::abs (cross) < 0.01);
        // flat spectrum: lag-1 autocorrelation ~ 0
        double ac = 0.0;
        for (size_t i = from + 1; i < l.size(); ++i)
            ac += l[i] * l[i - 1];
        CHECK (std::abs (ac / (double (l.size() - from) * rl * rl)) < 0.02);
    }
    // EQ responses unchanged by the analog switch (deterministic part)
    auto s = cg::Settings::defaults();
    s.lfGain = rOf (TaperId::lfGain, 8.5);
    const double off = stripMagnitudeDb (s, fsTest, 100.0);
    s.analog = true;
    CHECK (std::abs (stripMagnitudeDb (s, fsTest, 100.0) - off) < 0.01);
}

TEST_CASE ("polarity is an exact sign inversion; latency is zero (spec 1, 10)", "[strip]")
{
    auto s = cg::Settings::defaults();
    s.lfGain = rOf (TaperId::lfGain, 6.0);
    s.hpFreq = rOf (TaperId::hpFreq, 70.0);
    s.compRatio = rOf (TaperId::compRatio, 4.0);
    s.compThreshold = rOf (TaperId::compThreshold, -10.0);
    auto sInv = s;
    sInv.polarity = true;
    auto a = makeEngine (fsTest, s), b = makeEngine (fsTest, sInv);
    std::vector<double> x (20000);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 0.5 * std::sin (0.01 * double (i) * (1.0 + double (i) / 20000.0)) + 0.1 * std::sin (0.7 * double (i));
    auto ya = x, yb = x;
    run (a, ya);
    run (b, yb);
    for (size_t i = 0; i < x.size(); ++i)
        REQUIRE (yb[i] == -ya[i]);

    // impulse at defaults: the output peak is at the input sample
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        auto e = makeEngine (fs);
        std::vector<double> imp (1024, 0.0);
        imp[100] = 0.5;
        run (e, imp);
        size_t peak = 0;
        for (size_t i = 0; i < imp.size(); ++i)
            if (std::abs (imp[i]) > std::abs (imp[peak]))
                peak = i;
        CHECK (peak == 100);
        CHECK (cg::ConsoleEngine::getLatencySamples() == 0);
    }
}

TEST_CASE ("routing (spec 7): pre-EQ default, Ch Out post-EQ, filters to side-chain, split", "[strip][routing]")
{
    auto shelf = [] (cg::Settings s) {
        s.lfGain = rOf (TaperId::lfGain, 11.9);
        s.lfFreq = rOf (TaperId::lfFreq, 450.0);
        return s;
    };
    const auto base = staircase (comp (-10, 4), fsTest);
    // default: the shelf (+5.9 dB at 1 kHz) acts after the compressor: curve unchanged, output +5.9 dB
    const auto pre = staircase (shelf (comp (-10, 4)), fsTest);
    const double shelfDb = stripMagnitudeDb (shelf (cg::Settings::defaults()), fsTest, 1000.0);
    INFO ("shelf at 1 kHz " << shelfDb);
    CHECK (std::abs (shelfDb - 5.9) < 1.0); // off-grid shelf (11.9 dB on the sparse 450 Hz row): spec 10, <= 1 dB
    for (size_t k = 0; k < base.size(); ++k)
        CHECK (std::abs (pre[k] - (base[k] + shelfDb)) < 0.1);

    // Ch Out: the shelf moves the compressor curve by its gain (spec 9.2 row)
    auto post = shelf (comp (-10, 4));
    post.dynPostEq = true;
    const auto gp = staircase (post, fsTest);
    const double ref[] = { 8.90, 5.97, 1.82, -2.94, -8.26, -12.30 };
    const double lv[] = { -54, -30, -22, -14, -6, 0 };
    for (int k = 0; k < 6; ++k)
    {
        const double got = gp[size_t ((lv[k] + 54) / 2)];
        INFO ("post-EQ at " << lv[k] << ": " << got << " vs " << ref[k]);
        CHECK (std::abs (got - ref[k]) <= 1.0);
    }

    // filters to side-chain (HP 350): audio unfiltered, detector filtered: 1 kHz curve within 0.2 dB
    auto sc = comp (-10, 4);
    sc.hpFreq = rOf (TaperId::hpFreq, 350.0);
    sc.filtersSidechain = true;
    const auto gsc = staircase (sc, fsTest);
    for (size_t k = 0; k < base.size(); ++k)
        CHECK (std::abs (gsc[k] - base[k]) < 0.2);
    // ... and a 100 Hz tone passes the audio path unfiltered (-37 dB if filtered)
    auto scOnly = cg::Settings::defaults();
    scOnly.hpFreq = rOf (TaperId::hpFreq, 350.0);
    scOnly.filtersSidechain = true;
    CHECK (std::abs (stripMagnitudeDb (scOnly, fsTest, 100.0)) < 0.05);
    scOnly.filtersSidechain = false;
    CHECK (stripMagnitudeDb (scOnly, fsTest, 100.0) < -30.0);

    // split: filters before the dynamics -> a 200 Hz tone (HP 350: -19 dB) no longer drives the compressor
    auto split = comp (-10, 4);
    split.hpFreq = rOf (TaperId::hpFreq, 350.0);
    split.filtersSplit = true;
    auto noSplit = split;
    noSplit.filtersSplit = false;
    const double gSplit = stripMagnitudeDb (split, fsTest, 200.0, -6.0);
    const double gNo = stripMagnitudeDb (noSplit, fsTest, 200.0, -6.0);
    INFO ("split " << gSplit << " no split " << gNo);
    CHECK (gSplit > gNo + 5.0);
}

TEST_CASE ("EQ / dynamics bypass remove their sections; the baseline curve remains", "[strip]")
{
    auto s = comp (-10, 4);
    s.lfGain = rOf (TaperId::lfGain, 15.3);
    s.eqBypass = true;
    s.dynBypass = true;
    for (double f : { 100.0, 1000.0 })
        CHECK (std::abs (stripMagnitudeDb (s, fsTest, f, -6.0)) < 0.02);
}

TEST_CASE ("mono equals one channel of stereo with identical channels (D5); L/R identical", "[strip]")
{
    auto s = comp (-20, 6);
    s = expander (s, -20, 20);
    s.hmfGain = rOf (TaperId::hmfGain, 8.0);
    s.hpFreq = rOf (TaperId::hpFreq, 120.0);
    auto m = makeEngine (fsTest, s), st = makeEngine (fsTest, s);
    std::vector<double> x (30000);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = (i % 9000 < 4000 ? 0.5 : 0.01) * std::sin (0.05 * double (i));
    auto ym = x, yl = x, yr = x;
    run (m, ym);
    run (st, yl, &yr);
    for (size_t i = 0; i < x.size(); ++i)
    {
        REQUIRE (ym[i] == yl[i]);
        REQUIRE (yl[i] == yr[i]);
    }
}

TEST_CASE ("parameter changes are smoothed (D6): no step larger than the signal allows", "[strip]")
{
    auto e = makeEngine (fsTest);
    std::vector<double> x (size_t (fsTest * 0.2));
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 0.3 * std::sin (2.0 * cg::pi * 100.0 * double (i) / fsTest);
    auto y = x;
    // jump output gain to +12 dB, LF to +17 dB and polarity in the middle
    const size_t half = x.size() / 2;
    std::vector<double> a (y.begin(), y.begin() + long (half)), b (y.begin() + long (half), y.end());
    run (e, a);
    auto s = cg::Settings::defaults();
    s.outputGain = 1.0;
    s.lfGain = 1.0;
    s.lfFreq = rOf (TaperId::lfFreq, 100.0);
    s.polarity = true;
    s.dynPostEq = true;
    e.setSettings (s);
    run (e, b);
    std::vector<double> out (a);
    out.insert (out.end(), b.begin(), b.end());
    double maxStep = 0.0;
    for (size_t i = 1; i < out.size(); ++i)
        maxStep = std::max (maxStep, std::abs (out[i] - out[i - 1]));
    // a 100 Hz sine at the final level (0.3 * 4 * 7) changes by at most ~0.11 per sample
    CHECK (maxStep < 0.2);
    // settles within 20 ms + routing duck
    const double final = rms (b, b.size() - 4800, b.size());
    const double early = rms (b, size_t (0.025 * fsTest), size_t (0.025 * fsTest) + 480);
    CHECK (std::abs (cg::gainToDb (early / final)) < 1.0);
}

TEST_CASE ("extreme settings: output stays bounded (safety above +24 dBFS) and finite", "[strip]")
{
    auto s = cg::Settings::defaults();
    s.inputTrim = 1.0;
    s.outputGain = 1.0;
    s.lfGain = s.hfGain = s.lmfGain = s.hmfGain = 1.0;
    s.lmfQ = s.hmfQ = 0.0;
    s.compThreshold = 1.0;
    s.compRatio = 0.1;
    auto e = makeEngine (fsTest, s);
    std::vector<double> x (48000);
    uint32_t r = 1;
    for (auto& v : x)
    {
        r = r * 1664525u + 1013904223u;
        v = (double (r) / 4294967295.0) * 2.0 - 1.0;
    }
    auto y = x;
    run (e, y);
    for (double v : y)
    {
        REQUIRE (std::isfinite (v));
        REQUIRE (std::abs (v) <= 32.0);
    }
    CHECK (cg::ConsoleEngine::safetyClip (10.0) == 10.0);
}
