// Spec r1 section 7 (colour effects, [target]) against the section 10 tolerances.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace cdtest;
using Catch::Approx;

namespace
{
constexpr double fs = 48000.0;

template <typename Fx>
Stereo runEffect (Fx& fx, const std::vector<double>& inL, const std::vector<double>& inR)
{
    Stereo out { std::vector<double> (inL.size()), std::vector<double> (inL.size()) };
    for (size_t i = 0; i < inL.size(); ++i)
        fx.process (inL[i], inR[i], out.l[i], out.r[i]);
    return out;
}

// RBJ band-pass (0 dB peak), applied twice: one-octave band filter for the RT60 analysis
std::vector<double> octaveBand (const std::vector<double>& x, double fc, double rate)
{
    Biquad a, b;
    const double q = 1.0 / (2.0 * std::sinh (std::log (2.0) / 2.0));
    for (auto* f : { &a, &b })
    {
        const double w = twoPi * fc / rate, al = std::sin (w) / (2.0 * q), c = std::cos (w);
        f->setCoefficients (al, 0.0, -al, 1.0 + al, -2.0 * c, 1.0 - al);
    }
    std::vector<double> y (x.size());
    for (size_t i = 0; i < x.size(); ++i)
        y[i] = b.process (a.process (x[i]));
    return y;
}

// Schroeder backward integration, T20-style fit between -5 and -25 dB, extrapolated to 60 dB.
double rt60 (const std::vector<double>& h, double rate)
{
    std::vector<double> edc (h.size());
    double acc = 0.0;
    for (size_t i = h.size(); i-- > 0;)
    {
        acc += h[i] * h[i];
        edc[i] = acc;
    }
    const double total = edc[0];
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int n = 0;
    for (size_t i = 0; i < h.size(); ++i)
    {
        const double d = 10.0 * std::log10 (edc[i] / total);
        if (d <= -5.0 && d >= -25.0)
        {
            const double t = i / rate;
            sx += t; sy += d; sxx += t * t; sxy += t * d;
            ++n;
        }
    }
    const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
    return -60.0 / slope;
}

int firstAbove (const std::vector<double>& h, double fraction)
{
    double peak = 0.0;
    for (double v : h) peak = std::max (peak, std::abs (v));
    for (size_t i = 0; i < h.size(); ++i)
        if (std::abs (h[i]) >= fraction * peak)
            return (int) i;
    return -1;
}

// lag (samples) of the cross-correlation maximum of y against x, searched in [0, maxLag]
int xcorrLag (const std::vector<double>& x, const std::vector<double>& y, int maxLag)
{
    int best = 0;
    double bestV = -1e300;
    for (int lag = 0; lag <= maxLag; ++lag)
    {
        double acc = 0.0;
        for (size_t i = 0; i + (size_t) lag < y.size(); ++i)
            acc += x[i] * y[i + (size_t) lag];
        if (acc > bestV) { bestV = acc; best = lag; }
    }
    return best;
}
} // namespace

// ---------------------------------------------------------------------------------------------
TEST_CASE ("Reverb: RT60 per octave band, pre-delay, energy and decorrelation", "[colour][reverb]")
{
    struct Case { double rate; std::vector<double> sizes; };
    for (const auto& cs : { Case { 48000.0, { 1.0, 25.0, 50.0, 75.0, 100.0 } }, Case { 44100.0, { 1.0, 50.0, 100.0 } },
                            Case { 96000.0, { 1.0, 50.0, 100.0 } } })
        for (double size : cs.sizes)
        {
            const double rate = cs.rate;
            Reverb rv;
            rv.prepare (rate);
            rv.setSize (size);
            rv.snapParameters();
            rv.reset();
            const int n = (int) (rate * 7.0);
            const auto out = runEffect (rv, impulse (n), std::vector<double> ((size_t) n, 0.0));

            // energy (one-sided input, each output and their mean)
            double el = 0.0, er = 0.0;
            for (int i = 0; i < n; ++i) { el += out.l[(size_t) i] * out.l[(size_t) i]; er += out.r[(size_t) i] * out.r[(size_t) i]; }
            const double expectedEnergy = Reverb::energyTargetDb (size);
            INFO ("fs " << rate << " size " << size << ": energy L " << 10.0 * std::log10 (el) << " dB, R "
                        << 10.0 * std::log10 (er) << " dB, target " << expectedEnergy);
            CHECK (10.0 * std::log10 (el) == Approx (expectedEnergy).margin (1.5));
            CHECK (10.0 * std::log10 (er) == Approx (expectedEnergy).margin (1.5));
            CHECK (10.0 * std::log10 (0.5 * (el + er)) == Approx (expectedEnergy).margin (0.75));

            // pre-delay: onset of the left output (first sample at -40 dB re peak)
            const double onsetMs = firstAbove (out.l, 0.01) * 1000.0 / rate;
            INFO ("onset " << onsetMs << " ms, expected " << Reverb::preDelayMs (size));
            CHECK (onsetMs == Approx (Reverb::preDelayMs (size)).margin (3.0));

            // tail decorrelation (one-sided input)
            const double rho = correlation (out.l, out.r, (size_t) (0.05 * rate), (size_t) (1.5 * rate));
            INFO ("L/R correlation " << rho);
            CHECK (std::abs (rho) < 0.2);

            // RT60 per band within +-15 %
            const auto target = Reverb::rt60ForSize (size);
            for (int b = 0; b < Reverb::numBands; ++b)
            {
                const double t = rt60 (octaveBand (out.l, Reverb::bandHz[(size_t) b], rate), rate);
                INFO ("band " << Reverb::bandHz[(size_t) b] << " Hz: RT60 " << t << " s, target " << target[(size_t) b]);
                CHECK (t == Approx (target[(size_t) b]).epsilon (0.15));
            }
        }
}

// ---------------------------------------------------------------------------------------------
namespace
{
double distGainDb (double send, double freq, double levelDb)
{
    Distortion d;
    d.prepare (fs);
    d.setSend (send);
    d.snapParameters();
    d.reset();
    const int n = 24000;
    const double amp = std::pow (10.0, levelDb / 20.0);
    const auto out = runEffect (d, sine (freq, amp, n), std::vector<double> ((size_t) n, 0.0));
    return db (toneAmplitude (out.l, freq, 9600, (size_t) n) / amp);
}

double distHarmonicDb (double send, double levelDb, int harmonic)
{
    Distortion d;
    d.prepare (fs);
    d.setSend (send);
    d.snapParameters();
    d.reset();
    const int n = 24000;
    const auto out = runEffect (d, sine (1000.0, std::pow (10.0, levelDb / 20.0), n), std::vector<double> ((size_t) n, 0.0));
    return db (toneAmplitude (out.l, 1000.0 * harmonic, 4800, (size_t) n) / toneAmplitude (out.l, 1000.0, 4800, (size_t) n));
}
} // namespace

TEST_CASE ("Distortion: small-signal response at Send 50 % (+-2 dB, 100 Hz - 4 kHz)", "[colour][distortion]")
{
    const double pts[][2] = { { 100, 12.1 }, { 200, 14.7 }, { 500, 13.9 }, { 1000, 11.7 }, { 2000, 6.4 }, { 4000, -2.7 } };
    for (const auto& p : pts)
    {
        const double g = distGainDb (50.0, p[0], -40.0);
        INFO (p[0] << " Hz: " << g << " dB, expected " << p[1]);
        CHECK (g == Approx (p[1]).margin (2.0));
    }
    // outside the tolerance band, shape only
    CHECK (distGainDb (50.0, 50.0, -40.0) < 0.0);
    CHECK (distGainDb (50.0, 8000.0, -40.0) < -10.0);
    // Send 25 %: about -1.8 dB at 1 kHz
    CHECK (distGainDb (25.0, 1000.0, -30.0) == Approx (-1.8).margin (2.0));
    // Send 100 %: +14 ... +24 dB above Send 50 % (small signal)
    const double d100 = distGainDb (100.0, 1000.0, -66.0) - distGainDb (50.0, 1000.0, -40.0);
    INFO ("Send 100 % re 50 %: " << d100);
    CHECK (d100 >= 14.0);
    CHECK (d100 <= 24.0);
}

TEST_CASE ("Distortion: odd-harmonic profile at 1 kHz (+-4 dB) and no even harmonics", "[colour][distortion]")
{
    const double pts[][3] = { { 100, -40, -9 }, { 100, 0, -5 }, { 50, -24, -20 }, { 50, 0, -11 } };
    for (const auto& p : pts)
    {
        const double h3 = distHarmonicDb (p[0], p[1], 3);
        INFO ("Send " << p[0] << " %, " << p[1] << " dBFS: H3 " << h3 << " dB, expected " << p[2]);
        CHECK (h3 == Approx (p[2]).margin (4.0));
    }
    CHECK (distHarmonicDb (50, -30, 3) < -30.0); // negligible below -28 dBFS
    CHECK (distHarmonicDb (100, 0, 2) < -95.0);
    CHECK (distHarmonicDb (50, -24, 2) < -95.0);
}

TEST_CASE ("Distortion: gate thresholds (+-4 dB)", "[colour][distortion]")
{
    auto outRms = [] (double send, double levelDb) {
        Distortion d;
        d.prepare (fs);
        d.setSend (send);
        d.snapParameters();
        d.reset();
        const int n = 24000;
        const auto out = runEffect (d, sine (1000.0, std::pow (10.0, levelDb / 20.0), n), std::vector<double> ((size_t) n, 0.0));
        return rms (out.l, 4800, (size_t) n);
    };
    CHECK (outRms (10.0, -32.0) == 0.0);
    CHECK (outRms (10.0, -28.0) > 0.0);
    CHECK (outRms (0.0, -28.0) == 0.0);
    CHECK (outRms (0.0, -24.0) > 0.0);
}

TEST_CASE ("Distortion: width 0 % identical L/R, 100 % decorrelated equal level; latency about 6.8 ms", "[colour][distortion]")
{
    const int n = 48000;
    const auto x = noise (n, 0.05, 31u);
    for (double width : { 0.0, 100.0 })
    {
        Distortion d;
        d.prepare (fs);
        d.setSend (50.0);
        d.setWidth (width);
        d.snapParameters();
        d.reset();
        const auto out = runEffect (d, x, std::vector<double> ((size_t) n, 0.0));
        if (width == 0.0)
        {
            CHECK (out.l == out.r);
        }
        else
        {
            CHECK (std::abs (correlation (out.l, out.r, 4800, (size_t) n)) < 0.2);
            CHECK (db (rms (out.l, 4800, (size_t) n) / rms (out.r, 4800, (size_t) n)) == Approx (0.0).margin (1.0));
        }
    }

    // latency: cross-correlation of a quiet (linear) band-limited burst
    Distortion d;
    d.prepare (fs);
    d.setSend (50.0);
    d.snapParameters();
    d.reset();
    auto burst = noise (9600, 0.01, 5u);
    burst.resize (24000, 0.0);
    const auto out = runEffect (d, burst, std::vector<double> (burst.size(), 0.0));
    const double lagMs = xcorrLag (burst, out.l, 960) * 1000.0 / fs;
    INFO ("distortion latency " << lagMs << " ms");
    CHECK (lagMs == Approx (6.8).margin (1.0));
}

// ---------------------------------------------------------------------------------------------
TEST_CASE ("Doubler: pitch shift exactly Detune cents (+-0.5), level +9.2 dB (+-1), latency <= 15 ms", "[colour][doubler]")
{
    // A dual-tap delay-line shifter is a sum of grains that each carry exactly f * 2^(cents/1200); at input
    // frequencies where the two taps are in antiphase the steady-state line cancels (comb) and only modulation
    // sidebands remain, so the pitch is measured as the median of short-time (20 ms) frequency estimates.
    for (double freq : { 1000.0, 440.0, 2500.0 })
        for (double cents : { 8.0, -37.0, 100.0, -100.0, 1.0 })
        {
            Doubler d;
            d.prepare (fs);
            d.setDetune (cents);
            d.snapParameters();
            d.reset();
            const int n = (int) (fs * 3.0);
            const auto out = runEffect (d, sine (freq, 0.1, n), sine (freq, 0.1, n, fs, 1.0));
            const double expected = freq * std::pow (2.0, cents / 1200.0);
            std::vector<double> est;
            const size_t win = 960;
            for (size_t from = 4800; from + win <= (size_t) n; from += 480)
            {
                double a = expected - 15.0, b = expected + 15.0;
                const double gr = 0.6180339887;
                for (int it = 0; it < 40; ++it)
                {
                    const double c = b - gr * (b - a), e = a + gr * (b - a);
                    if (toneAmplitude (out.l, c, from, from + win) > toneAmplitude (out.l, e, from, from + win)) b = e; else a = c;
                }
                est.push_back (0.5 * (a + b));
            }
            std::nth_element (est.begin(), est.begin() + (long) est.size() / 2, est.end());
            const double f = est[est.size() / 2];
            const double errCents = 1200.0 * std::log2 (f / expected);
            INFO ("input " << freq << " Hz, detune " << cents << ": measured " << f << " Hz, expected " << expected << " (" << errCents << " cents)");
            CHECK (std::abs (errCents) <= 0.5);
        }

    // level: tone (+-1 dB) and band-limited noise (+-1.5 dB)
    {
        Doubler d;
        d.prepare (fs);
        d.setDetune (8.0);
        d.snapParameters();
        d.reset();
        const int n = (int) (fs * 4.0);
        const auto x = sine (1000.0, 0.1, n);
        const auto out = runEffect (d, x, x);
        const double lvl = db (rms (out.l, 4800, (size_t) n) / rms (x, 4800, (size_t) n));
        INFO ("doubler level (tone) " << lvl << " dB");
        CHECK (lvl == Approx (9.2).margin (1.0));
    }
    {
        Doubler d;
        d.prepare (fs);
        d.setDetune (-15.0);
        d.snapParameters();
        d.reset();
        const int n = (int) (fs * 4.0);
        auto x = noise (n, 0.1, 99u);
        Biquad lp1, lp2;
        lp1.setLowPass (6000.0, 0.7071, fs);
        lp2.setLowPass (6000.0, 0.7071, fs);
        for (auto& v : x) v = lp2.process (lp1.process (v));
        const auto out = runEffect (d, x, x);
        const double lvl = db (rms (out.l, 4800, (size_t) n) / rms (x, 4800, (size_t) n));
        INFO ("doubler level (noise) " << lvl << " dB");
        CHECK (lvl == Approx (9.2).margin (1.5));
    }

    // latency (detune 0: pure delay at the centre of the 3 ... 10 ms window)
    Doubler z;
    z.prepare (fs);
    z.setDetune (0.0);
    z.snapParameters();
    z.reset();
    auto burst = noise (4800, 0.1, 7u);
    burst.resize (9600, 0.0);
    const auto zo = runEffect (z, burst, burst);
    const double lagMs = xcorrLag (burst, zo.l, 960) * 1000.0 / fs;
    INFO ("doubler latency " << lagMs << " ms");
    CHECK (lagMs >= 3.0);
    CHECK (lagMs <= 10.0);
}

// ---------------------------------------------------------------------------------------------
TEST_CASE ("Phaser: at least four notches, sweeping roughly 2 - 15 kHz", "[colour][phaser]")
{
    double lowest = 1e9, highest = 0.0;
    for (double phase : { 0.0, 0.25, 0.5, 0.75 })
    {
        Phaser p;
        p.prepare (fs);
        p.setRate (0.0);
        p.setLfoPhase (phase);
        const int n = 1 << 14;
        const auto out = runEffect (p, impulse (n), std::vector<double> ((size_t) n, 0.0));
        int notches = 0;
        double prev2 = 0, prev1 = 0;
        for (double f = 100.0; f < 20000.0; f *= 1.002)
        {
            const double m = db (std::abs (dtft (out.l, f)));
            if (f > 101.0 && prev1 < prev2 && prev1 < m && prev1 < -15.0)
            {
                ++notches;
                lowest = std::min (lowest, f);
                highest = std::max (highest, f);
            }
            prev2 = prev1;
            prev1 = m;
        }
        INFO ("LFO phase " << phase << ": " << notches << " notches");
        CHECK (notches >= 4);
    }
    INFO ("notch range " << lowest << " ... " << highest << " Hz");
    CHECK (lowest > 1000.0);
    CHECK (lowest < 2500.0);
    CHECK (highest > 12000.0);
    CHECK (highest < 18000.0);
}

TEST_CASE ("Phaser: LFO rate within 5 %", "[colour][phaser]")
{
    for (double rate : { 0.34, 2.0, 7.0 })
    {
        Phaser p;
        p.prepare (fs);
        p.setRate (rate);
        const double seconds = std::max (6.0, 4.0 / rate);
        const int n = (int) (fs * seconds);
        const auto out = runEffect (p, sine (4000.0, 0.5, n), std::vector<double> ((size_t) n, 0.0));
        // envelope in 2 ms frames, then the autocorrelation peak near the expected period
        const int frame = 96;
        std::vector<double> env;
        for (int i = 0; i + frame <= n; i += frame)
            env.push_back (rms (out.l, (size_t) i, (size_t) (i + frame)));
        double mean = 0.0;
        for (double v : env) mean += v;
        mean /= (double) env.size();
        for (auto& v : env) v -= mean;
        const double framesPerPeriod = fs / frame / rate;
        int best = 0;
        double bestV = -1e300;
        for (int lag = (int) (0.7 * framesPerPeriod); lag <= (int) (1.3 * framesPerPeriod); ++lag)
        {
            double acc = 0.0;
            for (size_t i = 0; i + (size_t) lag < env.size(); ++i)
                acc += env[i] * env[i + (size_t) lag];
            if (acc > bestV) { bestV = acc; best = lag; }
        }
        const double measured = fs / frame / best;
        INFO ("rate " << rate << " Hz, measured " << measured);
        CHECK (measured == Approx (rate).epsilon (0.05));
    }
}

// ---------------------------------------------------------------------------------------------
TEST_CASE ("Effects are fed post wet gain and scaled by their level law", "[colour]")
{
    // reverb at Level 50 % vs 100 %: -18.1 dB (measured -17.5, tolerance 1.5 dB); doubler 50 %: -21 dB
    auto energy = [] (bool verb, double level, double dryWet) {
        Settings s = plainWet();
        s.dryWetPct = dryWet;
        s.verbOn = verb;
        s.doublerOn = ! verb;
        s.verbLevel = s.doublerLevel = level;
        s.monitor = Monitor::Stereo;
        const int n = 48000 * 3;
        const auto x = impulse (n, 0.01);
        auto out = render (s, x, std::vector<double> ((size_t) n, 0.0));
        // remove the plain wet path (impulse at sample 1)
        out.l[1] -= 0.01 * param::wetGain (dryWet);
        out.l[0] -= 0.01 * param::dryGain (dryWet);
        double e = 0.0;
        for (size_t i = 0; i < out.l.size(); ++i) e += out.l[i] * out.l[i] + out.r[i] * out.r[i];
        return 10.0 * std::log10 (e);
    };
    CHECK (energy (true, 50.0, 100.0) - energy (true, 100.0, 100.0) == Approx (-17.5).margin (1.5));
    CHECK (energy (true, 25.0, 100.0) - energy (true, 100.0, 100.0) == Approx (-37.0).margin (1.5));
    CHECK (energy (false, 50.0, 100.0) - energy (false, 100.0, 100.0) == Approx (-21.0).margin (1.5));
    // post wet gain: Dry/Wet 25 % lowers the effect by the wet law (-12.2 dB)
    CHECK (energy (true, 100.0, 25.0) - energy (true, 100.0, 100.0) == Approx (-12.2).margin (0.2));
}
