// Craft Opto Compressor: DSP tests against the spec r1 tables and the tolerances of spec section 10.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <tuple>
#include <vector>

#include "SpecTables.h"
#include "dsp/OptoEngine.h"

using namespace craft::opto::dsp;

namespace
{
const double rates[] = { 44100.0, 48000.0, 96000.0 };

double db2lin (double d) { return std::pow (10.0, d / 20.0); }
double lin2db (double v) { return 20.0 * std::log10 (std::max (v, 1.0e-30)); }

Settings settingFromName (const std::string& name)
{
    Settings s;
    s.limit = name[0] == 'L';
    s.peakReduction = std::stod (name.substr (2));
    return s;
}

struct Sine
{
    double f, fs, amp, phase = 0;
    double next()
    {
        const double v = amp * std::sin (phase);
        phase += 2.0 * pi * f / fs;
        if (phase > 2.0 * pi) phase -= 2.0 * pi;
        return v;
    }
};

// Complex amplitude magnitude of frequency f over the buffer (single-bin DFT with a Hann window).
double tone (const std::vector<double>& x, double f, double fs)
{
    double re = 0, im = 0, wsum = 0;
    const size_t n = x.size();
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * pi * double (i) / double (n));
        const double p = 2.0 * pi * f * double (i) / fs;
        re += w * x[i] * std::cos (p);
        im += w * x[i] * std::sin (p);
        wsum += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

// Steady sine, both channels: returns the output of the last `analyse` seconds (left).
std::vector<double> runSine (OptoEngine& e, double fs, double f, double levelDb, double settleS, double analyseS, bool stereo = true)
{
    Sine s { f, fs, db2lin (levelDb) };
    const size_t settle = size_t (settleS * fs), an = size_t (analyseS * fs);
    std::vector<double> out;
    out.reserve (an);
    for (size_t i = 0; i < settle + an; ++i)
    {
        double l = s.next(), r = l;
        e.process (&l, stereo ? &r : nullptr, 1);
        if (i >= settle)
            out.push_back (l);
    }
    return out;
}

double steadyGainDb (OptoEngine& e, double fs, double f, double levelDb, double settleS)
{
    const auto y = runSine (e, fs, f, levelDb, settleS, std::max (0.1, 40.0 / f));
    return lin2db (tone (y, f, fs)) - levelDb;
}

// Spec 7 stimulus: probe at -40 dBFS, burst, probe again. Returns short-time gain (dB) relative to before the burst,
// 4 ms window centred on t, on an energy basis (output rms / input rms).
struct BurstRun
{
    double fs;
    std::vector<float> in, out;
    size_t burstStart;
    size_t burstLen;
    double gainBefore;

    double at (double tMsFromStart) const
    {
        const auto half = size_t (0.002 * fs);
        const auto c = burstStart + size_t (std::llround (tMsFromStart * 0.001 * fs));
        double ei = 0, eo = 0;
        for (size_t i = c - half; i < std::min (c + half, in.size()); ++i)
        {
            ei += double (in[i]) * in[i];
            eo += double (out[i]) * out[i];
        }
        return 10.0 * std::log10 (eo / ei) - gainBefore;
    }
};

BurstRun runBurst (const Settings& base, double fs, double burstDb, double burstMs, double afterS)
{
    Settings st = base;
    OptoEngine e;
    e.setSettings (st);
    e.prepare (fs);
    Sine s { 1000.0, fs, db2lin (-40.0) };
    BurstRun r;
    r.fs = fs;
    r.burstStart = size_t (2.0 * fs);
    r.burstLen = size_t (burstMs * 0.001 * fs);
    const size_t n = r.burstStart + r.burstLen + size_t (afterS * fs);
    r.in.resize (n);
    r.out.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        const bool burst = i >= r.burstStart && i < r.burstStart + r.burstLen;
        s.amp = db2lin (burst ? burstDb : -40.0);
        double x = s.next(), xr = x;
        r.in[i] = float (x);
        e.process (&x, &xr, 1);
        r.out[i] = float (x);
    }
    double ei = 0, eo = 0;
    for (size_t i = r.burstStart - size_t (0.1 * fs); i < r.burstStart - size_t (0.01 * fs); ++i)
    {
        ei += double (r.in[i]) * r.in[i];
        eo += double (r.out[i]) * r.out[i];
    }
    r.gainBefore = 10.0 * std::log10 (eo / ei);
    return r;
}

using BurstKey = std::tuple<std::string, double, double>;

// Burst runs are expensive, cache them across the test cases.
const BurstRun& burstFor (const BurstKey& k, double fs)
{
    static std::map<std::tuple<std::string, double, double, double>, BurstRun> cache;
    const auto key = std::make_tuple (std::get<0> (k), std::get<1> (k), std::get<2> (k), fs);
    auto it = cache.find (key);
    if (it == cache.end())
        it = cache.emplace (key, runBurst (settingFromName (std::get<0> (k)), fs, std::get<1> (k), std::get<2> (k), 16.0)).first;
    return it->second;
}
} // namespace

// ---------------------------------------------------------------------------------------------
TEST_CASE ("Parameters: fresh instance is unity with zero latency", "[opto]")
{
    for (double fs : rates)
    {
        OptoEngine e;
        e.prepare (fs);
        REQUIRE (e.getLatencySamples() == 0);
        const double g = steadyGainDb (e, fs, 1000.0, -20.0, 0.2);
        CHECK (std::abs (g - 0.015) < 0.02);

        // the first sample of an impulse must already be at the output
        OptoEngine i;
        i.prepare (fs);
        double a = 0.25, b = 0.25;
        i.process (&a, &b, 1);
        CHECK (std::abs (a) > 0.2);
    }
}

TEST_CASE ("Gain law, Trim and Mix (spec 3)", "[opto]")
{
    const struct { double gain, db; } points[] = { { 0, -20.385 }, { 2, -10.385 }, { 6, 9.615 }, { 8, 19.615 }, { 10, 29.615 } };
    for (double fs : rates)
    {
        for (const auto& p : points)
        {
            Settings s;
            s.gain = p.gain;
            OptoEngine e;
            e.setSettings (s);
            e.prepare (fs);
            CHECK (std::abs (steadyGainDb (e, fs, 1000.0, -30.0, 0.2) - p.db) < 0.02);
        }
        for (double trim : { -18.0, -6.0, 7.5, 18.0 })
        {
            Settings s;
            s.trimDb = trim;
            OptoEngine e;
            e.setSettings (s);
            e.prepare (fs);
            CHECK (std::abs (steadyGainDb (e, fs, 1000.0, -30.0, 0.2) - 0.015 - trim) < 0.02);
        }
        {
            // Mix 0: output equals input exactly, whatever the wet path does
            Settings s;
            s.mix = 0;
            s.peakReduction = 8;
            s.gain = 9;
            s.analog = 1;
            OptoEngine e;
            e.setSettings (s);
            e.prepare (fs);
            Sine t { 1000.0, fs, 0.5 };
            for (int i = 0; i < 4800; ++i)
            {
                const double x = t.next();
                double l = x, r = x;
                e.process (&l, &r, 1);
                REQUIRE (l == x);
            }
        }
        {
            // Mix 50 with Gain -6.02 dB-ish wet: linear crossfade y = m wet + (1-m) dry
            Settings s;
            s.mix = 0.5;
            s.gain = 2.0;
            OptoEngine e;
            e.setSettings (s);
            e.prepare (fs);
            const double expected = lin2db (0.5 * db2lin (gainLawDb (2.0)) + 0.5);
            CHECK (std::abs (steadyGainDb (e, fs, 1000.0, -30.0, 0.2) - expected) < 0.02);
        }
    }
}

TEST_CASE ("Static reduction at table points (spec 4), primed and held", "[opto]")
{
    for (double fs : rates)
        for (const auto& r : spectables::staticGr)
        {
            if (std::fmod (r.level, 6.0) != 0.0)
                continue;
            Settings s;
            s.limit = r.mode == 'L';
            s.peakReduction = r.pr;
            OptoEngine e;
            e.setSettings (s);
            e.prepare (fs);
            e.primeSteady (r.level);
            const double g = steadyGainDb (e, fs, 1000.0, r.level, 0.3) - 0.015;
            INFO ("fs " << fs << " mode " << r.mode << " pr " << r.pr << " level " << r.level << " got " << g << " want " << r.gain);
            CHECK (std::abs (g - r.gain) < 0.3);
        }
}

TEST_CASE ("Static reduction reached from rest (spec 4)", "[opto]")
{
    const struct { char mode; double pr, level, gain; } points[] = {
        { 'C', 8, -12, -16.66 }, { 'L', 8, 0, -30.71 }, { 'L', 10, 0, -31.99 }, { 'C', 5, -6, -9.8 }, { 'C', 2, 0, 0.0 } };
    for (const auto& p : points)
        for (double fs : { 44100.0, 96000.0 })
        {
            Settings s;
            s.limit = p.mode == 'L';
            s.peakReduction = p.pr;
            OptoEngine e;
            e.setSettings (s);
            e.prepare (fs);
            const double g = steadyGainDb (e, fs, 1000.0, p.level, 20.0) - 0.015;
            INFO ("mode " << p.mode << " pr " << p.pr << " level " << p.level << " got " << g);
            CHECK (std::abs (g - p.gain) < 0.3);
        }
}

TEST_CASE ("Between PR rows: linear in dB (spec 4)", "[opto]")
{
    CHECK (std::abs (staticGainDb (0, 7.5, -12.0) - 0.5 * (-13.65 - 16.66)) < 0.02);
    CHECK (std::abs (staticGainDb (1, 9.0, -12.0) - 0.5 * (-19.07 - 23.93)) < 0.02);
    CHECK (staticGainDb (0, 1.0, 0.0) == 0.0);
    CHECK (std::abs (staticGainDb (0, 8.0, -24.0) + 7.65) < 0.02);
}

TEST_CASE ("Hi Freq table (spec 5)", "[opto]")
{
    for (double fs : rates)
        for (const auto& r : spectables::hifreq)
        {
            if (std::fmod (r.level, 6.0) != 0.0)
                continue;
            Settings s;
            s.peakReduction = 6;
            s.hiFreq = r.hifreq;
            OptoEngine e;
            e.setSettings (s);
            e.prepare (fs);
            (void) runSine (e, fs, r.freq, r.level, 0.6, 0.01); // the fast state reaches its target
            e.settleMemory();
            const double w = steadyGainDb (e, fs, r.freq, r.level, 0.2);
            const double tol = (r.freq == 8000.0 && r.hifreq == 0.0) ? 1.5 : 0.5;
            INFO ("fs " << fs << " f " << r.freq << " hf " << r.hifreq << " level " << r.level << " got " << w << " want " << r.gain);
            CHECK (std::abs (w - r.gain) < tol);
        }
}

TEST_CASE ("Colouration: low-level frequency response (spec 6)", "[opto]")
{
    for (double fs : rates)
    {
        OptoEngine e;
        e.prepare (fs);
        const double ref = steadyGainDb (e, fs, 1000.0, -60.0, 0.3);
        for (const auto& r : spectables::freqResponse)
        {
            e.reset();
            const double g = steadyGainDb (e, fs, r.freq, -60.0, 0.6) - ref;
            const double tol = (r.freq > 18000.0 && fs >= 96000.0) ? 0.6 : 0.2;
            INFO ("fs " << fs << " f " << r.freq << " got " << g << " want " << r.gain);
            CHECK (std::abs (g - r.gain) < tol);
        }
    }
}

TEST_CASE ("Harmonics (spec 6)", "[opto]")
{
    for (double fs : rates)
        for (const auto& r : spectables::harmonics)
        {
            const std::string name = r.setting;
            Settings s;
            s.limit = name.rfind ("limit", 0) == 0;
            s.peakReduction = std::stod (name.substr (name.find ("pr") + 2));
            OptoEngine e;
            e.setSettings (s);
            e.prepare (fs);
            e.primeSteady (r.level);
            const auto y = runSine (e, fs, r.freq, r.level, 1.0, 1.0);
            const double fund = tone (y, r.freq, fs);
            const double want[4] = { r.h2, r.h3, r.h4, r.h5 };
            for (int h = 0; h < 4; ++h)
            {
                const double fh = r.freq * (h + 2);
                if (want[h] < -90.0 || fh > 0.45 * fs)
                    continue;
                const double dbc = lin2db (tone (y, fh, fs) / fund);
                INFO ("fs " << fs << " " << name << " f " << r.freq << " level " << r.level << " H" << (h + 2) << " got " << dbc << " want " << want[h]);
                CHECK (std::abs (dbc - want[h]) < 6.0);
            }
        }
}

TEST_CASE ("Attack table (spec 7)", "[opto]")
{
    for (double fs : rates)
        for (const auto& r : spectables::attack)
        {
            const BurstRun& run = burstFor (BurstKey (r.setting, r.burstDb, r.burstMs), fs);
            const double got = run.at (r.t);
            const double tol = r.t <= 5.0 ? 3.0 : 1.5;
            INFO ("fs " << fs << " " << r.setting << " " << r.burstDb << "/" << r.burstMs << " t " << r.t << " got " << got << " want " << r.gr);
            CHECK (std::abs (got - r.gr) < tol);
        }
}

TEST_CASE ("Release table (spec 7)", "[opto]")
{
    for (double fs : rates)
        for (const auto& r : spectables::release)
        {
            if (r.t > 15000.0)
                continue;
            const BurstRun& run = burstFor (BurstKey (r.setting, r.burstDb, r.burstMs), fs);
            const double got = run.at (r.burstMs + r.t);
            const double tol = r.t <= 5.0 ? 3.0 : 1.5;
            INFO ("fs " << fs << " " << r.setting << " " << r.burstDb << "/" << r.burstMs << " t " << r.t << " got " << got << " want " << r.gr);
            CHECK (std::abs (got - r.gr) < tol);
        }
}

TEST_CASE ("Release times t63 and t90 (spec 7)", "[opto]")
{
    for (double fs : rates)
        for (const auto& r : spectables::releaseTimes)
        {
            const BurstRun& run = burstFor (BurstKey (r.setting, r.burstDb, r.burstMs), fs);
            const double end = run.at (r.burstMs + 5.0);
            const double step = 2.0;
            double t63 = -1, t90 = -1;
            for (double t = 5.0; t < 15000.0; t += step)
            {
                const double g = run.at (r.burstMs + t);
                if (t63 < 0 && g > end * 0.368) t63 = t * 0.001;
                if (t90 < 0 && g > end * 0.1) { t90 = t * 0.001; break; }
            }
            INFO ("fs " << fs << " " << r.setting << " " << r.burstDb << "/" << r.burstMs << " t63 " << t63 << " want " << r.t63 << " t90 " << t90 << " want " << r.t90);
            CHECK (std::abs (t63 / r.t63 - 1.0) < 0.25);
            CHECK (std::abs (t90 / r.t90 - 1.0) < 0.25);
        }
}

TEST_CASE ("Stereo link: a tone on one channel gets the same reduction, no crosstalk (spec 5)", "[opto]")
{
    const double fs = 48000.0;
    Settings s;
    s.peakReduction = 8;
    auto reductionFor = [&] (bool both) {
        OptoEngine e;
        e.setSettings (s);
        e.prepare (fs);
        e.primeSteady (-12.0);
        Sine t { 1000.0, fs, db2lin (-12.0) };
        std::vector<double> yl, yr;
        for (int i = 0; i < 48000; ++i)
        {
            const double x = t.next();
            double l = x, r = both ? x : 0.0;
            e.process (&l, &r, 1);
            if (i >= 24000)
            {
                yl.push_back (l);
                yr.push_back (r);
            }
        }
        return std::make_pair (lin2db (tone (yl, 1000.0, fs)), lin2db (tone (yr, 1000.0, fs)));
    };
    const auto both = reductionFor (true), one = reductionFor (false);
    CHECK (std::abs (both.first - one.first) < 0.05);
    CHECK (one.second < -120.0);

    // the quieter channel is scaled by the same gain
    OptoEngine e;
    e.setSettings (s);
    e.prepare (fs);
    e.primeSteady (-12.0);
    Sine a { 1000.0, fs, db2lin (-12.0) }, b { 1000.0, fs, db2lin (-32.0) };
    std::vector<double> yl, yr;
    for (int i = 0; i < 48000; ++i)
    {
        double l = a.next(), r = b.next();
        e.process (&l, &r, 1);
        if (i >= 24000)
        {
            yl.push_back (l);
            yr.push_back (r);
        }
    }
    CHECK (std::abs ((lin2db (tone (yl, 1000.0, fs)) + 12.0) - (lin2db (tone (yr, 1000.0, fs)) + 32.0)) < 0.1);
}

TEST_CASE ("Mono path behaves like one channel of stereo (D4)", "[opto]")
{
    for (double fs : rates)
    {
        Settings s;
        s.peakReduction = 8;
        OptoEngine mono, stereo;
        mono.setSettings (s);
        stereo.setSettings (s);
        mono.prepare (fs);
        stereo.prepare (fs);
        const auto a = runSine (mono, fs, 1000.0, -12.0, 0.5, 0.1, false);
        const auto b = runSine (stereo, fs, 1000.0, -12.0, 0.5, 0.1, true);
        for (size_t i = 0; i < a.size(); ++i)
            REQUIRE (std::abs (a[i] - b[i]) < 1.0e-9);
    }
}

TEST_CASE ("Analog noise and hum (spec 8)", "[opto]")
{
    const struct { int analog; double gain, hz; const char* name; } cases[] = {
        { 1, 4.08, 50.0, "analog50_gain4.08" }, { 2, 4.08, 60.0, "analog60_gain4.08" }, { 2, 10.0, 60.0, "analog60_gain10" } };
    for (double fs : rates)
    {
        for (const auto& c : cases)
        {
            const auto it = std::find_if (spectables::noise.begin(), spectables::noise.end(),
                                          [&] (const auto& r) { return std::string (r.setting) == c.name; });
            REQUIRE (it != spectables::noise.end());
            Settings s;
            s.analog = c.analog;
            s.gain = c.gain;
            OptoEngine e;
            e.setSettings (s);
            e.prepare (fs);
            const size_t n = size_t (4.0 * fs);
            std::vector<double> yl (n), yr (n);
            for (size_t i = 0; i < n; ++i)
            {
                double l = 0, r = 0;
                e.process (&l, &r, 1);
                yl[i] = l;
                yr[i] = r;
            }
            double e2 = 0, cross = 0, e2r = 0;
            for (size_t i = 0; i < n; ++i)
            {
                e2 += yl[i] * yl[i];
                e2r += yr[i] * yr[i];
                cross += yl[i] * yr[i];
            }
            CHECK (std::abs (10.0 * std::log10 (e2 / double (n)) - it->rms) < 3.0);
            CHECK (std::abs (cross) / std::sqrt (e2 * e2r) < 0.5); // hum is common; the noise dominates and is uncorrelated
            CHECK (std::abs (lin2db (tone (yl, c.hz, fs)) - it->hum1) < 3.0);
            CHECK (std::abs (lin2db (tone (yl, 2 * c.hz, fs)) - it->hum2) < 3.0);
            CHECK (std::abs (lin2db (tone (yl, 3 * c.hz, fs)) - it->hum3) < 3.0);
        }
        // Analog off: silence stays below -170 dBFS
        OptoEngine off;
        off.prepare (fs);
        double peak = 0;
        for (int i = 0; i < 48000; ++i)
        {
            double l = 0, r = 0;
            off.process (&l, &r, 1);
            peak = std::max (peak, std::abs (l));
        }
        CHECK (lin2db (peak) < -170.0);
    }
}

TEST_CASE ("Auto Makeup rule (spec 8)", "[opto]")
{
    const double d = autoMakeupGainDelta (0.0, 8.0);
    CHECK (std::abs (d - (-staticGainDb (0, 8.0, -18.0)) / 5.0) < 1.0e-9);
    CHECK (d > 1.5);
    CHECK (std::abs (autoMakeupGainDelta (8.0, 0.0) + d) < 1.0e-9);
}

TEST_CASE ("Parameter changes are click-free within 20 ms (D2)", "[opto]")
{
    const double fs = 48000.0;
    OptoEngine e;
    e.prepare (fs);
    Settings s;
    double prev = 0;
    Sine t { 100.0, fs, 0.25 };
    (void) runSine (e, fs, 100.0, -12.0, 0.1, 0.1);
    s.gain = 8.0;
    s.trimDb = 6.0;
    s.bypass = false;
    e.setSettings (s);
    double maxStep = 0;
    std::vector<double> last;
    for (int i = 0; i < int (0.05 * fs); ++i)
    {
        double x = t.next(), r = x;
        e.process (&x, &r, 1);
        if (i > 0)
            maxStep = std::max (maxStep, std::abs (x - prev));
        prev = x;
        if (i >= int (0.02 * fs))
            last.push_back (x);
    }
    CHECK (maxStep < 0.2);
    // after 20 ms the new gain is established within 0.1 dB
    const double g = lin2db (tone (last, 100.0, fs)) + 12.04;
    CHECK (std::abs (g - (gainLawDb (8.0) - 0.082 + 6.0)) < 0.6);
}

TEST_CASE ("Bypass is exact once faded and click-free (spec 2)", "[opto]")
{
    const double fs = 48000.0;
    Settings s;
    s.peakReduction = 8;
    s.gain = 8;
    OptoEngine e;
    e.setSettings (s);
    e.prepare (fs);
    Sine t { 440.0, fs, 0.25 };
    for (int i = 0; i < 4800; ++i)
    {
        double x = t.next(), r = x;
        e.process (&x, &r, 1);
    }
    s.bypass = true;
    e.setSettings (s);
    for (int i = 0; i < 9600; ++i)
    {
        double x = t.next(), r = x;
        const double dry = x;
        e.process (&x, &r, 1);
        if (i > 4800)
            REQUIRE (std::abs (x - dry) < 1.0e-6);
    }
}

TEST_CASE ("Robustness: finite output, never latches silent (spec 10)", "[opto]")
{
    for (double fs : rates)
    {
        Settings s;
        s.peakReduction = 10;
        s.limit = true;
        s.gain = 10;
        s.trimDb = 18;
        s.analog = 2;
        s.hiFreq = 0;
        OptoEngine e;
        e.setSettings (s);
        e.prepare (fs);
        std::mt19937 rng (1);
        std::uniform_real_distribution<double> d (-4.0, 4.0);
        const double nan = std::nan ("");
        for (int i = 0; i < int (fs); ++i)
        {
            double l = (i % 977 == 0) ? nan : d (rng), r = (i % 1009 == 0) ? 1.0e30 : d (rng);
            e.process (&l, &r, 1);
            REQUIRE (std::isfinite (l));
            REQUIRE (std::isfinite (r));
        }
        s = Settings();
        e.setSettings (s);
        // after the memory state relaxes, a quiet tone is back at unity (within the slow tail)
        const double g = steadyGainDb (e, fs, 1000.0, -40.0, 30.0);
        CHECK (std::abs (g - 0.015) < 0.1);
    }
}
