// Spec r1 sections 4, 7, 8, 9, 10: fixed chain, output HF response, mixer, delay taps / feedback / mix table,
// routes, drive, crush, sample-rate behaviour and robustness.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::mf;
using namespace mftest;
using Catch::Approx;

TEST_CASE ("Wet chain: 21 Hz Butterworth high-pass and +0.35 dB", "[chain]")
{
    Settings s = transparentSettings();
    const double fs = 96000.0;
    CHECK (db (engineGain (s, fs, 1000.0)) == Approx (0.35).margin (0.05));
    CHECK (db (engineGain (s, fs, 20.0, 0.0316, 1.0, 0.5)) - 0.35 == Approx (-3.6).margin (0.3));
    CHECK (db (engineGain (s, fs, 50.0, 0.0316, 1.0, 0.5)) - 0.35 == Approx (-0.15).margin (0.1));
    // the dry path is not high-passed
    s.mix = -100.0;
    CHECK (db (engineGain (s, fs, 20.0, 0.0316, 1.0, 0.5)) == Approx (0.0).margin (0.05));
}

TEST_CASE ("Output HF response at 44.1 / 48 kHz and flat at high rates", "[chain][samplerate]")
{
    struct P { double f, dB; };
    const P points[] = { { 10000, -0.3 }, { 15000, -1.2 }, { 18000, -2.1 }, { 20000, -3.2 }, { 22000, -4.8 } };
    for (double fs : { 44100.0, 48000.0 })
    {
        Settings s;
        s.mix = -100.0; // dry path
        for (auto p : points)
        {
            if (p.f >= 0.5 * fs)
                continue;
            const double got = db (engineGain (s, fs, p.f));
            INFO ("fs " << fs << " f " << p.f << " got " << got);
            if (p.f <= 10000.0)
                CHECK (std::abs (got - p.dB) <= 0.5);
            else
                CHECK ((got - p.dB >= -5.0 && got - p.dB <= 0.5));
        }
        CHECK (db (engineGain (s, fs, 1000.0)) == Approx (0.0).margin (0.1));
        CHECK (db (engineGain (s, fs, 5000.0)) == Approx (0.0).margin (0.2));
    }
    for (double fs : { 88200.0, 96000.0, 192000.0 })
    {
        Settings s;
        s.mix = -100.0;
        for (double f : { 1000.0, 10000.0, 15000.0, 20000.0 })
            CHECK (std::abs (db (engineGain (s, fs, f))) <= 0.5);
    }
}

TEST_CASE ("Filter mix: dry (1 - m)/2, wet (1 + m)/2", "[chain]")
{
    CHECK (law::mixDry (-100) == 1.0);
    CHECK (law::mixWet (-100) == 0.0);
    CHECK (law::mixDry (0) == 0.5);
    CHECK (law::mixWet (50) == 0.75);
    Settings s = transparentSettings();
    s.freqNorm = 0.0; // muted wet
    s.mix = 0.0;
    CHECK (db (engineGain (s, 96000.0, 1000.0)) == Approx (-6.02).margin (0.05));
    s.mix = 100.0;
    s.freqNorm = 1.0;
    s.inputDb = -6.0;
    s.outputDb = 4.0;
    CHECK (db (engineGain (s, 96000.0, 1000.0)) == Approx (-2.0 + law::wetGainDb).margin (0.05));
}

namespace
{
// Impulse response of the engine (left and right).
StereoOut impulse (const Settings& s, double fs, double seconds, double amp = 0.01, Transport t = {})
{
    auto e = makeEngine (s, fs);
    std::vector<double> x (size_t (seconds * fs), 0.0);
    x[0] = amp;
    return runEngine (e, x, t);
}
size_t peakIndex (const std::vector<double>& y, size_t from, size_t to)
{
    size_t best = from;
    for (size_t i = from; i < std::min (to, y.size()); ++i)
        if (std::abs (y[i]) > std::abs (y[best]))
            best = i;
    return best;
}
double localSum (const std::vector<double>& y, size_t centre, int halfWidth = 40)
{
    double s = 0;
    for (int k = -halfWidth; k <= halfWidth; ++k)
        if (int (centre) + k >= 0 && size_t (int (centre) + k) < y.size())
            s += y[size_t (int (centre) + k)];
    return s;
}
} // namespace

TEST_CASE ("Delay taps at the displayed time, right x ratio (review decision 3)", "[delay]")
{
    for (double fs : { 44100.0, 96000.0 })
        for (double ms : { 50.0, 415.0, 1000.0 })
            for (int ratio : { 0, 1, 2 })
            {
                Settings s = transparentSettings();
                s.delaySync = false;
                s.delayMs = ms;
                s.delayRatioIndex = ratio;
                s.delayMix = 0.0;
                s.delayFeedback = 0.0;
                const double dl = ms * 0.001 * fs, dr = dl * delayRatioChoices[size_t (ratio)];
                auto o = impulse (s, fs, 2.2 * ms * 0.001 * 2.0 + 0.01);
                INFO ("fs " << fs << " ms " << ms << " ratio " << ratio);
                CHECK (std::abs (double (peakIndex (o.l, 1000, o.l.size())) - dl - latencyFor (fs)) <= 1.0);
                CHECK (std::abs (double (peakIndex (o.r, 1000, o.r.size())) - dr - latencyFor (fs)) <= 1.0);
            }
}

TEST_CASE ("Synced delay times are note lengths at host tempo", "[delay][sync]")
{
    const double fs = 48000.0;
    Transport t;
    t.valid = true;
    t.playing = true;
    t.bpm = 120.0;
    for (auto [index, seconds] : { std::pair { 3, 0.25 }, std::pair { 0, 0.0625 }, std::pair { 7, 0.75 }, std::pair { 9, 2.0 } })
    {
        Settings s = transparentSettings();
        s.delaySync = true;
        s.delaySyncIndex = index;
        s.delayMix = 0.0;
        s.delayFeedback = 0.0;
        auto o = impulse (s, fs, seconds + 0.05, 0.01, t);
        CHECK (std::abs (double (peakIndex (o.l, 1000, o.l.size())) - seconds * fs - latencyFor (fs)) <= 1.0);
    }
}

TEST_CASE ("Delay feedback: each repeat = previous x Feedback (linear)", "[delay]")
{
    const double fs = 48000.0;
    for (double fb : { 0.0, 30.0, 70.0, 100.0 })
    {
        Settings s = transparentSettings();
        s.delaySync = false;
        s.delayMs = 100.0;
        s.delayMix = 0.0;
        s.delayFeedback = fb;
        auto o = impulse (s, fs, 0.45);
        const size_t d = 4800;
        const size_t L = size_t (latencyFor (fs));
        const double t1 = localSum (o.l, L + d), t2 = localSum (o.l, L + 2 * d), t3 = localSum (o.l, L + 3 * d);
        CHECK (t2 / t1 == Approx (fb / 100.0).margin (0.01));
        if (fb > 0)
            CHECK (t3 / t2 == Approx (fb / 100.0).margin (0.01));
    }
}

TEST_CASE ("Delay mix table (first tap, feedback 0)", "[delay]")
{
    const double fs = 96000.0;
    const double ms[] = { -100, -75, -50, -25, 0, 25, 50, 75, 100 };
    // reference: delay mix -100 -> dry gain 1 (the chain's +0.35 dB etc. cancel in the ratio)
    Settings ref = transparentSettings();
    ref.delaySync = false;
    ref.delayMs = 100.0;
    ref.delayFeedback = 0.0;
    ref.delayMix = -100.0;
    const auto r0 = impulse (ref, fs, 0.12);
    const double unit = localSum (r0.l, size_t (latencyFor (fs)));
    for (size_t k = 0; k < 9; ++k)
    {
        Settings s = ref;
        s.delayMix = ms[k];
        const auto o = impulse (s, fs, 0.12);
        const double dry = localSum (o.l, size_t (latencyFor (fs))) / unit, wet = localSum (o.l, size_t (latencyFor (fs)) + 9600) / unit;
        INFO ("mix " << ms[k]);
        CHECK (dry == Approx (law::delayMixDryTable[k]).margin (0.03));
        CHECK (wet == Approx (law::delayMixWetTable[k]).margin (0.03));
    }
    // interpolation between the table points stays smooth and within the table's range
    for (double m = -100; m <= 100; m += 1.0)
    {
        CHECK (law::delayMixDry (m) >= 0.0);
        CHECK (law::delayMixDry (m) <= 1.0);
        CHECK (law::delayMixWet (m) <= 0.65);
    }
}

TEST_CASE ("Delay routes: pre-filter vs. filter inside the feedback loop", "[delay]")
{
    const double fs = 48000.0;
    Settings s;
    s.type = FilterType::LowPass;
    s.freqNorm = freqKnob (500.0);
    s.delaySync = false;
    s.delayMs = 50.0;
    s.delayMix = 0.0;
    s.delayFeedback = 50.0;

    // test tone 4 kHz: pre route -> every repeat filtered once; feedback route -> repeat n filtered n times
    auto burst = [&] (const Settings& st) {
        auto e = makeEngine (st, fs);
        std::vector<double> x (size_t (0.25 * fs), 0.0);
        for (size_t i = 0; i < 480; ++i)
            x[i] = 0.1 * std::sin (2.0 * pi * 4000.0 * double (i) / fs) * std::sin (pi * double (i) / 480.0);
        auto o = runEngine (e, x);
        auto energy = [&] (size_t from) {
            double sum = 0;
            for (size_t i = from; i < from + 1200; ++i) sum += o.l[i] * o.l[i];
            return std::sqrt (sum);
        };
        return std::array<double, 3> { energy (2400 - 300), energy (4800 - 300), energy (7200 - 300) };
    };
    s.delayRouteFeedback = false;
    const auto pre = burst (s);
    s.delayRouteFeedback = true;
    const auto fbk = burst (s);
    // pre: repeat 2 / repeat 1 = feedback ratio
    CHECK (pre[1] / pre[0] == Approx (0.5).margin (0.03));
    // feedback route: repeat 2 / repeat 1 is the feedback ratio x the filter's attenuation at 4 kHz (<< 0.5)
    CHECK (fbk[1] / fbk[0] < 0.1);

    // delay mix -100 in feedback route leaves the plain filtered signal (no repeats)
    s.delayMix = -100.0;
    const auto off = burst (s);
    CHECK (off[0] < 1.0e-6);
}

TEST_CASE ("Delay 100 % feedback sustains and is guarded", "[delay]")
{
    const double fs = 48000.0;
    Settings s = transparentSettings();
    s.delaySync = false;
    s.delayMs = 50.0;
    s.delayMix = 0.0;
    s.delayFeedback = 100.0;
    auto o = impulse (s, fs, 2.0);
    const double first = localSum (o.l, size_t (latencyFor (fs)) + 2400), late = localSum (o.l, size_t (latencyFor (fs)) + 2400 * 30);
    CHECK (late / first == Approx (1.0).margin (0.02));

    // loud continuous input at 100 % feedback must not blow up
    auto e = makeEngine (s, fs);
    std::vector<double> x (size_t (5 * fs));
    Random rnd;
    for (auto& v : x) v = 0.9 * rnd.nextBipolar();
    auto big = runEngine (e, x);
    double peak = 0.0;
    for (double v : big.l) peak = std::max (peak, std::abs (v));
    CHECK (std::isfinite (peak));
    CHECK (peak < 4.0);
}

TEST_CASE ("Delay time changes: digital crossfade, analog glide", "[delay]")
{
    const double fs = 48000.0;
    DelayChannel d;
    d.prepare (fs);
    d.setTarget (4800.0);
    for (int i = 0; i < 100; ++i) { d.read(); d.write (0.0); }
    d.setTarget (9600.0);
    d.read();
    d.write (0.0);
    CHECK (d.currentDelay() == Approx (4800.0)); // digital: old tap still sounding during the fade
    for (int i = 0; i < int (0.021 * fs); ++i) { d.read(); d.write (0.0); }
    CHECK (d.currentDelay() == Approx (9600.0));

    DelayChannel a;
    a.prepare (fs);
    a.setAnalog (true);
    a.setTarget (4800.0);
    a.read();
    a.setTarget (9600.0);
    for (int i = 0; i < int (0.1 * fs); ++i) { a.read(); a.write (0.0); }
    // one time constant (100 ms): 63 % of the way
    CHECK ((a.currentDelay() - 4800.0) / 4800.0 == Approx (0.632).margin (0.02));
}

TEST_CASE ("Drive: +0.073 dB per % small-signal, compression at -3 dBFS", "[drive]")
{
    const double fs = 96000.0;
    struct P { double drive, gainDb, compDb; };
    // compression targets at drive 0 / 25 / 50 / 100 (drive 0: baseline 0.2 dB, section 4)
    for (auto p : { P { 0, 0.0, 0.2 }, P { 25, 1.9, 0.4 }, P { 50, 3.7, 0.8 }, P { 100, 7.2, 2.7 } })
    {
        Settings s = transparentSettings();
        s.drive = p.drive;
        const double small = db (engineGain (s, fs, 1000.0, 0.0316)) - law::wetGainDb;
        const double loud = db (engineGain (s, fs, 1000.0, fromDb (-3.0))) - law::wetGainDb;
        INFO ("drive " << p.drive << " small " << small << " compression " << small - loud);
        CHECK (small == Approx (p.gainDb).margin (0.5));
        CHECK (small - loud == Approx (p.compDb).margin (0.5));
        // also at -4.8 dBFS (the level the verification harness's -3 dBFS stimulus reaches the saturator with)
        const double lower = db (engineGain (s, fs, 1000.0, fromDb (-4.8))) - law::wetGainDb;
        CHECK (small - lower == Approx (p.compDb).margin (0.5));
    }
}

TEST_CASE ("Baseline saturation of the default wet path", "[drive]")
{
    Settings s; // LP 18 kHz, Res 0, drive 0
    const double small = db (engineGain (s, 48000.0, 1000.0, 0.0316));
    const double loud = db (engineGain (s, 48000.0, 1000.0, fromDb (-3.0)));
    CHECK (small - loud == Approx (0.2).margin (0.15));
    // odd-dominant harmonics: the saturator is odd-symmetric
    CHECK (saturate (0.5, 2.0, 1.5) == Approx (-saturate (-0.5, 2.0, 1.5)));
}

TEST_CASE ("Crush: hold N = 1 + 64 (Crush/100)^2 samples at 48 kHz, host-rate independent (r2)", "[crush][samplerate]")
{
    CHECK (law::crushHoldSamples48k (0) == Approx (1.0));
    CHECK (law::crushHoldSamples48k (25) == Approx (5.0));
    CHECK (law::crushHoldSamples48k (50) == Approx (17.0));
    CHECK (law::crushHoldSamples48k (75) == Approx (37.0));
    CHECK (law::crushHoldSamples48k (100) == Approx (65.0));
    CHECK (law::crushRateHz (50) == Approx (48000.0 / 17.0));
    // hold lengths at 48 kHz exactly N samples (+-1)
    for (double crush : { 25.0, 50.0, 75.0, 100.0 })
    {
        Crusher c;
        c.reset();
        double prev = -1.0;
        int run = 0, minRun = 1 << 30, maxRun = 0;
        for (int i = 0; i < 48000; ++i)
        {
            const double y = c.process (double (i + 1), law::crushRateHz (crush), 48000.0);
            if (y != prev)
            {
                if (i > 200) { minRun = std::min (minRun, run); maxRun = std::max (maxRun, run); }
                run = 0;
            }
            ++run;
            prev = y;
        }
        const double n = law::crushHoldSamples48k (crush);
        CHECK (double (minRun) >= n - 1.0);
        CHECK (double (maxRun) <= n + 1.0);
    }
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (double crush : { 25.0, 50.0, 100.0 })
        {
            Crusher c;
            c.reset();
            int changes = 0;
            double prev = 0.0;
            const double rate = law::crushRateHz (crush);
            for (int i = 0; i < int (fs); ++i)
            {
                const double y = c.process (double (i + 1), rate, fs); // ramp: every hold gives a new value
                if (y != prev) ++changes;
                prev = y;
            }
            INFO ("fs " << fs << " crush " << crush);
            CHECK (double (changes) == Approx (rate).epsilon (0.10));
        }

    // engine: the output level is preserved (held sample values), a 100 Hz tone keeps its level at Crush 50
    {
        Settings s = transparentSettings();
        s.crush = 50.0;
        CHECK (db (engineGain (s, 48000.0, 100.0)) == Approx (law::wetGainDb).margin (0.3));
    }
    // and a fast tone gets strong aliasing products (hold 37 samples = 1.3 kHz rate vs a 997 Hz tone)
    Settings s = transparentSettings();
    s.crush = 75.0;
    auto e = makeEngine (s, 48000.0);
    std::vector<double> x (48000);
    for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5 * std::sin (2.0 * pi * 997.0 * double (i) / 48000.0);
    auto o = runEngine (e, x);
    double total = 0.0;
    for (size_t i = 24000; i < 48000; ++i) total += o.l[i] * o.l[i];
    const double rms = std::sqrt (total / 24000.0);
    const double tone = toneAmplitude (o.l, 24000, 24000, 997.0, 48000.0) / std::sqrt (2.0);
    CHECK (rms > 0.25);                               // level preserved overall
    CHECK (std::sqrt (rms * rms - tone * tone) > 0.3 * rms); // > -10 dB of non-harmonic energy
}

TEST_CASE ("Sample-rate behaviour: same cutoff, delay and rates at every host rate", "[samplerate]")
{
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        Settings s;
        s.freqNorm = freqKnob (2000.0);
        auto e = makeEngine (s, fs);
        traceTelemetry (e, 8, [] (const auto& t) { return t.cutoffHz[0]; });
        CHECK (e.getTelemetry().cutoffHz[0] == Approx (2000.0).epsilon (1e-9));
        CHECK (e.getTelemetry().delayMs[0] == Approx (250.0).epsilon (1e-9)); // 1/8 at 120 BPM default
    }
}

TEST_CASE ("Parameter changes are click-free", "[smoothing]")
{
    const double fs = 48000.0;
    Settings s;
    s.freqNorm = freqKnob (300.0);
    auto e = makeEngine (s, fs);
    std::vector<double> x (static_cast<size_t> (fs));
    for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5 * std::sin (2.0 * pi * 100.0 * double (i) / fs);
    double prev = 0.0, maxStep = 0.0;
    for (size_t i = 0; i < x.size(); i += 128)
    {
        if (i == 9600) { s.freqNorm = freqKnob (8000.0); s.resonance = 90.0; s.outputDb = 6.0; s.mix = -50.0; e.setSettings (s); }
        if (i == 24000) { s.inputDb = -20.0; s.drive = 100.0; s.delayMix = 50.0; e.setSettings (s); }
        double l[128], r[128];
        for (int k = 0; k < 128; ++k) l[k] = r[k] = x[i + size_t (k)];
        e.process (l, r, (const double*) nullptr, (const double*) nullptr, 128);
        for (int k = 0; k < 128; ++k)
        {
            if (i + size_t (k) > 1000)
                maxStep = std::max (maxStep, std::abs (l[k] - prev));
            prev = l[k];
        }
    }
    CHECK (maxStep < 0.05); // a 100 Hz sine at 0.5 (x2 output) changes by ~0.013 per sample
}

TEST_CASE ("Robustness: extreme settings stay finite", "[robustness]")
{
    const double fs = 44100.0;
    Settings s;
    s.resonance = 100.0;
    s.drive = 100.0;
    s.crush = 30.0;
    s.delayFeedback = 100.0;
    s.delayMix = 50.0;
    s.delayRouteFeedback = true;
    s.delaySync = false;
    s.delayMs = 50.0;
    s.mod[modLfo][destFreq] = 100.0;
    s.mod[modFollower][destRes] = 100.0;
    s.mod[modSeq][destTime] = -100.0;
    s.lfoSync = false;
    s.lfoRateHz = 100.0;
    s.inputDb = 10.0;
    s.outputDb = 10.0;
    for (int k = 0; k < 16; ++k) s.steps[size_t (k)] = (k % 2 ? 100.0 : -100.0);
    s.seqSync = false;
    s.seqRate = 500.0;
    for (auto type : { FilterType::LowPass, FilterType::BandPass, FilterType::Comb, FilterType::Amp })
    {
        s.type = type;
        auto e = makeEngine (s, fs);
        std::vector<double> x (size_t (3 * fs));
        Random rnd;
        for (auto& v : x) v = rnd.nextBipolar();
        auto o = runEngine (e, x);
        double peak = 0.0;
        bool finite = true;
        for (double v : o.l) { finite = finite && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
        CHECK (finite);
        CHECK (peak < 100.0);
    }
}

TEST_CASE ("Bypass crossfades to the unprocessed input; latency is zero", "[chain]")
{
    CHECK (MotionEngine::latencySamples == 0);
    CHECK (latencyFor (44100.0) == 37); // 4x oversampled, compensated (<= 64)
    CHECK (latencyFor (48000.0) == 37);
    CHECK (latencyFor (96000.0) == 31); // 2x
    CHECK (latencyFor (192000.0) == 0);
    Settings s;
    s.freqNorm = freqKnob (200.0);
    s.bypass = true;
    auto e = makeEngine (s, 192000.0);
    std::vector<double> x (4800);
    for (size_t i = 0; i < x.size(); ++i) x[i] = std::sin (0.1 * double (i));
    auto o = runEngine (e, x);
    for (size_t i = 0; i < x.size(); ++i)
        CHECK (o.l[i] == Approx (x[i]).margin (1e-12));

    // oversampled rates: bypassed output is the input delayed by exactly the reported latency
    for (double fs : { 48000.0, 96000.0 })
    {
        auto eb = makeEngine (s, fs);
        const size_t lat = size_t (eb.getLatencySamples());
        std::vector<double> sine (4800);
        for (size_t i = 0; i < sine.size(); ++i) sine[i] = std::sin (2.0 * pi * 1000.0 * double (i) / fs);
        auto ob = runEngine (eb, sine);
        double err = 0.0;
        for (size_t i = 500; i < sine.size(); ++i) err = std::max (err, std::abs (ob.l[i] - sine[i - lat]));
        CHECK (err < (fs < 60000.0 ? 0.02 : 1.0e-3)); // at 48 kHz the §4 output response adds a little phase
    }
}
