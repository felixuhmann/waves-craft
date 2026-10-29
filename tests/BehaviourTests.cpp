// Spec r1 sections 1, 3, 4, 6, 8: quantisation, bit-exact neutral path, latency,
// static accuracy vs. the model, level independence, smoothing.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cstring>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;
namespace pm = craft::dsp::param;

TEST_CASE ("Parameter mappings and quantisation", "[params]")
{
    CHECK (pm::Gain::fromNorm (1.0) == 0.0);
    CHECK (pm::Gain::fromNorm (0.0) == -24.0);
    CHECK (pm::Gain::fromNorm (0.75) == -6.0);
    CHECK (pm::Gain::fromNorm (0.7512) == -6.0);  // -5.971 -> -6.0
    CHECK (pm::Gain::fromNorm (0.7530) == -5.9);  // -5.928 -> -5.9

    CHECK (pm::Width::fromNorm (0.5) == 1.0);
    CHECK (pm::Width::fromNorm (0.3) == 0.48);
    CHECK (pm::Width::fromNorm (0.6) == 1.32);
    CHECK (pm::Width::fromNorm (0.8) == 2.08);
    CHECK (pm::Width::fromNorm (1.0) == 3.0);
    CHECK (pm::Width::fromNorm (0.3011) == 0.48);
    CHECK_THAT (pm::Width::toNorm (1.0), WithinAbs (0.5, 1e-15));
    CHECK_THAT (pm::Width::toNorm (2.08), WithinAbs (0.8, 1e-15));

    CHECK (pm::Rotation::fromNorm (0.5) == 0.0);
    CHECK (pm::Rotation::fromNorm (1.0) == 45.0);
    CHECK (pm::Rotation::fromNorm (0.3) == -18.0);
    CHECK (pm::Rotation::fromNorm (0.30004) == -18.0);

    CHECK (pm::Asymmetry::fromNorm (0.5) == 0.0);
    CHECK (pm::Asymmetry::fromNorm (0.0) == -90.0);
    CHECK (pm::Asymmetry::fromNorm (0.502) == 0.0); // 0.36 deg -> 0
    CHECK (pm::Asymmetry::fromNorm (0.7) == 36.0);

    CHECK (pm::Shuffle::fromNorm (0.0) == 1.0);
    CHECK (pm::Shuffle::fromNorm (1.0) == 3.0);
    CHECK (pm::Shuffle::fromNorm (0.02) == 1.0);  // 1.04 -> 1.0
    CHECK (pm::Shuffle::fromNorm (0.25) == 1.5);

    CHECK (pm::Frequency::fromNorm (0.0) == 350.0);
    CHECK (pm::Frequency::fromNorm (1.0) == 1400.0);
    CHECK (pm::Frequency::fromNorm (0.447) == 650.0);
    CHECK (pm::Frequency::fromNorm (pm::Frequency::toNorm (650.0)) == 650.0);
    CHECK_THAT (pm::Frequency::toNorm (650.0), WithinAbs (0.4466, 1e-4));

    CHECK (pm::BassTrim::fromNorm (0.5) == 0.0);
    CHECK (pm::BassTrim::fromNorm (1.0) == 3.0);
    CHECK (pm::BassTrim::fromNorm (0.505) == 0.0); // 0.03 dB -> 0.0
    CHECK (pm::BassTrim::fromNorm (0.0) == -3.0);

    // every mapping round-trips at every quantisation step
    for (int i = 0; i <= 300; ++i)
        CHECK (pm::Width::fromNorm (pm::Width::toNorm (i / 100.0)) == i / 100.0);
    for (int i = 350; i <= 1400; ++i)
        CHECK (pm::Frequency::fromNorm (pm::Frequency::toNorm (double (i))) == double (i));
}

namespace
{
std::vector<float> runFloat (Variant v, const Settings& s, double fs, std::vector<float> l, std::vector<float> r,
                             std::vector<float>* rOut = nullptr)
{
    auto e = makeEngine (v, s, fs);
    e.process (l.data(), r.data(), int (l.size()));
    if (rOut)
        *rOut = r;
    return l;
}

Settings settingsFromNorm (double g, double w, double rot, double asym, double sh, double fr, double bt)
{
    Settings s;
    s.gainDb = pm::Gain::fromNorm (g);
    s.width = pm::Width::fromNorm (w);
    s.rotationDeg = pm::Rotation::fromNorm (rot);
    s.asymmetryDeg = pm::Asymmetry::fromNorm (asym);
    s.shuffle = pm::Shuffle::fromNorm (sh);
    s.frequencyHz = pm::Frequency::fromNorm (fr);
    s.bassTrimDb = pm::BassTrim::fromNorm (bt);
    return s;
}
} // namespace

TEST_CASE ("Settings between quantisation steps give bit-identical output", "[params]")
{
    const auto l = noise (4096, 1), r = noise (4096, 2);
    std::vector<float> r1, r2;
    const auto a = runFloat (Variant::Full, settingsFromNorm (0.7, 0.8, 0.6, 0.3, 0.9, 0.3, 0.7), 48000, l, r, &r1);
    const auto b = runFloat (Variant::Full, settingsFromNorm (0.70001, 0.80001, 0.60001, 0.30001, 0.90001, 0.30001, 0.70001),
                             48000, l, r, &r2);
    CHECK (std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0);
    CHECK (std::memcmp (r1.data(), r2.data(), r1.size() * sizeof (float)) == 0);
}

TEST_CASE ("Neutral settings are a bit-exact passthrough", "[bypass]")
{
    const auto l = noise (8192, 3, 1.0f), r = noise (8192, 4, 1.0f);
    for (auto v : { Variant::Full, Variant::Imager })
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            std::vector<float> ro;
            const auto lo = runFloat (v, Settings {}, fs, l, r, &ro);
            CHECK (std::memcmp (lo.data(), l.data(), l.size() * sizeof (float)) == 0);
            CHECK (std::memcmp (ro.data(), r.data(), r.size() * sizeof (float)) == 0);
        }

    // Frequency alone does not activate the shuffler (Shuffle 1.0, Trim 0.0)
    Settings s;
    s.frequencyHz = 1400;
    std::vector<float> ro;
    const auto lo = runFloat (Variant::Full, s, 48000, l, r, &ro);
    CHECK (std::memcmp (lo.data(), l.data(), l.size() * sizeof (float)) == 0);
}

TEST_CASE ("Neutral reached by automation is bit-exact once settled", "[bypass][smoothing]")
{
    auto e = makeEngine (Variant::Full, settingsFromNorm (0.5, 0.9, 0.2, 0.8, 1.0, 0.0, 0.0), 48000);
    std::vector<float> l = noise (48000, 5), r = noise (48000, 6);
    e.process (l.data(), r.data(), int (l.size()));
    e.setSettings (Settings {});
    // run 4 s of audio to let every smoother settle
    for (int k = 0; k < 4; ++k)
    {
        l = noise (48000, 7);
        r = noise (48000, 8);
        e.process (l.data(), r.data(), int (l.size()));
    }
    REQUIRE (e.isSettled());
    const auto l2 = noise (4096, 9), r2 = noise (4096, 10);
    auto lo = l2, ro = r2;
    e.process (lo.data(), ro.data(), 4096);
    CHECK (std::memcmp (lo.data(), l2.data(), l2.size() * sizeof (float)) == 0);
    CHECK (std::memcmp (ro.data(), r2.data(), r2.size() * sizeof (float)) == 0);
}

TEST_CASE ("Host bypass is bit-exact once faded", "[bypass]")
{
    auto e = makeEngine (Variant::Full, settingsFromNorm (0.3, 0.9, 0.7, 0.2, 0.6, 0.5, 0.8), 48000);
    e.setBypassed (true);
    std::vector<float> l = noise (4800, 11), r = noise (4800, 12);
    e.process (l.data(), r.data(), 4800); // 100 ms > fade
    const auto l2 = noise (4096, 13), r2 = noise (4096, 14);
    auto lo = l2, ro = r2;
    e.process (lo.data(), ro.data(), 4096);
    CHECK (std::memcmp (lo.data(), l2.data(), l2.size() * sizeof (float)) == 0);
    CHECK (std::memcmp (ro.data(), r2.data(), r2.size() * sizeof (float)) == 0);
}

TEST_CASE ("Imager variant output equals Full at Shuffle 1.0 / Trim 0.0", "[variant]")
{
    const auto l = noise (8192, 15), r = noise (8192, 16);
    for (unsigned seed = 0; seed < 20; ++seed)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<double> u (0.0, 1.0);
        auto s = settingsFromNorm (u (rng), u (rng), u (rng), u (rng), 0.0, u (rng), 0.5);
        s.swap = rng() & 1;
        s.polarity1 = rng() & 1;
        s.polarity2 = rng() & 1;
        s.msInput = rng() & 1;
        std::vector<float> rf, ri;
        const auto lf = runFloat (Variant::Full, s, 48000, l, r, &rf);
        const auto li = runFloat (Variant::Imager, s, 48000, l, r, &ri);
        double maxErr = 0;
        for (size_t i = 0; i < l.size(); ++i)
            maxErr = std::max ({ maxErr, double (std::abs (lf[i] - li[i])), double (std::abs (rf[i] - ri[i])) });
        CHECK (maxErr <= 6e-8);
    }
}

TEST_CASE ("Zero latency", "[latency]")
{
    CHECK (ImagerEngine::getLatencySamples() == 0);
    for (double sh : { 0.0, 1.0 })
    {
        Settings s = settingsFromNorm (0.9, 0.8, 0.55, 0.5, sh, 0.5, 0.5);
        auto e = makeEngine (Variant::Full, s, 48000);
        auto ir = impulseResponse (e, 0, 4096);
        // The output starts at sample 0 and its largest sample is at sample 0
        size_t peak = 0;
        for (size_t i = 1; i < ir.out1.size(); ++i)
            if (std::abs (ir.out1[i]) > std::abs (ir.out1[peak]))
                peak = i;
        CHECK (peak == 0);
        CHECK (ir.out1[0] != 0.0);
        if (sh == 0.0) // broadband-only: the whole response is in sample 0
            for (size_t i = 1; i < ir.out1.size(); ++i)
                REQUIRE (ir.out1[i] == 0.0);
    }
}

TEST_CASE ("Static response matches the section 4-5 model within -100 dB (sweeps and noise)", "[accuracy][samplerate]")
{
    std::mt19937 rng (1234);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int trial = 0; trial < 12; ++trial)
        {
            auto s = settingsFromNorm (u (rng), u (rng), u (rng), u (rng), trial % 3 == 0 ? 0.0 : u (rng), u (rng),
                                       trial % 4 == 0 ? 0.5 : u (rng));
            s.swap = rng() & 1;
            s.polarity1 = rng() & 1;
            s.polarity2 = rng() & 1;
            s.msInput = rng() & 1;
            for (auto v : { Variant::Full, Variant::Imager, Variant::MsMatrix })
            {
                const int n = 16384;
                const auto l = trial % 2 ? sweep (n, fs) : noise (n, unsigned (trial + 100));
                const auto r = trial % 2 ? noise (n, unsigned (trial + 200)) : sweep (n, fs);
                std::vector<float> ro;
                const auto lo = runFloat (v, s, fs, l, r, &ro);
                ReferenceModel model (s, v, fs);
                double peak = 0, err = 0;
                for (int i = 0; i < n; ++i)
                {
                    double m1, m2;
                    model.process (l[size_t (i)], r[size_t (i)], m1, m2);
                    peak = std::max ({ peak, std::abs (m1), std::abs (m2) });
                    err = std::max ({ err, std::abs (m1 - lo[size_t (i)]), std::abs (m2 - ro[size_t (i)]) });
                }
                INFO ("fs " << fs << " trial " << trial << " variant " << int (v));
                if (peak > 0)
                    CHECK (20 * std::log10 (std::max (err, 1e-30) / peak) <= -100.0);
            }
        }
}

TEST_CASE ("No level dependence (-30 / -12 / -3 dBFS)", "[accuracy]")
{
    auto s = settingsFromNorm (0.8, 0.85, 0.6, 0.4, 0.8, 0.3, 0.7);
    const int n = 8192;
    const auto nl = noise (n, 21, 1.0f), nr = noise (n, 22, 1.0f);
    std::vector<std::vector<double>> outs;
    const double levels[] = { -30.0, -12.0, -3.0 };
    for (double lev : levels)
    {
        const double g = std::pow (10.0, lev / 20.0);
        std::vector<double> l (static_cast<size_t> (n)), r (static_cast<size_t> (n));
        for (int i = 0; i < n; ++i)
        {
            l[size_t (i)] = g * nl[size_t (i)];
            r[size_t (i)] = g * nr[size_t (i)];
        }
        auto e = makeEngine (Variant::Full, s, 48000);
        e.process (l.data(), r.data(), n);
        for (auto& x : l)
            x /= g;
        outs.push_back (l);
    }
    double peak = 0, err = 0;
    for (int i = 0; i < n; ++i)
    {
        peak = std::max (peak, std::abs (outs[0][size_t (i)]));
        err = std::max ({ err, std::abs (outs[0][size_t (i)] - outs[1][size_t (i)]), std::abs (outs[0][size_t (i)] - outs[2][size_t (i)]) });
    }
    CHECK (20 * std::log10 (std::max (err, 1e-30) / peak) <= -150.0);
}

namespace
{
// Run DC input (1, 0.25) through the engine, switch settings at `stepAt`, return output channel 1.
std::vector<double> stepResponse (const Settings& from, const Settings& to, double fs, int n, int stepAt,
                                  std::vector<double>* out2 = nullptr, double in1 = 1.0, double in2 = 0.25)
{
    auto e = makeEngine (Variant::Full, from, fs);
    std::vector<double> l (size_t (n), in1), r (size_t (n), in2);
    e.process (l.data(), r.data(), stepAt);
    e.setSettings (to);
    e.process (l.data() + stepAt, r.data() + stepAt, n - stepAt);
    if (out2)
        *out2 = r;
    return l;
}
} // namespace

TEST_CASE ("Broadband smoothing: first order, tau 30-50 ms, onset immediate", "[smoothing]")
{
    const double fs = 48000;
    Settings a, b;
    b.gainDb = -12.0;
    const int n = int (fs), at = 1000;
    const auto y = stepResponse (a, b, fs, n, at);
    const double y0 = y[size_t (at - 1)], yEnd = std::pow (10.0, -12.0 / 20.0);
    // onset within 3 ms
    CHECK (std::abs (y[size_t (at + int (0.003 * fs))] - y0) > 1e-3);
    // remaining error after 12 ms
    const double ratio = (y[size_t (at + int (0.012 * fs))] - yEnd) / (y0 - yEnd);
    CHECK_THAT (ratio, WithinAbs (0.741, 0.01));
    // time constant: time to 1/e
    int i = at;
    while (std::abs (y[size_t (i)] - yEnd) > std::abs (y0 - yEnd) / std::exp (1.0))
        ++i;
    const double tau = (i - at) / fs;
    CHECK (tau >= 0.030);
    CHECK (tau <= 0.050);
}

TEST_CASE ("Toggles crossfade without discontinuities", "[smoothing]")
{
    const double fs = 48000;
    for (int which = 0; which < 4; ++which)
    {
        Settings a, b;
        if (which == 0) b.swap = true;
        if (which == 1) b.polarity1 = true;
        if (which == 2) b.polarity2 = true;
        if (which == 3) b.msInput = true;
        std::vector<double> y2;
        const auto y1 = stepResponse (a, b, fs, 24000, 1000, &y2);
        double maxJump = 0;
        for (size_t i = 1; i < y1.size(); ++i)
            maxJump = std::max ({ maxJump, std::abs (y1[i] - y1[i - 1]), std::abs (y2[i] - y2[i - 1]) });
        INFO ("toggle " << which);
        CHECK (maxJump < 2.0 / (0.040 * fs)); // a hard switch would jump by up to 2
    }
}

TEST_CASE ("Shuffle smoothing: S-shaped, 10-90 % about 160 ms", "[smoothing]")
{
    const double fs = 48000;
    Settings a, b;
    a.msInput = b.msInput = true;
    a.shuffle = 2.0;
    b.shuffle = 3.0;
    const int n = int (fs), at = 1000;
    std::vector<double> y2;
    // DC on S only: output channel 1 = S gain
    const auto y1 = stepResponse (a, b, fs, n, at, &y2, 0.0, 1.0);
    std::vector<double> sOut (y1.size());
    for (size_t i = 0; i < y1.size(); ++i)
        sOut[i] = 0.5 * (y1[i] - y2[i]);
    const double s0 = sOut[size_t (at - 1)], s1 = sOut.back();
    int t10 = -1, t90 = -1;
    for (int i = at; i < n; ++i)
    {
        const double frac = (sOut[size_t (i)] - s0) / (s1 - s0);
        if (t10 < 0 && frac >= 0.1) t10 = i;
        if (t90 < 0 && frac >= 0.9) t90 = i;
    }
    const double rise = (t90 - t10) / fs;
    CHECK (rise > 0.13);
    CHECK (rise < 0.19);
    // S-shape: the slope right after the step is much smaller than the maximum slope
    double maxSlope = 0;
    for (int i = at + 1; i < n; ++i)
        maxSlope = std::max (maxSlope, std::abs (sOut[size_t (i)] - sOut[size_t (i - 1)]));
    CHECK (std::abs (sOut[size_t (at + 10)] - sOut[size_t (at + 9)]) < 0.2 * maxSlope);
}

TEST_CASE ("Shuffler engage / disengage is click-free and returns to bit-exact bypass", "[smoothing][bypass]")
{
    const double fs = 48000;
    auto e = makeEngine (Variant::Full, Settings {}, fs);
    const int n = 4 * int (fs);
    std::vector<double> l (static_cast<size_t> (n)), r (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i)
    {
        l[size_t (i)] = 0.5 * std::sin (2 * pi * 100 * i / fs);
        r[size_t (i)] = 0.5 * std::sin (2 * pi * 157 * i / fs);
    }
    auto lo = l, ro = r;
    Settings on;
    on.shuffle = 3.0;
    on.bassTrimDb = 3.0;
    e.process (lo.data(), ro.data(), 1000);
    e.setSettings (on);
    e.process (lo.data() + 1000, ro.data() + 1000, int (fs));
    e.setSettings (Settings {});
    e.process (lo.data() + 1000 + int (fs), ro.data() + 1000 + int (fs), n - 1000 - int (fs));
    double maxJump = 0;
    for (size_t i = 1; i < size_t (n); ++i)
        maxJump = std::max ({ maxJump, std::abs (lo[i] - lo[i - 1]), std::abs (ro[i] - ro[i - 1]) });
    // a 157 Hz sine at amplitude ~1.6 changes by at most ~0.033 per sample; a click would be far larger
    CHECK (maxJump < 0.05);
    CHECK (e.isSettled());
    CHECK (! e.isShufflerInCircuit());
    for (int i = n - 1000; i < n; ++i)
    {
        REQUIRE (lo[size_t (i)] == l[size_t (i)]);
        REQUIRE (ro[size_t (i)] == r[size_t (i)]);
    }
}
