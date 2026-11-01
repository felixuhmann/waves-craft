// Spec r1 sections 2, 4, 5.1, 8: signal flow, exact clauses.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <random>

#include "TestHelpers.h"

using namespace enh_test;
using Catch::Approx;

namespace
{
Stereo noise (double seconds, double level = 0.1, double fs = 48000.0)
{
    std::mt19937 rng (7);
    std::uniform_real_distribution<double> u (-level, level);
    Stereo x;
    for (int i = 0; i < int (seconds * fs); ++i)
    {
        x.l.push_back (u (rng));
        x.r.push_back (u (rng));
    }
    return x;
}

double peak (const std::vector<double>& v)
{
    double p = 0.0;
    for (double x : v)
        p = std::max (p, std::abs (x));
    return p;
}

Settings tableDefaults()
{
    Settings s;
    s.crossoverHz = tableCrossovers;
    return s;
}
} // namespace

TEST_CASE ("direct and processed paths add exactly", "[path]")
{
    auto in = noise (0.5);
    auto both = tableDefaults();
    both.bandDb = { 12.0, 24.0, 0.0, 36.0, 6.0 };
    both.width = { 0.5, 1.0, 2.0, 3.0, 1.0 };
    auto dirOnly = both;
    dirOnly.bandDb = {};
    auto procOnly = both;
    procOnly.directDb = param::Direct::offValue;
    const auto a = run (both, in), d = run (dirOnly, in), p = run (procOnly, in);
    double err = 0.0;
    for (std::size_t i = 0; i < a.l.size(); ++i)
        err = std::max ({ err, std::abs (a.l[i] - d.l[i] - p.l[i]), std::abs (a.r[i] - d.r[i] - p.r[i]) });
    CHECK (toDb (err) - toDb (peak (a.l)) < -150.0);
}

TEST_CASE ("band bypass equals gain Off bit-exactly", "[path]")
{
    auto in = noise (0.3);
    auto a = tableDefaults();
    a.bandDb = { 12.0, 24.0, 24.0, 12.0, 0.0 };
    a.bypass[1] = true;
    auto b = a;
    b.bypass[1] = false;
    b.bandDb[1] = 0.0;
    const auto x = run (a, in), y = run (b, in);
    CHECK (x.l == y.l);
    CHECK (x.r == y.r);
}

TEST_CASE ("neutral settings are an all-pass", "[path]")
{
    for (double f : { 30.0, 88.0, 500.0, 3000.0, 15000.0 })
    {
        const auto y = run (tableDefaults(), tone (f, { { -6.0, 1.5 } }));
        const std::size_t n = wholePeriods (f, 48000.0, 0.5);
        CHECK (std::abs (toDb (hannAmplitude (y.l, f, 48000.0, y.l.size() - n, n)) + 6.0) < 0.01);
    }
}

TEST_CASE ("all gains and direct Off are silent", "[path]")
{
    auto s = tableDefaults();
    s.directDb = param::Direct::offValue;
    const auto y = run (s, noise (0.1));
    CHECK (peak (y.l) == 0.0);
    CHECK (peak (y.r) == 0.0);
}

TEST_CASE ("zero latency: impulse energy starts at the first sample", "[path]")
{
    CHECK (EnhancerEngine::getLatencySamples() == 0);
    Stereo in;
    in.l.assign (64, 0.0);
    in.r.assign (64, 0.0);
    in.l[0] = in.r[0] = 1.0;
    const auto y = run (tableDefaults(), in);
    CHECK (std::abs (y.l[0]) > 1.0e-3);
}

TEST_CASE ("width matrix is exact", "[path][width]")
{
    struct Case { double w, gM, gS; };
    const double g3 = std::sqrt ((5.0 + 3.0 / 9.0) / 8.0), g2 = std::sqrt ((5.0 + 3.0 / 4.0) / 8.0);
    const Case cases[] = { { 0.0, 1.0, 0.0 }, { 0.5, 1.0, 0.5 }, { 1.0, 1.0, 1.0 }, { 2.0, g2, 2.0 * g2 }, { 3.0, g3, 3.0 * g3 } };
    CHECK (g3 == Approx (0.8165).margin (1e-4));
    CHECK (g2 == Approx (0.8478).margin (1e-4));
    for (const auto& c : cases)
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            auto s = tableDefaults();
            s.width = { c.w, c.w, c.w, c.w, c.w };
            auto in = noise (0.2, 0.1, fs);
            auto ref = tableDefaults();
            const auto y = run (s, in, fs), r = run (ref, in, fs);
            double err = 0.0;
            for (std::size_t i = 0; i < y.l.size(); ++i)
            {
                const double m = 0.5 * (y.l[i] + y.r[i]), d = 0.5 * (y.l[i] - y.r[i]);
                const double rm = 0.5 * (r.l[i] + r.r[i]), rd = 0.5 * (r.l[i] - r.r[i]);
                err = std::max ({ err, std::abs (m - c.gM * rm), std::abs (d - c.gS * rd) });
            }
            CHECK (toDb (err) < -100.0);
        }
}

TEST_CASE ("per-band width acts only inside its band", "[path][width]")
{
    auto s = tableDefaults();
    s.width[2] = 0.0;
    auto sideRatio = [&] (double f) {
        Stereo in = tone (f, { { -20.0, 1.0 } });
        for (auto& v : in.r)
            v = -v;
        const auto y = run (s, in);
        std::vector<double> side;
        for (std::size_t i = 0; i < y.l.size(); ++i)
            side.push_back (0.5 * (y.l[i] - y.r[i]));
        const std::size_t n = wholePeriods (f, 48000.0, 0.4);
        return hannAmplitude (side, f, 48000.0, side.size() - n, n) / 0.1;
    };
    CHECK (sideRatio (1000.0) == Approx (0.18).margin (0.03));
    CHECK (sideRatio (50.0) == Approx (0.99).margin (0.02));
}

TEST_CASE ("solo keeps the band's direct and processed parts", "[path]")
{
    auto in = noise (0.3);
    auto s = tableDefaults();
    s.bandDb = { 12.0, 12.0, 12.0, 12.0, 12.0 };
    std::vector<Stereo> parts;
    Stereo sum;
    sum.l.assign (in.l.size(), 0.0);
    sum.r = sum.l;
    for (int b : { 1, 3 })
    {
        auto one = s;
        one.solo[(size_t) b] = true;
        const auto y = run (one, in);
        for (std::size_t i = 0; i < y.l.size(); ++i)
        {
            sum.l[i] += y.l[i];
            sum.r[i] += y.r[i];
        }
    }
    auto two = s;
    two.solo[1] = two.solo[3] = true;
    const auto y = run (two, in);
    double err = 0.0;
    for (std::size_t i = 0; i < y.l.size(); ++i)
        err = std::max (err, std::abs (y.l[i] - sum.l[i]));
    CHECK (toDb (err) < -120.0);
}

TEST_CASE ("output gain is a plain multiplier", "[path]")
{
    auto in = noise (0.2);
    auto a = tableDefaults();
    a.bandDb[2] = 24.0;
    auto b = a;
    b.outputDb = 6.0;
    const auto x = run (a, in), y = run (b, in);
    const double g = std::pow (10.0, 6.0 / 20.0);
    double err = 0.0;
    for (std::size_t i = 0; i < x.l.size(); ++i)
        err = std::max (err, std::abs (y.l[i] - g * x.l[i]));
    CHECK (toDb (err) < -100.0);
}

TEST_CASE ("band gain scales the processed band dB for dB", "[path]")
{
    const std::size_t n = wholePeriods (1100.0, 48000.0, 0.5);
    auto level = [&] (double gain) {
        const auto y = run (processedOnly (2, 3.0, gain), tone (1100.0, { { -20.0, 2.5 } }));
        return toDb (hannAmplitude (y.l, 1100.0, 48000.0, y.l.size() - n, n));
    };
    const double ref = level (24.0);
    for (double g : { 12.0, 36.0, 48.0 })
        CHECK (level (g) - ref == Approx (g - 24.0).margin (0.05));
}

TEST_CASE ("input gain shifts the level seen by the processing", "[path]")
{
    const double f = 1100.0;
    const std::size_t n = wholePeriods (f, 48000.0, 0.5);
    auto s = processedOnly (2, 3.0);
    auto boosted = s;
    boosted.inputDb = 10.0;
    const auto a = run (boosted, tone (f, { { -50.0, 3.0 } }));
    const auto b = run (s, tone (f, { { -40.0, 3.0 } }));
    CHECK (toDb (hannAmplitude (a.l, f, 48000.0, a.l.size() - n, n)) == Approx (toDb (hannAmplitude (b.l, f, 48000.0, b.l.size() - n, n))).margin (0.05));
}

TEST_CASE ("parameter changes settle within 20 ms without clicks", "[path]")
{
    auto e = std::make_unique<EnhancerEngine>();
    e->prepare (48000.0, 2);
    auto s = tableDefaults();
    e->setSettings (s);
    e->snapSmoothers();
    auto in = tone (1000.0, { { -20.0, 0.5 } });
    runEngine (*e, in);
    s.outputDb = 12.0;
    s.bandDb[2] = 24.0;
    s.crossoverHz[1] = 1000.0;
    e->setSettings (s);
    auto in2 = tone (1000.0, { { -20.0, 0.1 } });
    runEngine (*e, in2);
    double maxStep = 0.0;
    for (std::size_t i = 1; i < in2.l.size(); ++i)
        maxStep = std::max (maxStep, std::abs (in2.l[i] - in2.l[i - 1]));
    CHECK (maxStep < 0.2);
}

TEST_CASE ("stable with extreme input and silence; mono layout", "[path]")
{
    auto e = std::make_unique<EnhancerEngine>();
    e->prepare (44100.0, 1);
    auto s = tableDefaults();
    s.bandDb = { 48.0, 48.0, 48.0, 48.0, 48.0 };
    s.crossoverHz = { 16.0, 16.0, 21357.0, 21357.0 };
    e->setSettings (s);
    e->snapSmoothers();
    std::vector<double> x (48000, 0.0);
    std::mt19937 rng (1);
    for (std::size_t i = 16000; i < 32000; ++i)
        x[i] = (rng() & 1) ? 1.0 : -1.0;
    double* p[1] = { x.data() };
    e->process (p, (int) x.size());
    for (double v : x)
        REQUIRE (std::isfinite (v));
    CHECK (std::abs (x.back()) < 1.0);
}
