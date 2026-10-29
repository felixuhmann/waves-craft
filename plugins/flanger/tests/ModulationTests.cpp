// Spec r1 sections 4, 5, 8, 9: delay trajectories, LFO, stereo offset, tempo sync, model accuracy.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;

namespace
{
constexpr double fs = 48000.0;

Settings trajBase()
{
    Settings s;
    s.mixPct = 100;
    s.feedbackPct = 0;
    s.delayMs = 2.2;
    s.rateHz = 1.0;
    s.depthPct = 100;
    return s;
}

// Run sample by sample and record d(n) per channel.
std::pair<std::vector<double>, std::vector<double>> trajectory (const Settings& s, int n, double rate = fs,
                                                                 Variant v = Variant::Stereo)
{
    auto e = makeEngine (s, rate, v);
    std::vector<double> dl, dr;
    double a = 0.0, b = 0.0;
    for (int i = 0; i < n; ++i)
    {
        e.process (&a, &b, 1);
        dl.push_back (e.getDelaySamples (0));
        dr.push_back (e.getDelaySamples (1));
    }
    return { dl, dr };
}

template <typename F>
void requireTrajectory (const std::vector<double>& d, F expected, double tol = 0.1)
{
    double worst = 0.0;
    for (size_t i = 0; i < d.size(); ++i)
        worst = std::max (worst, std::abs (d[i] - expected (double (i) / fs)));
    INFO ("worst trajectory error " << worst << " samples");
    REQUIRE (worst <= tol);
}
} // namespace

TEST_CASE ("10: sine trajectory, Rate 1, Depth 100", "[lfo]")
{
    const auto [dl, dr] = trajectory (trajBase(), 96000);
    requireTrajectory (dl, [] (double t) { return 105.0 + 99.75 * std::sin (2 * pi * t); }, 1e-6);
    CHECK_THAT (dl[36000], WithinAbs (5.25, 1e-6));   // t = 0.75
    CHECK_THAT (dl[12000], WithinAbs (204.75, 1e-6)); // t = 0.25
    CHECK (dl[0] == 105.0);                           // theta = 0 at the first sample
    requireTrajectory (dr, [] (double t) { return 105.0 + 99.75 * std::sin (2 * pi * t); }, 1e-6); // Stereo 0
}

TEST_CASE ("10: sine trajectory, Depth 50", "[lfo]")
{
    auto s = trajBase();
    s.depthPct = 50;
    requireTrajectory (trajectory (s, 96000).first, [] (double t) { return 105.0 + 49.875 * std::sin (2 * pi * t); }, 1e-6);
}

TEST_CASE ("10: triangle trajectory (linear delay sweep)", "[lfo]")
{
    auto s = trajBase();
    s.triangle = true;
    const auto d = trajectory (s, 96000).first;
    requireTrajectory (d, [] (double t) { return 105.0 + 99.75 * ReferenceModel::tri (2 * pi * t); }, 1e-6);
    CHECK_THAT (d[12000], WithinAbs (204.75, 1e-6));
    CHECK_THAT (d[24000], WithinAbs (105.0, 1e-6));
    CHECK_THAT (d[6000], WithinAbs (105.0 + 99.75 * 0.5, 1e-6)); // linear in between
}

TEST_CASE ("10: Stereo 90 -> right LFO leads by 90 degrees", "[lfo][stereo]")
{
    auto s = trajBase();
    s.stereoDeg = 90;
    const auto [dl, dr] = trajectory (s, 96000);
    requireTrajectory (dl, [] (double t) { return 105.0 + 99.75 * std::sin (2 * pi * t); }, 1e-6);
    requireTrajectory (dr, [] (double t) { return 105.0 + 99.75 * std::sin (2 * pi * t + pi / 2); }, 1e-6);
}

TEST_CASE ("5.3: stereo offset quantised to 1024 steps", "[lfo][stereo]")
{
    CHECK_THAT (stereoOffsetCycles (30) * 360.0, WithinAbs (29.8828125, 1e-12));
    CHECK (stereoOffsetCycles (45) * 360.0 == 45.0);
    CHECK (stereoOffsetCycles (90) * 360.0 == 90.0);
    CHECK (stereoOffsetCycles (180) * 360.0 == 180.0);
    CHECK (stereoOffsetCycles (1) == 2.0 / 1024.0);
    CHECK (stereoOffsetCycles (0) == 0.0);

    auto s = trajBase();
    s.stereoDeg = 30;
    const auto dr = trajectory (s, 48000).second;
    const double phi = 85.0 * 2.0 * pi / 1024.0;
    requireTrajectory (dr, [phi] (double t) { return 105.0 + 99.75 * std::sin (2 * pi * t + phi); }, 1e-6);
}

TEST_CASE ("10: tape trajectory (through zero), dry at c", "[lfo][tape]")
{
    auto s = trajBase();
    s.tape = true;
    const auto d = trajectory (s, 96000).first;
    requireTrajectory (d, [] (double t) { return 105.0 + 104.0 * std::sin (2 * pi * t); }, 1e-6);
    CHECK_THAT (d[36000], WithinAbs (1.0, 1e-6)); // sweeps down to 1 sample
}

TEST_CASE ("10: Delay 10, Depth 30, Rate 1", "[lfo]")
{
    auto s = trajBase();
    s.delayMs = 10;
    s.depthPct = 30;
    requireTrajectory (trajectory (s, 96000).first, [] (double t) { return 480.0 + 136.8 * std::sin (2 * pi * t); }, 1e-6);
}

TEST_CASE ("4: base delay is an integer number of samples at every sample rate", "[lfo][rates]")
{
    CHECK (model::baseDelaySamples (2.2, 48000) == 105);
    CHECK (model::baseDelaySamples (2.2, 44100) == 97);
    CHECK (model::baseDelaySamples (2.2, 96000) == 211);
    CHECK (model::baseDelaySamples (2.2, 192000) == 422);
    CHECK (model::baseDelaySamples (0.1, 48000) == 4);
    CHECK (model::baseDelaySamples (50, 48000) == 2400);
    CHECK (model::baseDelaySamples (50, 192000) == 9600);
    CHECK (model::tapeCentre (50, 105) == 52);
    CHECK (model::tapeCentre (29, 100) == 29); // integer arithmetic, no 28.999.. truncation
    CHECK (model::tapeCentre (0, 105) == 0);

    // 50 ms at Depth 100 reaches 97.5 ms (no upper limit), here at 192 kHz
    auto s = trajBase();
    s.delayMs = 50;
    s.rateHz = 0.25;
    const auto d = trajectory (s, 192000, 192000.0).first;
    CHECK_THAT (*std::max_element (d.begin(), d.end()), WithinAbs (1.95 * 9600, 1e-3));
}

TEST_CASE ("9: Tempo Sync - Rate is cycles per beat", "[lfo][sync]")
{
    auto s = trajBase();
    s.tempoSync = true;
    s.bpm = 120;
    s.rateHz = 1.0; // 1 cycle per beat at 120 BPM = 2 Hz
    requireTrajectory (trajectory (s, 48000).first, [] (double t) { return 105.0 + 99.75 * std::sin (2 * pi * 2.0 * t); }, 1e-6);
    CHECK (model::lfoHz (s) == 2.0);
    s.bpm = 90;
    s.rateHz = 0.5;
    CHECK (model::lfoHz (s) == 0.75);
}

TEST_CASE ("5.2: Rate below 0.01 Hz is static; Stop freezes and resumes the phase", "[lfo]")
{
    auto s = trajBase();
    s.rateHz = 0.005;
    auto d = trajectory (s, 4800).first;
    CHECK (*std::min_element (d.begin(), d.end()) == 105.0);
    CHECK (*std::max_element (d.begin(), d.end()) == 105.0);

    // Stop at start: LFO holds theta = phi_ch
    s.rateHz = 1.0;
    s.stop = true;
    s.stereoDeg = 90;
    const auto [l, r] = trajectory (s, 4800);
    CHECK (l.front() == 105.0);
    CHECK (l.back() == 105.0);
    CHECK_THAT (r.back(), WithinAbs (105.0 + 99.75, 1e-9));

    // Stop while running: phase freezes (after the <= 20 ms ramp) and continues from there on release
    s.stop = false;
    s.stereoDeg = 0;
    auto e = makeEngine (s, fs);
    double a = 0, b = 0;
    for (int i = 0; i < 4800; ++i)
        e.process (&a, &b, 1);
    const double before = e.getPhase();
    CHECK_THAT (before, WithinAbs (0.1, 1e-9));
    s.stop = true;
    e.setSettings (s);
    for (int i = 0; i < 1440; ++i) // 30 ms
        e.process (&a, &b, 1);
    const double frozen = e.getPhase();
    CHECK (frozen > before);
    CHECK (frozen < before + 0.02);
    for (int i = 0; i < 48000; ++i)
        e.process (&a, &b, 1);
    CHECK (e.getPhase() == frozen);
    s.stop = false;
    e.setSettings (s);
    for (int i = 0; i < 4800; ++i)
        e.process (&a, &b, 1);
    CHECK (e.getPhase() > frozen + 0.09);
}

TEST_CASE ("9: restartLfo sets theta back to 0 (start of playback)", "[lfo]")
{
    auto e = makeEngine (trajBase(), fs);
    std::vector<double> l (10000, 0.0), r (10000, 0.0);
    e.process (l.data(), r.data(), 10000);
    CHECK (e.getPhase() > 0.2);
    e.restartLfo();
    double a = 0, b = 0;
    e.process (&a, &b, 1);
    CHECK (e.getDelaySamples (0) == 105.0);
}

// ---------------------------------------------------------------------------------------------
// Section 8: output versus the section 3-6 model on noise <= -60 dB re peak over 2 s from start.
TEST_CASE ("8: modulated settings match the independent model on noise", "[model]")
{
    struct Case
    {
        const char* name;
        double rate;
        Settings s;
    };
    std::vector<Case> cases;
    auto add = [&] (const char* name, double rate, auto tweak) {
        Settings s;
        tweak (s);
        cases.push_back ({ name, rate, s });
    };
    add ("default", 48000, [] (Settings&) {});
    add ("fast sine, high feedback", 44100, [] (Settings& s) { s.rateHz = 3.7; s.feedbackPct = 95; s.delayMs = 1.3; });
    add ("triangle, stereo 120, fb-", 96000, [] (Settings& s) { s.triangle = true; s.stereoDeg = 120; s.feedbackInvert = true; s.rateHz = 0.8; });
    add ("tape, wet-, mix 70", 48000, [] (Settings& s) { s.tape = true; s.wetInvert = true; s.mixPct = 70; s.rateHz = 1.5; s.depthPct = 80; });
    add ("LP filter, delay 12", 192000, [] (Settings& s) { s.filterOn = true; s.filterHighPass = false; s.filterHz = 3000; s.delayMs = 12; s.rateHz = 0.4; });
    add ("HP filter, gain +6, fb 100", 48000, [] (Settings& s) { s.filterOn = true; s.filterHz = 12000; s.gainDb = 6; s.feedbackPct = 100; s.rateHz = 7.5; });
    add ("tiny delay, depth 100", 44100, [] (Settings& s) { s.delayMs = 0.3; s.rateHz = 2; });
    add ("tempo sync 0.25/beat @ 140", 48000, [] (Settings& s) { s.tempoSync = true; s.bpm = 140; s.rateHz = 0.25; s.stereoDeg = 45; });

    for (const auto& c : cases)
    {
        const int n = int (2.0 * c.rate);
        const auto x1 = noise (n, 11), x2 = noise (n, 23);
        const auto out = render (c.s, c.rate, x1, x2);
        const auto refL = ReferenceModel::channel (c.s, c.rate, x1, 0.0);
        const auto refR = ReferenceModel::channel (c.s, c.rate, x2, ReferenceModel::rightOffset (c.s));
        INFO (c.name << " @ " << c.rate);
        const double eL = errorDb (out.l, refL), eR = errorDb (out.r, refR);
        INFO ("error L " << eL << " dB, R " << eR << " dB");
        CHECK (eL <= -60.0);
        CHECK (eR <= -60.0);
        CHECK (eL <= -120.0); // in practice the engine is the model up to rounding
        CHECK (eR <= -120.0);
    }
}

TEST_CASE ("8: static settings match the model to <= -100 dB at 44.1 ... 192 kHz", "[model][rates]")
{
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        Settings s;
        s.depthPct = 0;
        s.feedbackPct = 80;
        s.delayMs = 3.3;
        s.filterOn = true;
        s.filterHz = 2500;
        const auto x = impulse (int (rate / 4));
        const auto y = render (s, rate, x, x);
        const auto ref = ReferenceModel::channel (s, rate, x, 0.0);
        INFO ("fs " << rate);
        CHECK (errorDb (y.l, ref) <= -100.0);

        s.tape = true;
        s.depthPct = 37;
        s.rateHz = 0;
        const auto yt = render (s, rate, x, x);
        CHECK (errorDb (yt.l, ReferenceModel::channel (s, rate, x, 0.0)) <= -100.0);
    }
}

TEST_CASE ("1: linear - no level dependence over 20 dB and more", "[model]")
{
    Settings s;
    s.feedbackPct = 90;
    s.rateHz = 2.0;
    const auto x = noise (48000, 5);
    auto x10 = x;
    for (auto& v : x10)
        v *= 10.0;
    const auto a = render (s, fs, x, x);
    const auto b = render (s, fs, x10, x10);
    std::vector<double> scaled (b.l.size());
    for (size_t i = 0; i < scaled.size(); ++i)
        scaled[i] = b.l[i] / 10.0;
    CHECK (errorDb (scaled, a.l) <= -200.0);
}
