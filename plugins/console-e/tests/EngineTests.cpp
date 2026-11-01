// Spec r1 sections 3, 8, 9, 10 and test vectors 1, 5, 6: signal flow, routing, gains, polarity, saturation,
// analogue noise, latency, smoothing.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>

#include "TestHelpers.h"

using namespace craft::consolee::test;

namespace
{
constexpr double fs = 48000.0;

double gainAt (const Settings& s, double freq, double levelDb, double seconds = 1.0, int channels = 2)
{
    auto e = makeEngine (s, fs, channels);
    const size_t n = size_t (seconds * fs);
    const auto x = sine (freq, fs, levelDb, n);
    const auto y = run (e, x);
    const size_t len = wholeCycles (freq, fs, 0.2);
    return gainToDb (toneAmplitude (y[0], freq, fs, n - len, len) / toneAmplitude (x, freq, fs, n - len, len));
}

std::vector<double> noise (size_t n, double levelDb, unsigned seed = 1)
{
    std::vector<double> x (n);
    unsigned s = seed;
    const double a = dbToGain (levelDb);
    for (auto& v : x)
    {
        s = s * 1664525u + 1013904223u;
        v = a * (double (s >> 8) / double (1u << 24) * 2.0 - 1.0);
    }
    return x;
}

// Section 8 example: ratio 4, threshold -10, LF +12 dB at 100 Hz, -6 dBFS tones
Settings routingExample()
{
    Settings s = withCompressor (4.0, -10.0);
    s.eq.lfFreq = param::lfFreq.toNorm (100.0);
    s.eq.lfGain = param::bandGain.toNorm (12.0);
    return s;
}
} // namespace

TEST_CASE ("Neutral settings: output equals saturate(input) (test vector 1)", "[engine]")
{
    auto e = makeEngine (Settings {});
    const auto x = noise (48000, -6.0);
    const auto y = run (e, x);
    double worst = 0;
    for (size_t i = 0; i < x.size(); ++i)
        worst = std::max (worst, std::abs (y[0][i] - saturate (x[i])));
    CHECK (worst < 1.4e-7);
    // below -30 dBFS the saturation is itself below -100 dB re peak
    auto e2 = makeEngine (Settings {});
    const auto q = sine (997.0, fs, -30.0, 48000);
    const auto z = run (e2, q);
    double err = 0;
    for (size_t i = 0; i < q.size(); ++i)
        err = std::max (err, std::abs (z[1][i] - q[i]));
    CHECK (gainToDb (err / dbToGain (-30.0)) < -100.0);
}

TEST_CASE ("Zero latency: impulse comes out at sample 0", "[engine]")
{
    Settings s;
    s.eq.lmfGain = param::bandGain.toNorm (9.0);
    s = withCompressor (4.0, 0.0, s);
    auto e = makeEngine (s);
    CHECK (ConsoleEngine::getLatencySamples() == 0);
    std::vector<double> x (512, 0.0);
    x[0] = 0.01;
    const auto y = run (e, x);
    const auto peak = std::max_element (y[0].begin(), y[0].end(), [] (double a, double b) { return std::abs (a) < std::abs (b); });
    CHECK (peak - y[0].begin() == 0);
}

TEST_CASE ("Input trim and output fader are exact dB gains (test vector 5)", "[engine][gain]")
{
    for (double r : { 0.0, 0.1, 0.2, 0.35, 0.5, 0.8, 1.0 })
    {
        Settings s;
        s.outputFader = r;
        const double expected = param::outputFader (r);
        CHECK (std::abs (gainAt (s, 1000.0, -40.0, 0.3) - expected) < 0.002);
        Settings t;
        t.inputTrim = r;
        // the trim sits before the saturation: measure low enough for it to be negligible
        CHECK (std::abs (gainAt (t, 1000.0, -60.0, 0.3) - param::inputTrim (r)) < 0.002);
    }
}

TEST_CASE ("Polarity inverts exactly (test vector 5)", "[engine][gain]")
{
    Settings s = routingExample();
    s.eq.hmfGain = param::bandGain.toNorm (-6.0);
    Settings p = s;
    p.polarity = true;
    auto a = makeEngine (s), b = makeEngine (p);
    const auto x = noise (24000, -3.0, 7);
    const auto ya = run (a, x), yb = run (b, x);
    for (size_t i = 0; i < x.size(); ++i)
        REQUIRE (ya[0][i] == -yb[0][i]);
}

TEST_CASE ("Fixed saturation: gain and third harmonic (section 9)", "[engine][saturation]")
{
    const size_t n = 48000;
    for (auto [level, h3] : { std::pair { 0.0, -62.0 }, std::pair { -6.0, -74.0 } })
    {
        auto e = makeEngine (Settings {});
        const auto x = sine (1000.0, fs, level, n);
        const auto y = run (e, x);
        const size_t len = wholeCycles (1000.0, fs, 0.5);
        const double fund = toneAmplitude (y[0], 1000.0, fs, n - len, len);
        const double third = toneAmplitude (y[0], 3000.0, fs, n - len, len);
        INFO ("level " << level << ": H3 " << gainToDb (third / fund) << " dBc");
        CHECK (std::abs (gainToDb (third / fund) - h3) <= 3.0);
        if (level == 0.0)
            CHECK (std::abs (gainToDb (fund / dbToGain (level)) - 0.021) < 0.003);
    }
    // the output fader does not move it, the input trim does
    Settings s;
    s.outputFader = param::outputFader.toNorm (-12.0);
    auto e = makeEngine (s);
    const auto x = sine (1000.0, fs, 0.0, n);
    const auto y = run (e, x);
    const size_t len = wholeCycles (1000.0, fs, 0.5);
    CHECK (std::abs (gainToDb (toneAmplitude (y[0], 3000, fs, n - len, len) / toneAmplitude (y[0], 1000, fs, n - len, len)) + 62.0) <= 3.0);
}

TEST_CASE ("Analogue switch: noise floor and leakage (test vector 6)", "[engine][analogue]")
{
    Settings s;
    s.analogue = true;
    auto e = makeEngine (s);
    const size_t n = size_t (2.0 * fs);
    const auto y = run (e, std::vector<double> (n, 0.0));
    for (int c = 0; c < 2; ++c)
    {
        double sum = 0;
        for (size_t i = n / 2; i < n; ++i)
            sum += y[size_t (c)][i] * y[size_t (c)][i];
        const double rmsDb = 10.0 * std::log10 (sum / double (n / 2));
        INFO ("channel " << c << ": " << rmsDb << " dBFS rms");
        CHECK (std::abs (rmsDb + 110.7) <= 1.5);
    }
    // channels are independent
    double cross = 0, e0 = 0, e1 = 0;
    for (size_t i = n / 2; i < n; ++i)
    {
        cross += y[0][i] * y[1][i];
        e0 += y[0][i] * y[0][i];
        e1 += y[1][i] * y[1][i];
    }
    CHECK (std::abs (cross / std::sqrt (e0 * e1)) < 0.05);
    // leakage: a tone in the left channel appears at about -69 dB in the right one
    auto e2 = makeEngine (s);
    const auto x = sine (1000.0, fs, -6.0, n);
    const auto z = run (e2, x, std::vector<double> (n, 0.0));
    const size_t len = wholeCycles (1000.0, fs, 0.5);
    const double leak = gainToDb (toneAmplitude (z[1], 1000, fs, n - len, len) / toneAmplitude (z[0], 1000, fs, n - len, len));
    CHECK (std::abs (leak + 69.0) < 1.0);
    // off: silence stays silent
    auto e3 = makeEngine (Settings {});
    const auto q = run (e3, std::vector<double> (4800, 0.0));
    CHECK (*std::max_element (q[0].begin(), q[0].end()) == 0.0);
    // it does not change the frequency response
    CHECK (std::abs (gainAt (s, 1000.0, -20.0, 0.3) - gainAt (Settings {}, 1000.0, -20.0, 0.3)) < 0.01);
}

TEST_CASE ("Routing: dynamics/EQ order, EQ to sidechain, split (section 8)", "[engine][routing]")
{
    const Settings base = routingExample();
    // default: dynamics -> EQ
    const double def = gainAt (base, 50.0, -6.0, 1.5);
    INFO ("default " << def);
    CHECK (std::abs (def - 3.2) <= 0.75);
    Settings co = base;
    co.channelOut = true;
    const double chOut = gainAt (co, 50.0, -6.0, 1.5);
    INFO ("channel out " << chOut);
    CHECK (std::abs (chOut + 4.9) <= 0.75);
    Settings sc = base;
    sc.eqToSidechain = true;
    const double side = gainAt (sc, 50.0, -6.0, 1.5);
    INFO ("EQ to sidechain " << side);
    CHECK (std::abs (side + 17.6) <= 0.75);

    // HP at r = 1: split puts the filter before the dynamics
    Settings hp = withCompressor (4.0, -10.0);
    hp.eq.hp = 1.0;
    const double unsplit = gainAt (hp, 50.0, -6.0, 1.5);
    hp.split = true;
    const double split = gainAt (hp, 50.0, -6.0, 1.5);
    INFO ("unsplit " << unsplit << " split " << split);
    CHECK (std::abs (unsplit + 64.2) <= 1.0);
    CHECK (std::abs (split + 52.2) <= 1.0);
}

TEST_CASE ("Bypass switches pass audio unchanged", "[engine][routing]")
{
    Settings s = routingExample();
    s.eq.hp = 0.6;
    s.eq.lp = 0.6;
    s.eq.hmfGain = 0.9;
    s.eqBypass = true;
    s.dynamicsBypass = true;
    auto e = makeEngine (s);
    const auto x = noise (24000, -12.0, 3);
    const auto y = run (e, x);
    double worst = 0;
    for (size_t i = 0; i < x.size(); ++i)
        worst = std::max (worst, std::abs (y[0][i] - saturate (x[i])));
    CHECK (worst < 1.4e-7);

    // EQ bypass alone: dynamics still act
    Settings d = withCompressor (4.0, -10.0);
    d.eq.lfGain = 1.0;
    d.eqBypass = true;
    CHECK (std::abs (gainAt (d, 1000.0, -6.0, 1.0) + 10.33) < 0.5);
    // dynamics bypass alone: EQ still acts
    Settings q;
    q.eq.lmfFreq = 0.5;
    q.eq.lmfGain = param::bandGain.toNorm (12.0);
    q.eq.lmfQ = 0.5;
    q.dynamicsBypass = true;
    q = withCompressor (4.0, -20.0, q);
    CHECK (std::abs (gainAt (q, 800.0, -20.0, 0.5) - designEq (q.eq, fs).lmf.magnitudeDb (800.0, fs)) < 0.05);
}

TEST_CASE ("Mono variant equals one channel of the stereo variant", "[engine]")
{
    Settings s = routingExample();
    s.eq.hfGain = 0.8;
    s.expRange = 0.6;
    s.expThreshold = 0.4;
    auto st = makeEngine (s, fs, 2);
    auto mo = makeEngine (s, fs, 1);
    const auto x = noise (24000, -10.0, 11);
    const auto ys = run (st, x), ym = run (mo, x);
    double worst = 0;
    for (size_t i = 0; i < x.size(); ++i)
        worst = std::max (worst, std::abs (ys[0][i] - ym[0][i]));
    CHECK (worst < 1e-12);
}

TEST_CASE ("Parameter changes are click-free (decision D5)", "[engine][smoothing]")
{
    // a 1 kHz tone while controls jump: the sample-to-sample step never exceeds what the tone itself allows
    Settings s;
    auto e = makeEngine (s);
    const size_t block = 480;
    const auto x = sine (1000.0, fs, -12.0, block * 200);
    std::vector<double> y;
    double prevOut = 0;
    double maxStep = 0;
    for (size_t b = 0; b < 200; ++b)
    {
        Settings t = s;
        // every 20 ms jump something
        switch (b % 10)
        {
            case 1: t.outputFader = 1.0; break;
            case 2: t.inputTrim = 0.8; break;
            case 3: t.eq.lmfGain = 1.0; t.eq.lmfFreq = 0.5; break;
            case 4: t.polarity = true; break;
            case 5: t.eqBypass = true; t.eq.hfGain = 1.0; break;
            case 6: t.channelOut = true; t = withCompressor (8.0, -20.0, t); break;
            case 7: t.eq.hp = 1.0; break;
            case 8: t.eq.lfBell = true; t.eq.lfGain = 0.0; break;
            default: break;
        }
        e.setSettings (t);
        std::vector<double> l (x.begin() + long (b * block), x.begin() + long ((b + 1) * block)), r = l;
        double* ptr[2] = { l.data(), r.data() };
        e.process (ptr, int (block));
        for (double v : l)
        {
            maxStep = std::max (maxStep, std::abs (v - prevOut));
            prevOut = v;
        }
    }
    // the tone's own maximum step: 2 pi f / fs * amplitude, times the largest gain in play (+12 dB fader, +15 dB EQ, trim)
    const double toneStep = 2.0 * pi * 1000.0 / fs * dbToGain (-12.0);
    INFO ("max step " << maxStep << " vs tone step " << toneStep);
    CHECK (maxStep < toneStep * dbToGain (12.0 + 10.8 + 18.0));
}
