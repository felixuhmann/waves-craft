// Spec r1 sections 3, 4, 6, 10 ([exact] parts) and the section 11 test vectors for delay, mix and filters.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>

#include "TestHelpers.h"

using namespace cdtest;
using Catch::Approx;

namespace
{
// Independent reference: third-order bilinear transform expanded as one polynomial, direct form I.
struct Poly3Filter
{
    std::array<long double, 4> b {}, a {};
    std::array<long double, 4> x {}, y {};

    Poly3Filter (bool highPass, double fc, double fs)
    {
        const long double A = 0.132316L;
        const long double k = std::tan ((long double) 3.14159265358979323846L * fc / fs);
        // numerator / denominator in s, coefficients of s^0..s^3
        const std::array<long double, 4> num = highPass ? std::array<long double, 4> { 0.0L, -A, 0.0L, 1.0L }
                                                        : std::array<long double, 4> { 1.0L, 0.0L, -A, 0.0L };
        const std::array<long double, 4> den { 1.0L, 2.0L + A, 2.0L + A, 1.0L };
        auto expand = [k] (const std::array<long double, 4>& p) {
            std::array<long double, 4> out {};
            for (int m = 0; m <= 3; ++m)
            {
                // (1 - z^-1)^m (1 + z^-1)^(3-m) k^(3-m)
                std::array<long double, 4> t { 1.0L, 0.0L, 0.0L, 0.0L };
                int deg = 0;
                for (int i = 0; i < 3; ++i)
                {
                    const long double sgn = i < m ? -1.0L : 1.0L;
                    for (int j = deg + 1; j >= 1; --j)
                        t[(size_t) j] += sgn * t[(size_t) j - 1];
                    ++deg;
                }
                const long double scale = p[(size_t) m] * std::pow (k, (long double) (3 - m));
                for (int j = 0; j < 4; ++j)
                    out[(size_t) j] += scale * t[(size_t) j];
            }
            return out;
        };
        b = expand (num);
        a = expand (den);
        const long double a0 = a[0];
        for (auto& v : b) v /= a0;
        for (auto& v : a) v /= a0;
    }

    double process (double in)
    {
        for (int i = 3; i > 0; --i)
        {
            x[(size_t) i] = x[(size_t) i - 1];
            y[(size_t) i] = y[(size_t) i - 1];
        }
        x[0] = in;
        long double acc = b[0] * x[0] + b[1] * x[1] + b[2] * x[2] + b[3] * x[3] - a[1] * y[1] - a[2] * y[2] - a[3] * y[3];
        y[0] = acc;
        return (double) acc;
    }
};

// Straight-line model of spec sections 3-6 with the compressor inactive.
Stereo referenceModel (const Settings& s, const std::vector<double>& inL, const std::vector<double>& inR, double fs)
{
    const size_t n = inL.size();
    Stereo out { std::vector<double> (n), std::vector<double> (n) };
    const double gi = std::pow (10.0, s.inputDb / 20.0), go = std::pow (10.0, s.outputDb / 20.0);
    const double dry = param::dryGain (s.dryWetPct), wet = param::wetGain (s.dryWetPct);
    for (int c = 0; c < 2; ++c)
    {
        const auto& in = c == 0 ? inL : inR;
        auto& y = c == 0 ? out.l : out.r;
        const int src = s.delayLink ? 0 : c;
        const int D = std::max (1, (int) std::lround (s.delayMs[(size_t) src] * fs / 1000.0));
        const double g = s.feedbackPct[(size_t) src] / 100.0;
        std::vector<double> w (n, 0.0);
        Poly3Filter hp (true, s.hpHz, fs), lp (false, s.lpHz, fs);
        const bool hpOn = s.hpHz > 16.0, lpOn = s.lpHz < 21357.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double xi = in[i] * gi;
            if (i >= (size_t) D)
                w[i] = in[i - (size_t) D] * gi + g * w[i - (size_t) D];
            double v = w[i];
            if (hpOn) v = hp.process (v);
            if (lpOn) v = lp.process (v);
            y[i] = (dry * xi + wet * v) * go;
        }
        const bool silent = (c == 1 && s.monitor == Monitor::Left) || (c == 0 && s.monitor == Monitor::Right);
        if (silent)
            std::fill (y.begin(), y.end(), 0.0);
    }
    return out;
}

double maxAbs (const std::vector<double>& x)
{
    double m = 0.0;
    for (double v : x) m = std::max (m, std::abs (v));
    return m;
}

double errorDbRePeak (const Stereo& a, const Stereo& b)
{
    double e = 0.0, p = 0.0;
    for (size_t i = 0; i < a.l.size(); ++i)
    {
        e = std::max ({ e, std::abs (a.l[i] - b.l[i]), std::abs (a.r[i] - b.r[i]) });
        p = std::max ({ p, std::abs (b.l[i]), std::abs (b.r[i]) });
    }
    return db (e / p);
}
} // namespace

TEST_CASE ("Test vector: impulse, Delay L 100 ms, Feedback 50 %, Dry/Wet 100 %", "[path][vector]")
{
    Settings s;
    s.hostSync = false;
    s.delayLink = false;
    s.delayMs = { 100.0, 100.0 };
    s.feedbackPct = { 50.0, 50.0 };
    s.dryWetPct = 100.0;
    const int n = 48000 / 2;
    const auto out = render (s, impulse (n, 0.001), std::vector<double> ((size_t) n, 0.0));
    CHECK (out.l[4800] == Approx (0.001).margin (1e-12));
    CHECK (out.l[9600] == Approx (0.0005).margin (1e-12));
    CHECK (out.l[14400] == Approx (0.00025).margin (1e-12));
    double others = 0.0;
    for (size_t i = 0; i < out.l.size(); ++i)
        if (i != 4800 && i != 9600 && i != 14400 && i != 19200)
            others = std::max (others, std::abs (out.l[i]));
    CHECK (others == 0.0);
    CHECK (maxAbs (out.r) == 0.0); // no cross-feed
}

TEST_CASE ("Test vector: same with Dry/Wet 25 %", "[path][vector]")
{
    Settings s;
    s.hostSync = false;
    s.delayMs = { 100.0, 100.0 };
    s.feedbackPct = { 50.0, 50.0 };
    s.dryWetPct = 25.0;
    const int n = 6000;
    const auto out = render (s, impulse (n, 0.001), std::vector<double> ((size_t) n, 0.0));
    CHECK (out.l[0] / 0.001 == Approx (0.74989).margin (5e-6));
    CHECK (out.l[4800] / 0.001 == Approx (0.24547).margin (5e-6));
}

TEST_CASE ("Delay is integer-sample, minimum 1 sample, zero latency on the dry path", "[path]")
{
    Settings s = plainWet();
    s.dryWetPct = 50.0;
    const auto out = render (s, impulse (100, 0.01), std::vector<double> (100, 0.0));
    CHECK (out.l[0] == Approx (0.01 * param::dryGain (50.0)));
    CHECK (out.l[1] == Approx (0.01 * param::wetGain (50.0)));
    for (size_t i = 2; i < 100; ++i)
        CHECK (out.l[i] == 0.0);
}

TEST_CASE ("Feedback 100 % repeats without loss", "[path]")
{
    Settings s;
    s.hostSync = false;
    s.delayMs = { 20.0, 20.0 };
    s.feedbackPct = { 100.0, 100.0 };
    const int n = 48000 * 2;
    const auto out = render (s, impulse (n, 0.001), std::vector<double> ((size_t) n, 0.0));
    for (int k = 1; k < 100; ++k)
        REQUIRE (out.l[(size_t) (960 * k)] == Approx (0.001).margin (1e-15));
}

TEST_CASE ("Exact parts match the straight-line model of sections 3-6 to <= -90 dB re peak", "[path][exact]")
{
    uint32_t seed = 777u;
    auto rnd = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return (double) (seed >> 8) / 16777216.0;
    };
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int trial = 0; trial < 6; ++trial)
        {
            Settings s;
            s.hostSync = false;
            s.delayLink = trial % 3 == 0;
            s.delayMs = { param::DelayMs::fromNorm (0.02 * rnd()), param::DelayMs::fromNorm (0.02 * rnd()) };
            s.feedbackPct = { param::Percent::fromNorm (0.9 * rnd()), param::Percent::fromNorm (0.9 * rnd()) };
            s.dryWetPct = param::Percent::fromNorm (rnd());
            s.inputDb = param::GainDb::fromNorm (0.5 * rnd());
            s.outputDb = param::GainDb::fromNorm (rnd());
            s.hpHz = trial % 2 == 0 ? param::FilterHz::fromNorm (0.6 * rnd()) : 16.0;
            s.lpHz = trial % 4 < 2 ? param::FilterHz::fromNorm (0.4 + 0.6 * rnd()) : 21357.0;
            s.monitor = (Monitor) (trial % 3);
            const int n = (int) (fs * 0.5);
            const auto inL = noise (n, 0.001, 11u + (uint32_t) trial);
            const auto inR = noise (n, 0.001, 999u + (uint32_t) trial);
            const auto out = render (s, inL, inR, fs);
            const auto ref = referenceModel (s, inL, inR, fs);
            INFO ("fs " << fs << " trial " << trial);
            CHECK (errorDbRePeak (out, ref) <= -90.0);
        }
}

TEST_CASE ("Test vectors: HP / LP at 1 kHz on a quiet signal", "[path][filters][vector]")
{
    struct Pt { double f, dbExpected; };
    for (bool high : { true, false })
    {
        Settings s = plainWet();
        if (high) s.hpHz = 1000.0; else s.lpHz = 1000.0;
        const int n = 1 << 15;
        const auto out = render (s, impulse (n, 0.01), std::vector<double> ((size_t) n, 0.0));
        const std::vector<Pt> pts = high ? std::vector<Pt> { { 1000.0, -3.01 }, { 500.0, -14.82 }, { 200.0, -29.32 }, { 100.0, -36.96 } }
                                         : std::vector<Pt> { { 1000.0, -3.01 }, { 2000.0, -14.88 }, { 4000.0, -26.64 }, { 10000.0, -38.48 } };
        for (const auto& p : pts)
        {
            const double g = db (std::abs (dtft (out.l, p.f)) / 0.01);
            INFO ((high ? "HP " : "LP ") << p.f << " Hz: " << g);
            CHECK (g == Approx (p.dbExpected).margin (0.02));
        }
    }
}

TEST_CASE ("HP / LP: -3.01 dB at fc at every rate, LP + HP is an all-pass, matches the prototype", "[path][filters]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double fc : { 40.0, 150.0, 1000.0, 6000.0, 15000.0 })
        {
            CrossoverFilter hp, lp;
            hp.design (true, fc, fs);
            lp.design (false, fc, fs);
            const double wc = 2.0 * craft::cdl::pi * fc / fs;
            CHECK (db (std::abs (hp.response (wc))) == Approx (-3.0103).margin (0.001));
            CHECK (db (std::abs (lp.response (wc))) == Approx (-3.0103).margin (0.001));
            for (double f : { 20.0, 100.0, 700.0, 3000.0, 12000.0, 19000.0 })
            {
                const double w = 2.0 * craft::cdl::pi * f / fs;
                CHECK (std::abs (hp.response (w) + lp.response (w)) == Approx (1.0).margin (1e-9));
                CHECK (std::abs (hp.response (w) - crossoverPrototype (true, f, fc, fs)) < 1e-9);
                CHECK (std::abs (lp.response (w) - crossoverPrototype (false, f, fc, fs)) < 1e-9);
            }
        }
}

TEST_CASE ("Test vector: Input +6 dB on a -40 dBFS sweep gives +5.99 dB", "[path][vector]")
{
    Settings s = plainWet();
    s.inputDb = 6.0;
    const double fs = 48000.0;
    const int n = 48000 * 2;
    // logarithmic sweep 50 Hz .. 15 kHz at -40 dBFS peak
    std::vector<double> x ((size_t) n);
    const double f1 = 50.0, f2 = 15000.0, T = n / fs, Lr = T / std::log (f2 / f1);
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = 0.01 * std::sin (twoPi * f1 * Lr * (std::exp ((i / fs) / Lr) - 1.0));
    const auto out = render (s, x, std::vector<double> ((size_t) n, 0.0));
    const double g = db (rms (out.l, 4800, (size_t) n) / rms (x, 4800 - 1, (size_t) n - 1));
    CHECK (g == Approx (5.99).margin (0.03));
}

TEST_CASE ("Monitor: L silences the right output, R silences the left output", "[path]")
{
    for (int m = 0; m < 3; ++m)
    {
        Settings s = plainWet();
        s.dryWetPct = 50.0;
        s.monitor = (Monitor) m;
        const auto x = noise (4800, 0.01);
        const auto out = render (s, x, x);
        CHECK ((maxAbs (out.l) == 0.0) == (m == 2));
        CHECK ((maxAbs (out.r) == 0.0) == (m == 0));
    }
}

TEST_CASE ("Colour effects are silent at Dry/Wet 0 % and contribute nothing when off", "[path]")
{
    Settings s;
    s.hostSync = false;
    s.dryWetPct = 0.0;
    s.verbOn = s.distOn = s.doublerOn = s.phaserOn = true;
    s.distSend = 100.0;
    const auto x = noise (24000, 0.5);
    const auto out = render (s, x, x);
    double e = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
        e = std::max (e, std::abs (out.l[i] - x[i]));
    CHECK (e == 0.0);

    // all effects off: bit-identical to the plain delay path (same settings otherwise)
    Settings a;
    a.hostSync = false;
    a.dryWetPct = 60.0;
    Settings b = a;
    b.verbLevel = b.distLevel = b.doublerLevel = b.phaserLevel = 100.0; // levels irrelevant when off
    const auto y = noise (24000, 0.3, 4u);
    const auto oa = render (a, y, y), ob = render (b, y, y);
    CHECK (oa.l == ob.l);
    CHECK (oa.r == ob.r);
}

TEST_CASE ("Delay-time changes are click-free", "[path]")
{
    Settings s;
    s.hostSync = false;
    s.delayMs = { 200.0, 200.0 };
    s.feedbackPct = { 40.0, 40.0 };
    ColourDelayEngine e;
    e.setSettings (s);
    e.prepare (48000.0);
    e.reset();
    const int n = 48000;
    auto l = sine (440.0, 0.05, n), r = l;
    // change the delay mid-way
    e.process (l.data(), r.data(), n / 2);
    s.delayMs = { 352.0, 352.0 };
    e.setSettings (s);
    e.process (l.data() + n / 2, r.data() + n / 2, n / 2);
    double maxStep = 0.0;
    for (int i = n / 2 - 100; i < n - 1; ++i)
        maxStep = std::max (maxStep, std::abs (l[(size_t) i + 1] - l[(size_t) i]));
    // a 440 Hz sine of amplitude A has a maximum step of 2 pi 440 / 48000 A; allow for the comb sum (|w| <= A / (1 - 0.4))
    CHECK (maxStep < 2.0 * twoPi * 440.0 / 48000.0 * 0.05 / 0.6);
    CHECK (e.getDelaySamples (0) == 352 * 48);
}

TEST_CASE ("Runaway protection: Feedback 100 % with sustained full-scale input stays bounded", "[path]")
{
    Settings s;
    s.hostSync = false;
    s.delayMs = { 0.0, 4.0 };
    s.feedbackPct = { 100.0, 100.0 };
    s.delayLink = false;
    s.inputDb = 18.0;
    s.outputDb = 18.0;
    s.dryWetPct = 50.0;
    s.verbOn = s.distOn = s.doublerOn = s.phaserOn = true;
    s.verbLevel = s.distLevel = s.doublerLevel = s.phaserLevel = 100.0;
    const int n = 48000 * 5;
    std::vector<double> x ((size_t) n);
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = ((i / 37) % 2) ? 1.0 : -1.0; // full-scale square
    const auto out = render (s, x, x);
    double peak = 0.0;
    bool finite = true;
    for (size_t i = 0; i < out.l.size(); ++i)
    {
        finite = finite && std::isfinite (out.l[i]) && std::isfinite (out.r[i]);
        peak = std::max ({ peak, std::abs (out.l[i]), std::abs (out.r[i]) });
    }
    CHECK (finite);
    CHECK (peak <= 32.0); // +30 dBFS
}

TEST_CASE ("Host bypass crossfades to the unprocessed input", "[path]")
{
    Settings s;
    s.hostSync = false;
    s.bypass = true;
    s.verbOn = true;
    const auto x = noise (9600, 0.2);
    const auto out = render (s, x, x);
    CHECK (out.l == x);
}
