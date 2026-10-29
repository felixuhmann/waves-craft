// Spec r1 sections 1, 2, 5.5, 7: parameters, display formats, link, smoothing, variants, bypass.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "dsp/Format.h"
#include "dsp/Link.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;
namespace pm = craft::flanger::dsp::param;

namespace
{
constexpr double fs = 48000.0;

std::vector<double> sine (int n, double hz, double amp = 0.5)
{
    std::vector<double> v (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i)
        v[size_t (i)] = amp * std::sin (2.0 * pi * hz * i / fs);
    return v;
}

double maxStep (const std::vector<double>& v, size_t from = 1)
{
    double m = 0.0;
    for (size_t i = std::max<size_t> (from, 1); i < v.size(); ++i)
        m = std::max (m, std::abs (v[i] - v[i - 1]));
    return m;
}
} // namespace

TEST_CASE ("2: parameter mappings, ranges and defaults", "[params]")
{
    CHECK (pm::Delay::fromNorm (0.0) == 0.1);
    CHECK (pm::Delay::fromNorm (1.0) == 50.0);
    CHECK (pm::snap<pm::Delay> (2.2) == 2.2);
    CHECK (pm::Delay::fromNorm (0.5) == std::round (0.1 * std::sqrt (500.0) * 10.0) / 10.0); // 0.1 ms resolution

    CHECK (pm::Depth::fromNorm (0.555) == 56.0);
    CHECK (pm::Mix::defaultValue == 50.0);
    CHECK (pm::Feedback::defaultValue == 50.0);
    CHECK (pm::Depth::defaultValue == 100.0);

    CHECK (pm::Rate::fromNorm (0.0) == 0.0); // 0.005 Hz < 0.01 -> static
    CHECK (pm::Rate::fromNorm (1.0) == 20.0);
    CHECK (pm::Rate::fromNorm (pm::Rate::toNorm (0.01)) == 0.01);
    CHECK (pm::snap<pm::Rate> (0.2) == 0.2);
    for (double r = 0.0; r <= 1.0; r += 0.01)
    {
        const double v = pm::Rate::fromNorm (r);
        CHECK (std::abs (v * 200.0 - std::round (v * 200.0)) < 1e-9); // 0.005 Hz grid
        CHECK ((v == 0.0 || v >= 0.01));
    }

    CHECK (pm::Gain::fromNorm (0.0) == -12.0);
    CHECK (pm::Gain::fromNorm (0.5) == 0.0);
    CHECK (pm::Gain::fromNorm (1.0) == 12.0);
    CHECK (pm::Stereo::fromNorm (0.5) == 90.0);

    CHECK (pm::FilterFreq::fromNorm (0.0) == 100.0);
    CHECK (pm::FilterFreq::fromNorm (1.0) == 20000.0);
    CHECK (pm::snap<pm::FilterFreq> (1000.0) == 1000.0);
    CHECK (pm::snap<pm::FilterFreq> (12345.0) == 12000.0);
    CHECK (pm::snap<pm::FilterFreq> (1234.0) == 1200.0);

    // monotonic over r
    double prev[5] = { -1e9, -1e9, -1e9, -1e9, -1e9 };
    for (int i = 0; i <= 1000; ++i)
    {
        const double r = i / 1000.0;
        const double v[5] = { pm::Delay::fromNorm (r), pm::Rate::fromNorm (r), pm::FilterFreq::fromNorm (r),
                              pm::Gain::fromNorm (r), pm::Stereo::fromNorm (r) };
        for (int k = 0; k < 5; ++k)
        {
            CHECK (v[k] >= prev[k]);
            prev[k] = v[k];
        }
    }
}

TEST_CASE ("2: display formats", "[params][format]")
{
    using craft::flanger::dsp::formatNumber;
    using Q = craft::flanger::dsp::Quantity;
    CHECK (formatNumber (Q::Delay, 2.2) == "2.2");
    CHECK (formatNumber (Q::Percent, 50) == "50");
    CHECK (formatNumber (Q::Rate, 0.2) == "0.20");
    CHECK (formatNumber (Q::Rate, 9.995) == "10.0");
    CHECK (formatNumber (Q::Rate, 12.5) == "12.5");
    CHECK (formatNumber (Q::Gain, 0.0) == "0.0");
    CHECK (formatNumber (Q::Gain, -0.04) == "0.0");
    CHECK (formatNumber (Q::Stereo, 90) == "90");
    CHECK (formatNumber (Q::FilterFreq, 1000) == "1.0k");
    CHECK (formatNumber (Q::FilterFreq, 9900) == "9.9k");
    CHECK (formatNumber (Q::FilterFreq, 12000) == "12k");
    CHECK (formatNumber (Q::FilterFreq, 100) == "0.1k");
}

TEST_CASE ("2: parameter display strings include the units", "[params][format]")
{
    using craft::flanger::dsp::formatDisplay;
    using Q = craft::flanger::dsp::Quantity;
    CHECK (formatDisplay (Q::Delay, 2.2) == "2.2 ms");
    CHECK (formatDisplay (Q::Delay, 50.0) == "50.0 ms");
    CHECK (formatDisplay (Q::Percent, 50) == "50 %");   // Depth, Mix, Feedback
    CHECK (formatDisplay (Q::Percent, 100) == "100 %");
    CHECK (formatDisplay (Q::Rate, 0.2) == "0.20 Hz");
    CHECK (formatDisplay (Q::Rate, 12.5) == "12.5 Hz");
    CHECK (formatDisplay (Q::Rate, 1.0, true) == "1.00 x/Beat");
    CHECK (formatDisplay (Q::Gain, 0.0) == "0.0 dB");
    CHECK (formatDisplay (Q::Gain, -12.0) == "-12.0 dB");
    CHECK (formatDisplay (Q::Gain, 4.5) == "4.5 dB");
    CHECK (formatDisplay (Q::Stereo, 90) == "90");
    CHECK (formatDisplay (Q::FilterFreq, 1000) == "1.0k");
    CHECK (formatDisplay (Q::FilterFreq, 12000) == "12k");

    // typed input is accepted with or without units
    using craft::flanger::dsp::parseNumber;
    CHECK (parseNumber ("2.2") == 2.2);
    CHECK (parseNumber ("2.2 ms") == 2.2);
    CHECK (parseNumber ("50 %") == 50.0);
    CHECK (parseNumber ("50%") == 50.0);
    CHECK (parseNumber ("-3.5 dB") == -3.5);
    CHECK (parseNumber ("+6dB") == 6.0);
    CHECK (parseNumber ("0.20 Hz") == 0.2);
    CHECK (parseNumber ("1.00 x/Beat") == 1.0);
    CHECK (parseNumber ("1.5k") == 1500.0);
    CHECK (parseNumber ("12 kHz") == 12000.0);
    CHECK (parseNumber ("800 Hz") == 800.0);
    CHECK (parseNumber ("abc") == 0.0);
    // the display string round-trips through the parser
    CHECK (pm::snap<pm::Delay> (parseNumber (formatDisplay (Q::Delay, 7.3))) == 7.3);
    CHECK (pm::snap<pm::Gain> (parseNumber (formatDisplay (Q::Gain, -4.5))) == -4.5);
    CHECK (pm::snap<pm::FilterFreq> (parseNumber (formatDisplay (Q::FilterFreq, 12000))) == 12000.0);

    // every reachable value carries its unit
    for (int i = 0; i <= 100; ++i)
    {
        const double r = i / 100.0;
        CHECK (formatDisplay (Q::Delay, pm::Delay::fromNorm (r)).find (" ms") != std::string::npos);
        CHECK (formatDisplay (Q::Percent, pm::Mix::fromNorm (r)).find (" %") != std::string::npos);
        CHECK (formatDisplay (Q::Gain, pm::Gain::fromNorm (r)).find (" dB") != std::string::npos);
    }
}

TEST_CASE ("5.5: Link keeps Rate x Depth constant", "[link]")
{
    craft::flanger::dsp::RateDepthLink link;
    link.capture (1.0, 50.0);
    CHECK (link.depthForRate (2.0) == 25.0);
    CHECK (link.depthForRate (0.5) == 100.0);
    CHECK (link.depthForRate (0.25) == 100.0); // clamped
    CHECK (link.rateForDepth (10.0) == 5.0);
    CHECK (link.rateForDepth (0.0) == 20.0);   // clamped
    CHECK (link.depthForRate (0.0) == 100.0);
    link.capture (0.0, 50.0);
    CHECK (link.depthForRate (2.0) < 0.0);     // nothing to hold
}

TEST_CASE ("7: settled smoothers reproduce the static response exactly", "[smoothing]")
{
    Settings a;
    a.depthPct = 0;
    Settings b = a;
    b.delayMs = 7.3;
    b.mixPct = 80;
    b.feedbackPct = 65;
    b.gainDb = -3.4;
    b.filterOn = true;
    b.filterHz = 4500;
    b.wetInvert = true;
    b.feedbackInvert = true;
    b.tape = true;
    b.depthPct = 40;
    b.rateHz = 0;

    auto e = makeEngine (a, fs);
    auto x = noise (24000, 3);
    auto y = x;
    e.process (y.data(), x.data(), 24000);
    e.setSettings (b);
    std::vector<double> z (96000, 0.0), z2 (96000, 0.0); // settle and let the delay lines run empty
    e.process (z.data(), z2.data(), 96000);
    e.restartLfo(); // Rate 0 holds the current phase; compare from theta = 0

    const auto imp = impulse (4096);
    auto l = imp, r = imp;
    e.process (l.data(), r.data(), 4096);
    const auto fresh = render (b, fs, imp, imp);
    CHECK (maxAbsDiff (l, fresh.l) <= 1e-12);
}

TEST_CASE ("7: delay changes glide (no jump of the read position)", "[smoothing]")
{
    Settings s;
    s.mixPct = 100;
    s.feedbackPct = 0;
    s.depthPct = 0;
    auto e = makeEngine (s, fs);
    auto x = sine (48000, 100.0);
    auto x2 = x;
    e.process (x.data(), x2.data(), 12000);
    s.delayMs = 20.0; // 105 -> 960 samples
    e.setSettings (s);
    double prevD = e.getDelaySamples (0), worstDStep = 0.0;
    for (int i = 12000; i < 48000; ++i)
    {
        e.process (x.data() + i, x2.data() + i, 1);
        worstDStep = std::max (worstDStep, std::abs (e.getDelaySamples (0) - prevD));
        prevD = e.getDelaySamples (0);
    }
    CHECK (prevD == 960.0);
    CHECK (worstDStep < 1.0);               // a glide, not a jump of 855 samples
    CHECK (maxStep (x, 200) < 0.03);        // continuous output (a jump would be up to 1.0)
}

TEST_CASE ("7: toggles crossfade within 20 ms", "[smoothing]")
{
    struct Toggle
    {
        const char* name;
        void (*flip) (Settings&);
    };
    const Toggle toggles[] = {
        { "wet polarity", [] (Settings& s) { s.wetInvert = true; } },
        { "feedback polarity", [] (Settings& s) { s.feedbackInvert = true; } },
        { "tape", [] (Settings& s) { s.tape = true; } },
        { "filter on", [] (Settings& s) { s.filterOn = true; s.filterHz = 300; } },
        { "waveform", [] (Settings& s) { s.triangle = true; } },
        { "stop", [] (Settings& s) { s.stop = true; } },
        { "mix", [] (Settings& s) { s.mixPct = 100; } },
        { "gain", [] (Settings& s) { s.gainDb = 12; } },
    };
    for (const auto& t : toggles)
    {
        Settings s;
        s.rateHz = 0.5;
        auto e = makeEngine (s, fs);
        auto x = sine (24000, 80.0, 0.25);
        auto x2 = x;
        e.process (x.data(), x2.data(), 12000);
        const double before = maxStep (std::vector<double> (x.begin() + 1000, x.begin() + 12000));
        t.flip (s);
        e.setSettings (s);
        e.process (x.data() + 12000, x2.data() + 12000, 12000);
        const double during = maxStep (std::vector<double> (x.begin() + 11990, x.begin() + 13500));
        INFO (t.name << ": step before " << before << ", during " << during);
        CHECK (during < 4.0 * before + 0.01);
    }
}

TEST_CASE ("1: variants - Mono equals the left processor, Mono->Stereo ignores input 2", "[variants]")
{
    Settings s;
    s.rateHz = 1.3;
    s.stereoDeg = 90;
    s.feedbackPct = 70;
    const int n = 48000;
    const auto x1 = noise (n, 1), x2 = noise (n, 2);

    const auto stereo = render (s, fs, x1, x2);
    const auto stereoSame = render (s, fs, x1, x1);

    auto mono = makeEngine (s, fs, Variant::Mono);
    auto m = x1;
    mono.process (m.data(), static_cast<double*> (nullptr), n);
    CHECK (maxAbsDiff (m, stereo.l) == 0.0);

    auto m2s = makeEngine (s, fs, Variant::MonoToStereo);
    const auto o = run (m2s, x1, x2); // x2 must be ignored
    CHECK (maxAbsDiff (o.l, stereoSame.l) == 0.0);
    CHECK (maxAbsDiff (o.r, stereoSame.r) == 0.0);
    CHECK (maxAbsDiff (o.l, o.r) > 0.01); // output 2 leads by 90 degrees

    CHECK (numInputChannels (Variant::MonoToStereo) == 1);
    CHECK (numOutputChannels (Variant::MonoToStereo) == 2);
    CHECK (numOutputChannels (Variant::Mono) == 1);
    CHECK (FlangerEngine::getLatencySamples() == 0);
}

TEST_CASE ("bypass crossfades to the unprocessed input", "[bypass]")
{
    Settings s;
    auto e = makeEngine (s, fs);
    const auto x = noise (48000, 9);
    auto l = x, r = x;
    e.process (l.data(), r.data(), 4800);
    e.setBypassed (true);
    e.process (l.data() + 4800, r.data() + 4800, 48000 - 4800);
    for (int i = 4800 + 1000; i < 48000; ++i)
        REQUIRE (l[size_t (i)] == x[size_t (i)]);
}

TEST_CASE ("block size does not change the output", "[engine]")
{
    Settings s;
    s.rateHz = 2.2;
    s.filterOn = true;
    s.tape = true;
    const auto x = noise (20000, 4);
    auto e1 = makeEngine (s, fs), e2 = makeEngine (s, fs);
    const auto a = run (e1, x, x, 1), b = run (e2, x, x, 4096);
    CHECK (maxAbsDiff (a.l, b.l) == 0.0);
    CHECK (maxAbsDiff (a.r, b.r) == 0.0);
}

TEST_CASE ("float and double processing agree", "[engine]")
{
    Settings s;
    s.rateHz = 0.7;
    const auto x = noise (8000, 8);
    auto e1 = makeEngine (s, fs), e2 = makeEngine (s, fs);
    std::vector<float> fl (x.size()), fr (x.size());
    for (size_t i = 0; i < x.size(); ++i)
        fl[i] = fr[i] = float (x[i]);
    e1.process (fl.data(), fr.data(), 8000);
    const auto d = run (e2, x, x);
    for (size_t i = 0; i < fl.size(); ++i)
        REQUIRE (std::abs (double (fl[i]) - d.l[i]) < 1e-6);
}
