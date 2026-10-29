// Spec r1 section 6: depth tapers, modulation ranges, LFO / sequencer timing, envelope follower, Smooth,
// keyboard tracking. Tolerances from section 12 (offsets +-0.15 oct or +-8 %, rates +-0.5 %, Smooth +-20 %).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::mf;
using namespace mftest;
using Catch::Approx;

namespace
{
bool offsetWithin (double got, double expect)
{
    return std::abs (got - expect) <= std::max (0.15, 0.08 * std::abs (expect));
}

// Freq offset (octaves) produced by a constant sequencer value v at amount a (Smooth 0, knob 1 kHz).
double seqFreqOffset (double v, double a)
{
    Settings s;
    s.freqNorm = freqKnob (1000.0);
    s.smooth = 0.0;
    s.steps.fill (v * 100.0);
    s.mod[modSeq][destFreq] = a * 100.0;
    auto e = makeEngine (s, 48000.0);
    traceTelemetry (e, 4, [] (const auto& t) { return t.cutoffHz[0]; });
    return octavesFrom (e.getTelemetry().cutoffHz[0], 1000.0);
}
} // namespace

TEST_CASE ("Depth taper T_k", "[mod]")
{
    CHECK (law::taper (0.0, 3.04) == 0.0);
    CHECK (law::taper (1.0, 3.04) == Approx (1.0));
    CHECK (law::taper (-1.0, 3.57) == Approx (-1.0));
    CHECK (law::taper (0.5, 3.04) == Approx ((std::exp (1.52) - 1) / (std::exp (3.04) - 1)));
    CHECK (law::taper (-0.25, 3.57) == Approx (-(std::exp (0.8925) - 1) / (std::exp (3.57) - 1)));
}

TEST_CASE ("Sequencer -> Freq matches the measured offsets", "[mod]")
{
    struct P { double v, a, oct; };
    const P points[] = { { 0.10, 1.0, 0.51 }, { 0.25, 1.0, 1.28 }, { 0.50, 1.0, 2.62 }, { 1.0, 0.10, 0.09 },
                         { 1.0, 0.25, 0.30 }, { 1.0, 0.40, 0.62 }, { 1.0, 0.50, 0.93 }, { 1.0, 0.60, 1.36 },
                         { 1.0, 0.75, 2.31 } };
    for (const auto& p : points)
    {
        const double got = seqFreqOffset (p.v, p.a);
        INFO ("v " << p.v << " a " << p.a << " got " << got << " expect " << p.oct);
        CHECK (offsetWithin (got, p.oct));
        CHECK (offsetWithin (seqFreqOffset (-p.v, p.a), -p.oct));
    }
}

TEST_CASE ("LFO -> Freq: +-0.39 oct at 25 %, +-1.33 oct at 50 %", "[mod][lfo]")
{
    for (auto [a, expect] : { std::pair { 0.25, 0.39 }, std::pair { 0.50, 1.33 } })
    {
        Settings s;
        s.freqNorm = freqKnob (1000.0);
        s.smooth = 0.0;
        s.lfoShape = LfoShape::Pulse; // +1 then -1
        s.lfoSync = false;
        s.lfoRateHz = 2.0;
        s.mod[modLfo][destFreq] = a * 100.0;
        auto e = makeEngine (s, 48000.0);
        auto oct = traceTelemetry (e, 24000, [] (const auto& t) { return std::log2 (t.cutoffHz[0] / 1000.0); });
        CHECK (offsetWithin (oct[100], expect));
        CHECK (offsetWithin (oct[12100], -expect));
    }
}

TEST_CASE ("Resonance destination adds 100 v T(a) points", "[mod]")
{
    Settings s;
    s.smooth = 0.0;
    s.resonance = 20.0;
    s.steps.fill (50.0);
    s.mod[modSeq][destRes] = 100.0;
    auto e = makeEngine (s, 48000.0);
    traceTelemetry (e, 4, [] (const auto& t) { return t.resonance[0]; });
    CHECK (e.getTelemetry().resonance[0] == Approx (70.0).margin (0.01));
    s.steps.fill (-100.0);
    auto e2 = makeEngine (s, 48000.0);
    traceTelemetry (e2, 4, [] (const auto& t) { return t.resonance[0]; });
    CHECK (e2.getTelemetry().resonance[0] == Approx (0.0).margin (1e-9)); // clamped
}

TEST_CASE ("Amp destination law", "[mod]")
{
    const double knob = fromDb (-40.0);
    CHECK (law::modulatedAmpGain (knob, 0.0) == Approx (knob));
    CHECK (law::modulatedAmpGain (knob, 1.0) == Approx (knob + 0.0647 * (std::exp (1.502) - 1.0)));
    CHECK (law::modulatedAmpGain (knob, -1.0) == 0.0); // large negative m mutes

    // engine: sequencer v = 1, a = 100 % on an Amp filter at -40 dB
    Settings s = transparentSettings();
    s.freqNorm = 0.5; // -40 dB
    s.smooth = 0.0;
    s.steps.fill (100.0);
    s.mod[modSeq][destFreq] = 100.0;
    const double g = engineGain (s, 96000.0, 1000.0);
    CHECK (db (g) == Approx (db (law::modulatedAmpGain (knob, 1.0)) + law::wetGainDb).margin (0.1));
}

TEST_CASE ("Delay-time destination scales in units of the knob range", "[mod][delay]")
{
    Settings s = transparentSettings();
    s.smooth = 0.0;
    s.delaySync = false;
    s.delayMs = 100.0;
    s.delayAnalog = true;
    s.steps.fill (100.0);
    s.mod[modSeq][destTime] = 100.0;
    auto e = makeEngine (s, 48000.0);
    traceTelemetry (e, 48000, [] (const auto& t) { return t.delayMs[0]; });
    const double expect = 100.0 * std::pow (20.0, law::seqFreqOct / law::knobRangeOct);
    CHECK (e.getTelemetry().delayMs[0] == Approx (expect).epsilon (0.01));
}

TEST_CASE ("LFO waveforms start at phase 0", "[lfo]")
{
    CHECK (Lfo::waveform (LfoShape::Sine, 0.0, 0) == Approx (0.0).margin (1e-12));
    CHECK (Lfo::waveform (LfoShape::Sine, 0.25, 0) == Approx (1.0));
    CHECK (Lfo::waveform (LfoShape::Triangle, 0.0, 0) == Approx (0.0));
    CHECK (Lfo::waveform (LfoShape::Triangle, 0.125, 0) == Approx (0.5)); // rising
    CHECK (Lfo::waveform (LfoShape::Triangle, 0.25, 0) == Approx (1.0));
    CHECK (Lfo::waveform (LfoShape::Triangle, 0.75, 0) == Approx (-1.0));
    CHECK (Lfo::waveform (LfoShape::Saw, 0.0, 0) == Approx (1.0));
    CHECK (Lfo::waveform (LfoShape::Saw, 0.5, 0) == Approx (0.0));
    CHECK (Lfo::waveform (LfoShape::Saw, 0.999, 0) == Approx (-1.0).margin (0.01));
    CHECK (Lfo::waveform (LfoShape::Pulse, 0.1, 0) == 1.0);
    CHECK (Lfo::waveform (LfoShape::Pulse, 0.6, 0) == -1.0);

    Lfo lfo;
    lfo.setShape (LfoShape::SampleHold);
    lfo.reset();
    lfo.clock.inc = 0.01;
    double first = lfo.value(), mn = 1, mx = -1;
    int changes = 0;
    for (int i = 0; i < 100000; ++i)
    {
        const double prev = lfo.value();
        lfo.advance();
        const double v = lfo.value();
        if (v != prev) ++changes;
        mn = std::min (mn, v);
        mx = std::max (mx, v);
    }
    (void) first;
    CHECK (changes == 1000); // one new value per cycle
    CHECK (mn >= -1.0);
    CHECK (mx <= 1.0);
    CHECK (mn < -0.9);
    CHECK (mx > 0.9);
}

namespace
{
// Period (seconds) of the LFO from rising zero crossings of the telemetry trace.
double lfoPeriod (const std::vector<double>& v, double fs)
{
    std::vector<double> crossings;
    for (size_t i = 1; i < v.size(); ++i)
        if (v[i - 1] < 0.0 && v[i] >= 0.0)
            crossings.push_back (double (i - 1) + (-v[i - 1]) / (v[i] - v[i - 1]));
    if (crossings.size() < 2)
        return 0.0;
    return (crossings.back() - crossings.front()) / double (crossings.size() - 1) / fs;
}
} // namespace

TEST_CASE ("LFO free rate is exact", "[lfo]")
{
    for (double fs : { 44100.0, 96000.0 })
        for (double rate : { 0.5, 4.0, 37.0 })
        {
            Settings s;
            s.lfoSync = false;
            s.lfoRateHz = rate;
            auto e = makeEngine (s, fs);
            auto v = traceTelemetry (e, size_t (fs * std::max (3.0, 6.0 / rate)), [] (const auto& t) { return t.lfo; });
            CHECK (lfoPeriod (v, fs) == Approx (1.0 / rate).epsilon (0.001));
            CHECK (v[0] == Approx (0.0).margin (0.01)); // phase 0 at plugin start
            CHECK (v[1] > 0.0);
        }
}

TEST_CASE ("LFO synced rates are note lengths at host tempo", "[lfo][sync]")
{
    const double fs = 48000.0;
    struct C { int index; double seconds; };
    for (auto c : { C { 3, 1.0 }, C { 7, 1.0 / 3.0 }, C { 9, 1.0 / 6.0 }, C { 2, 2.0 } })
    {
        Settings s;
        s.lfoSync = true;
        s.lfoSyncIndex = c.index;
        Transport t;
        t.valid = true;
        t.playing = true;
        t.bpm = 120.0;
        t.ppq = 0.0;
        auto e = makeEngine (s, fs);
        auto v = traceTelemetry (e, size_t (fs * 5.0), [] (const auto& tl) { return tl.lfo; }, {}, t);
        INFO ("choice " << lfoSyncChoices[size_t (c.index)].text);
        CHECK (lfoPeriod (v, fs) == Approx (c.seconds).epsilon (0.005));
        CHECK (v[0] == Approx (0.0).margin (0.01)); // phase 0 at transport start
    }
    // starting the transport mid-song locks the phase to the song position: ppq 3 with a 1/2 (2 beat) cycle
    // = half a cycle -> sine at phase 0.5 = 0, falling
    Settings s;
    s.lfoSyncIndex = 3;
    Transport t;
    t.valid = true;
    t.playing = true;
    t.ppq = 3.0;
    auto e = makeEngine (s, fs);
    auto v = traceTelemetry (e, 100, [] (const auto& tl) { return tl.lfo; }, {}, t);
    CHECK (v[0] == Approx (0.0).margin (0.01));
    CHECK (v[50] < 0.0);
}

TEST_CASE ("Sequencer starts on step 1 and advances at its rate", "[seq]")
{
    const double fs = 48000.0;
    // free running: 4 steps/s -> 12000 samples per step, from step 1 (review decision 4)
    Settings s;
    s.seqSync = false;
    s.seqRate = 4.0;
    auto e = makeEngine (s, fs);
    auto st = traceTelemetry (e, 60000, [] (const auto& t) { return double (t.step); });
    CHECK (st[0] == 0.0);
    CHECK (st[11999] == 0.0);
    CHECK (st[12000] == 1.0);
    CHECK (st[24000] == 2.0);

    // loop over the last step
    s.lastStep = 3;
    s.seqRate = 100.0; // 480 samples per step
    auto e2 = makeEngine (s, fs);
    auto st2 = traceTelemetry (e2, 2000, [] (const auto& t) { return double (t.step); });
    CHECK (st2[480 * 2] == 2.0);
    CHECK (st2[480 * 3] == 0.0);
    CHECK (st2[480 * 4] == 1.0);

    // synced 1/16 at 120 BPM = 125 ms per step
    Settings ss;
    ss.seqSync = true;
    ss.seqSyncIndex = 8;
    Transport t;
    t.valid = true;
    t.playing = true;
    t.bpm = 120.0;
    auto e3 = makeEngine (ss, fs);
    auto st3 = traceTelemetry (e3, 30000, [] (const auto& tl) { return double (tl.step); }, {}, t);
    CHECK (st3[0] == 0.0);
    CHECK (st3[5999] == 0.0);
    CHECK (st3[6001] == 1.0);
    CHECK (st3[12001] == 2.0);
}

TEST_CASE ("Swing delays every second step", "[seq]")
{
    CHECK (StepSequencer::stepIndexAt (0.0, 16, 50.0) == 0);
    CHECK (StepSequencer::stepIndexAt (1.4, 16, 50.0) == 0);
    CHECK (StepSequencer::stepIndexAt (1.5, 16, 50.0) == 1);
    CHECK (StepSequencer::stepIndexAt (1.99, 16, 50.0) == 1);
    CHECK (StepSequencer::stepIndexAt (2.0, 16, 50.0) == 2);
    CHECK (StepSequencer::stepIndexAt (1.0, 16, 0.0) == 1);
    CHECK (StepSequencer::stepIndexAt (3.74, 16, 75.0) == 2);
    CHECK (StepSequencer::stepIndexAt (3.76, 16, 75.0) == 3);
}

TEST_CASE ("Envelope follower -> Freq follows the r2 target table", "[mod][follower]")
{
    struct P { double amount, peak, oct; };
    const P table[] = {
        { 50, 0.01, 0.06 }, { 50, 0.072, 0.43 }, { 50, 0.135, 0.82 }, { 50, 0.26, 1.60 }, { 50, 0.51, 1.79 },
        { -50, 0.01, -0.06 }, { -50, 0.072, -0.42 }, { -50, 0.135, -0.83 }, { -50, 0.26, -1.66 },
        { 100, 0.01, 0.60 }, { 100, 0.072, 1.80 }, { 100, 0.135, 1.80 }, { 100, 0.26, 1.81 }, { 100, 0.51, 1.82 } };
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (const auto& p : table)
        {
            Settings s;
            s.type = FilterType::HighPass;
            s.smooth = 0.0;
            s.freqNorm = freqKnob (2000.0);
            s.mod[modFollower][destFreq] = p.amount;
            auto e = makeEngine (s, fs);
            const size_t n = size_t (0.3 * fs);
            auto oct = traceTelemetry (e, n, [] (const auto& t) { return std::log2 (t.cutoffHz[0] / 2000.0); },
                                       [&] (size_t i) { return p.peak * std::sin (2.0 * pi * 1000.0 * double (i) / fs); });
            double mean = 0.0;
            for (size_t i = n / 2; i < n; ++i) mean += oct[i];
            mean /= double (n - n / 2);
            INFO ("fs " << fs << " amount " << p.amount << " A " << p.peak << ": " << mean << " oct, expect " << p.oct);
            CHECK (std::abs (mean - p.oct) <= std::max (0.2, 0.25 * std::abs (p.oct)));
        }
    // -50 % at A = 0.51: at least -4 octaves
    {
        Settings s;
        s.smooth = 0.0;
        s.freqNorm = freqKnob (2000.0);
        s.mod[modFollower][destFreq] = -50.0;
        CHECK (law::followerOctaves (0.51, -0.5) <= -4.0);
    }
    // fast ballistics: attack within 5 ms, release within 25 ms
    EnvelopeFollower f;
    f.prepare (48000.0);
    double env = 0.0;
    for (int i = 0; i < 240; ++i) env = f.process (1.0);
    CHECK (env > 0.95);
    for (int i = 0; i < 1200; ++i) env = f.process (0.0);
    CHECK (env < 0.37);
}

TEST_CASE ("Smooth is a one-pole slew with tau = Smooth % x 1 ms", "[mod][smoothing]")
{
    for (double smooth : { 10.0, 50.0, 100.0 })
    {
        Settings s;
        s.freqNorm = freqKnob (1000.0);
        s.smooth = smooth;
        s.lfoShape = LfoShape::Pulse;
        s.lfoSync = false;
        s.lfoRateHz = 0.1; // +1 for 5 s
        s.mod[modLfo][destFreq] = 50.0;
        const double fs = 48000.0;
        auto e = makeEngine (s, fs);
        auto oct = traceTelemetry (e, size_t (fs), [] (const auto& t) { return std::log2 (t.cutoffHz[0] / 1000.0); });
        const double target = law::lfoFreqOct * law::taper (0.5, law::taperLfo);
        size_t i63 = 0;
        while (i63 < oct.size() && oct[i63] < 0.632 * target)
            ++i63;
        CHECK (double (i63) / fs == Approx (smooth * 0.001).epsilon (0.20));
    }
}

TEST_CASE ("Keyboard tracking: (n - 60)/12 octaves, comb delay x 2^(-(n-60)/12)", "[mod][kbd]")
{
    Settings s;
    s.freqNorm = freqKnob (1000.0);
    s.smooth = 0.0;
    s.keyboardTracking = true;
    auto e = makeEngine (s, 48000.0);
    e.setNote (72);
    traceTelemetry (e, 4, [] (const auto& t) { return t.cutoffHz[0]; });
    CHECK (octavesFrom (e.getTelemetry().cutoffHz[0], 1000.0) == Approx (1.0).margin (1e-6));
    e.setNote (48);
    traceTelemetry (e, 4, [] (const auto& t) { return t.cutoffHz[0]; });
    CHECK (octavesFrom (e.getTelemetry().cutoffHz[0], 1000.0) == Approx (-1.0).margin (1e-6));

    s.type = FilterType::Comb;
    s.freqNorm = 0.5; // 19.5 ms
    auto c = makeEngine (s, 48000.0);
    c.setNote (72);
    traceTelemetry (c, 4, [] (const auto& t) { return t.combDelayMs[0]; });
    CHECK (c.getTelemetry().combDelayMs[0] == Approx (19.5 / 2.0).epsilon (1e-6));

    s.keyboardTracking = false;
    auto off = makeEngine (s, 48000.0);
    off.setNote (72);
    traceTelemetry (off, 4, [] (const auto& t) { return t.combDelayMs[0]; });
    CHECK (off.getTelemetry().combDelayMs[0] == Approx (19.5).epsilon (1e-6));
}
