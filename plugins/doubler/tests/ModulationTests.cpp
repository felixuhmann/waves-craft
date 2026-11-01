// Spec sections 11.4 (modulation) and 11.5 (parameter dynamics).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
Settings modSettings (double detune, double depth, double rate)
{
    auto s = soloSettings();
    s.voices[0].panDeg = -45.0;
    s.voices[0].detuneCents = detune;
    s.voices[0].depthCents = depth;
    s.voices[0].rateHz = rate;
    return s;
}

// cents trajectory of voice 1 for `seconds`, sample by sample
std::vector<double> centsTrajectory (DoublerEngine& e, double seconds, int voice = 0)
{
    const int n = int (seconds * e.getSampleRate());
    std::vector<double> c (static_cast<size_t> (n));
    double l, r;
    for (int i = 0; i < n; ++i)
    {
        e.processSample (0.0, 0.0, l, r);
        c[size_t (i)] = e.getVoice (voice).getCents(); // cents used for sample i
    }
    return c;
}
} // namespace

TEST_CASE ("Triangle LFO: amplitude, period = 1 / Rate (decision D3), start direction", "[modulation]")
{
    const double fs = 48000.0;
    SECTION ("depth +100, rate 1.0, detune 0")
    {
        auto e = makeEngine (modSettings (0, 100, 1.0), fs);
        const auto c = centsTrajectory (e, 2.0);
        auto at = [&] (double t) { return c[size_t (std::lround (t * fs))]; };
        CHECK (at (0.001) > 0.0);
        CHECK (at (0.25) == Approx (100.0).margin (0.1));
        CHECK (at (0.5) == Approx (0.0).margin (0.1));
        CHECK (at (0.75) == Approx (-100.0).margin (0.1));
        CHECK (at (1.0) == Approx (0.0).margin (0.1));
        CHECK (at (1.25) == Approx (100.0).margin (0.1));
        CHECK (*std::max_element (c.begin(), c.end()) == Approx (100.0).margin (0.05));
        CHECK (*std::min_element (c.begin(), c.end()) == Approx (-100.0).margin (0.05));
    }
    SECTION ("depth -100 starts falling")
    {
        auto e = makeEngine (modSettings (0, -100, 1.0), fs);
        const auto c = centsTrajectory (e, 0.3);
        CHECK (c[100] < 0.0);
        CHECK (c[size_t (0.25 * fs)] == Approx (-100.0).margin (0.1));
    }
    SECTION ("detune +20, depth +100: between -80 and +120 cents")
    {
        auto e = makeEngine (modSettings (20, 100, 1.0), fs);
        const auto c = centsTrajectory (e, 1.5);
        CHECK (*std::max_element (c.begin(), c.end()) == Approx (120.0).margin (0.05));
        CHECK (*std::min_element (c.begin(), c.end()) == Approx (-80.0).margin (0.05));
        CHECK (c[0] == Approx (20.0));
    }
    SECTION ("the output pitch follows the triangle")
    {
        // average pitch over [0.2, 0.3] s of a triangle peaking at 0.25 s: 100 - 400 * 0.025 = 90 cents
        auto e = makeEngine (modSettings (0, 100, 1.0), fs);
        const auto x = sine (int (0.4 * fs), 1000.0, fs);
        const auto y = run (e, x, x).l;
        const double c = pitchCents (y, size_t (0.2 * fs + 0.02 * fs), size_t (0.3 * fs + 0.02 * fs), 1000.0, fs);
        CHECK (c == Approx (90.0).margin (3.0));
    }
}

TEST_CASE ("Modulation Reset restarts every LFO at phase 0", "[modulation]")
{
    const double fs = 44100.0;
    auto s = modSettings (10, 100, 3.7);
    s.voices[2] = s.voices[0];
    s.voices[2].depthCents = -50;
    s.voices[2].rateHz = 0.6;
    auto e = makeEngine (s, fs);
    centsTrajectory (e, 0.37);
    CHECK (e.getVoice (0).getCents() != Approx (10.0).margin (1.0));

    s.modReset = true;
    e.setSettings (s);
    CHECK (e.getVoice (0).getCents() != Approx (10.0).margin (1.0)); // value of the last processed sample
    const auto c0 = centsTrajectory (e, 0.01, 0);
    CHECK (c0[0] == Approx (10.0).margin (1e-9));
    CHECK (c0.back() > 10.0); // rises (positive depth)
    s.modReset = false;
    e.setSettings (s);
    const auto c2 = centsTrajectory (e, 0.01, 2);
    CHECK (c2.back() < 10.0); // voice 3: negative depth, reset too, falls
    // the button only acts on a rising edge: holding it does not keep resetting
    s.modReset = true;
    e.setSettings (s);
    centsTrajectory (e, 0.05);
    e.setSettings (s);
    const auto c3 = centsTrajectory (e, 0.001);
    CHECK (c3[0] > 10.0 + 100.0 * 4.0 * 3.7 * 0.04);
}

TEST_CASE ("Gain, pan, on/off and detune changes settle within 5 ms without steps", "[dynamics]")
{
    const double fs = 48000.0;
    const int five = int (0.005 * fs);

    // DC through voice 1 (static delay) shows the output coefficient directly
    auto measure = [&] (Settings a, Settings b, bool left) {
        auto e = makeEngine (a, fs);
        const int n = int (0.2 * fs);
        std::vector<double> l (size_t (n), 1.0), r (size_t (n), 1.0);
        const int t0 = int (0.1 * fs);
        e.process (l.data(), r.data(), t0);
        e.setSettings (b);
        e.process (l.data() + t0, r.data() + t0, n - t0);
        const auto& o = left ? l : r;
        const double before = o[size_t (t0 - 1)], after = o.back();
        double maxStep = 0.0;
        for (size_t i = size_t (t0) - 1; i < o.size() - 1; ++i)
            maxStep = std::max (maxStep, std::abs (o[i + 1] - o[i]));
        const double residual = std::abs (o[size_t (t0 + five)] - after) / std::abs (after - before);
        return std::make_pair (residual, maxStep / std::abs (after - before));
    };

    auto base = soloSettings();
    base.voices[0].panDeg = -45.0;
    SECTION ("gain")
    {
        auto b = base;
        b.voices[0].gainDb = -12.0;
        const auto [res, step] = measure (base, b, true);
        CHECK (res < 0.01);
        CHECK (step < 0.05);
    }
    SECTION ("pan")
    {
        auto b = base;
        b.voices[0].panDeg = 30.0;
        const auto [res, step] = measure (base, b, false);
        CHECK (res < 0.01);
        CHECK (step < 0.05);
    }
    SECTION ("on/off")
    {
        auto b = base;
        b.voices[0].on = false;
        const auto [res, step] = measure (base, b, true);
        CHECK (res < 0.01);
        CHECK (step < 0.05);
    }
    SECTION ("direct gain and master")
    {
        auto a = directSettings(), b = a;
        b.directGainDb = -20.0;
        b.masterDb = -6.0;
        const auto [res, step] = measure (a, b, true);
        CHECK (res < 0.01);
        CHECK (step < 0.05);
    }
    SECTION ("detune")
    {
        auto a = modSettings (0, 0, 1), b = modSettings (40, 0, 1);
        auto e = makeEngine (a, fs);
        centsTrajectory (e, 0.05);
        e.setSettings (b);
        const auto c = centsTrajectory (e, 0.01);
        CHECK (std::abs (c[size_t (five)] - 40.0) < 0.4);
        double maxStep = 0.0;
        for (size_t i = 0; i + 1 < c.size(); ++i)
            maxStep = std::max (maxStep, std::abs (c[i + 1] - c[i]));
        CHECK (maxStep < 2.0);
    }
}

TEST_CASE ("Switching detune to zero returns to the exact delay path", "[dynamics]")
{
    const double fs = 48000.0;
    auto s = modSettings (30, 0, 1);
    auto e = makeEngine (s, fs);
    const auto x = noise (int (0.6 * fs), 3);
    std::vector<double> l = x, r = x;
    const int t0 = int (0.3 * fs);
    e.process (l.data(), r.data(), t0);
    s.voices[0].detuneCents = 0.0;
    e.setSettings (s);
    e.process (l.data() + t0, r.data() + t0, int (x.size()) - t0);
    CHECK (e.getVoice (0).isStatic());
    const size_t d = size_t (0.02 * fs);
    for (size_t i = size_t (t0 + 0.02 * fs); i < x.size(); ++i)
        REQUIRE (l[i] == x[i - d]);
}
