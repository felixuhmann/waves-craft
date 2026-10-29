// Spec r1 sections 2 and 3: parameter mappings, note values, delay-time resolution, display formats.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
namespace pm = craft::hdelay::param;

TEST_CASE ("Delay ms taper passes through the spec points", "[params][delay]")
{
    CHECK (pm::DelayMs::fromNorm (0.0) == 1.0);
    CHECK (pm::DelayMs::fromNorm (0.25) == 251.0);
    CHECK (pm::DelayMs::fromNorm (0.374) == 375.0);
    CHECK (pm::DelayMs::fromNorm (0.5) == 501.0);
    const double r[] = { 0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85, 0.90, 0.95, 1.0 };
    const double ms[] = { 636, 815, 1024, 1265, 1540, 1852, 2201, 2591, 3023, 3500 };
    for (int i = 0; i < 10; ++i)
        CHECK (pm::DelayMs::fromNorm (r[i]) == ms[i]);

    // strictly monotonic (continuous taper) and C1 at the joint
    double prev = 0.0;
    for (int i = 0; i <= 2000; ++i)
    {
        const double v = pm::DelayMs::taper (i / 2000.0);
        CHECK (v > prev);
        prev = v;
    }
    const double slopeBelow = (pm::DelayMs::taper (0.5) - pm::DelayMs::taper (0.5 - 1e-6)) / 1e-6;
    const double slopeAbove = (pm::DelayMs::taper (0.5 + 1e-6) - pm::DelayMs::taper (0.5)) / 1e-6;
    CHECK_THAT (slopeAbove, WithinRel (slopeBelow, 0.01));

    // inverse
    for (double v : { 1.0, 2.0, 375.0, 500.0, 501.0, 777.0, 1000.0, 2222.0, 3499.0, 3500.0 })
        CHECK (pm::DelayMs::fromNorm (pm::DelayMs::toNorm (v)) == v);
}

TEST_CASE ("Note values: order, durations, names", "[params][notes]")
{
    REQUIRE (pm::numNotes == 20);
    CHECK (std::string (pm::noteNames[0]) == "1/64T");
    CHECK (std::string (pm::noteNames[11]) == "1/8D");
    CHECK (std::string (pm::noteNames[19]) == "2 bars");
    CHECK (pm::defaultNote == 11);

    CHECK_THAT (pm::noteBeats (0), WithinAbs (1.0 / 16.0 * 2.0 / 3.0, 1e-15)); // 1/64T
    CHECK (pm::noteBeats (1) == 1.0 / 16.0);                                   // 1/64
    CHECK (pm::noteBeats (2) == 1.5 / 16.0);                                   // 1/64D
    CHECK (pm::noteBeats (7) == 0.25);                                         // 1/16
    CHECK (pm::noteBeats (10) == 0.5);                                         // 1/8
    CHECK (pm::noteBeats (11) == 0.75);                                        // 1/8D
    CHECK_THAT (pm::noteBeats (12), WithinAbs (2.0 / 3.0, 1e-15));             // 1/4T
    CHECK (pm::noteBeats (13) == 1.0);                                         // 1/4 = one beat
    CHECK (pm::noteBeats (14) == 1.5);                                         // 1/4D
    CHECK (pm::noteBeats (16) == 2.0);                                         // 1/2
    CHECK (pm::noteBeats (17) == 3.0);                                         // 1/2D
    CHECK (pm::noteBeats (18) == 4.0);                                         // 1 bar
    CHECK (pm::noteBeats (19) == 8.0);                                         // 2 bars
    // within a family (T, straight, D) the durations double from one value to the next
    for (int i = 3; i < 18; ++i)
        CHECK (pm::noteBeats (i) == 2.0 * pm::noteBeats (i - 3));
}

TEST_CASE ("Delay time resolution and the 3500 ms fallback", "[delay]")
{
    using pm::Sync;
    // default: Host sync, 1/8D at 120 BPM = 375 ms
    CHECK_THAT (resolveDelayMs (Sync::Host, 375, 11, 120, 120), WithinAbs (375.0, 1e-12));
    // BPM sync uses the manual tempo, Host sync the host tempo
    CHECK_THAT (resolveDelayMs (Sync::Bpm, 375, 13, 100, 60), WithinAbs (600.0, 1e-12));
    CHECK_THAT (resolveDelayMs (Sync::Host, 375, 13, 100, 60), WithinAbs (1000.0, 1e-12));
    CHECK (resolveDelayMs (Sync::Ms, 1234, 13, 100, 60) == 1234.0);

    // fallback at 60 BPM: 1 bar (4 s) and 2 bars (8 s) both give 3.0 s = 1/2D
    CHECK (effectiveNote (18, 60.0) == 17);
    CHECK (effectiveNote (19, 60.0) == 17);
    CHECK_THAT (resolveDelayMs (Sync::Bpm, 0, 18, 60, 120), WithinAbs (3000.0, 1e-9));
    CHECK_THAT (resolveDelayMs (Sync::Bpm, 0, 19, 60, 120), WithinAbs (3000.0, 1e-9));
    // at 120 BPM 2 bars = 4 s -> 1 bar (2 s), the longest fitting note
    CHECK (effectiveNote (19, 120.0) == 18);
    // at 30 BPM: 1/2 = 4 s too long, 1/4D = 3 s fits; 1/2T = 2.667 s; longest fitting is 1/4D
    CHECK (effectiveNote (19, 30.0) == 14);
    // exactly 3500 ms is allowed
    CHECK (effectiveNote (18, 4.0 * 60000.0 / 3500.0) == 18);
    // every resolved time is within range
    for (double bpm = 30.0; bpm <= 300.0; bpm += 0.5)
        for (int n = 0; n < pm::numNotes; ++n)
        {
            const double t = resolveDelayMs (Sync::Bpm, 0, n, bpm, 120);
            CHECK (t <= 3500.0 + 1e-9);
            CHECK (t >= 1.0);
        }
}

TEST_CASE ("Delay samples are rounded to the nearest sample at every rate", "[delay][timing]")
{
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        for (double ms : { 1.0, 10.0, 100.0, 375.0, 1000.0, 3500.0 })
            CHECK (delaySamples (ms, fs) == std::round (ms * fs / 1000.0));
        for (double bpm : { 60.0, 96.0, 120.0, 133.0, 140.0 })
            for (int n = 0; n < pm::numNotes; ++n)
            {
                const double exact = resolveDelayMs (pm::Sync::Bpm, 0, n, bpm, 120) * fs / 1000.0;
                CHECK (std::abs (delaySamples (resolveDelayMs (pm::Sync::Bpm, 0, n, bpm, 120), fs) - exact) <= 0.5 + 1e-9);
            }
    }
    // triplets are not truncated (review decision 11.2): 1/4T at 120 BPM = 333.33 ms = 16000 samples at 48 kHz
    CHECK (delaySamples (resolveDelayMs (pm::Sync::Bpm, 0, 12, 120, 120), 48000.0) == 16000.0);
    CHECK (delaySamples (resolveDelayMs (pm::Sync::Bpm, 0, 12, 120, 120), 44100.0) == 14700.0);
    CHECK (delaySamples (resolveDelayMs (pm::Sync::Bpm, 0, 9, 120, 120), 48000.0) == 8000.0); // 1/8T
}

TEST_CASE ("Linear and log parameter mappings", "[params]")
{
    CHECK (pm::Bpm::fromNorm (0.0) == 30.0);
    CHECK (pm::Bpm::fromNorm (1.0) == 300.0);
    CHECK_THAT (pm::Bpm::fromNorm (pm::Bpm::toNorm (120.0)), WithinAbs (120.0, 1e-9));
    CHECK (pm::Feedback::fromNorm (0.3) == 60.0);
    CHECK (pm::Feedback::fromNorm (1.0) == 200.0);
    CHECK (pm::Output::fromNorm (0.5) == 0.0);
    CHECK (pm::Output::fromNorm (0.0) == -18.0);
    CHECK (pm::Mix::fromNorm (1.0) == 100.0);

    CHECK_THAT (pm::ModRate::fromNorm (0.0), WithinRel (0.1, 1e-12));
    CHECK_THAT (pm::ModRate::fromNorm (1.0), WithinRel (6000.0, 1e-12));
    CHECK_THAT (pm::ModRate::fromNorm (0.5), WithinRel (0.1 * std::sqrt (60000.0), 1e-12));
    CHECK_THAT (pm::ModRate::toNorm (pm::ModRate::fromNorm (0.37)), WithinAbs (0.37, 1e-12));

    // filters: Off at the ends, 19 * 1058^r in between
    CHECK (pm::HiPass::fromNorm (0.0) == 0.0);
    CHECK_THAT (pm::HiPass::fromNorm (0.05), WithinRel (26.9, 0.01));
    CHECK_THAT (pm::HiPass::fromNorm (1.0), WithinRel (20100.0, 0.01));
    CHECK (pm::LoPass::fromNorm (1.0) == 0.0);
    CHECK_THAT (pm::LoPass::fromNorm (0.0), WithinRel (19.0, 1e-12));
    CHECK_THAT (pm::LoPass::fromNorm (0.95), WithinRel (14200.0, 0.01));
    CHECK_THAT (pm::Filter::freq (0.5), WithinRel (19.0 * std::sqrt (1058.0), 1e-12));
}

TEST_CASE ("Display formats", "[params][format]")
{
    CHECK (pm::formatMs (375.0) == "375 ms");
    CHECK (pm::formatPercent (60.0) == "60");
    CHECK (pm::formatPercent (100.0) == "100");
    CHECK (pm::formatOutput (0.0) == "0.0 dB");
    CHECK (pm::formatOutput (-0.01) == "0.0 dB");
    CHECK (pm::formatOutput (-6.0) == "-6.0 dB");
    CHECK (pm::formatRate (0.1) == "0.10 Hz");
    CHECK (pm::formatRate (1150.0) == "1.15k Hz");
    CHECK (pm::formatDepth (0.0) == "0 %");
    CHECK (pm::formatFilter (437.0) == "437Hz");
    CHECK (pm::formatFilter (1240.0) == "1.24kHz");
    CHECK (pm::formatFilter (0.0) == "Off");
}

TEST_CASE ("Tap: nearest note value", "[params][tap]")
{
    CHECK (nearestNote (500.0, 120.0) == 13);   // one beat
    CHECK (nearestNote (370.0, 120.0) == 11);   // ~1/8D
    CHECK (nearestNote (2000.0, 120.0) == 18);  // 1 bar
    CHECK (nearestNote (3400.0, 120.0) == 18);  // 2 bars does not fit, 1 bar is nearest
}
