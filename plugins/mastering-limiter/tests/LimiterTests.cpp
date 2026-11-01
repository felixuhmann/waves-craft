// Limiter path: spec r1 sections 3, 4 and the section 9 test vectors (requantiser transparent at 24 bit).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <random>

#include "TestHelpers.h"

using namespace mltest;
using Catch::Approx;

namespace
{
// Peak deviation of a from b relative to the peak of b, in dB, ignoring the first `skip` samples.
double residualDb (const std::vector<double>& a, const std::vector<double>& b, size_t skip)
{
    double err = 0.0, peak = 0.0;
    for (size_t i = skip; i < a.size(); ++i)
    {
        err = std::max (err, std::abs (a[i] - b[i]));
        peak = std::max (peak, std::abs (b[i]));
    }
    return err > 0.0 ? 20.0 * std::log10 (err / peak) : -400.0;
}

// Engine output y[t] = v[t - L]: shift the reference model by L samples.
std::vector<double> delayed (const std::vector<double>& v, int L)
{
    std::vector<double> d (v.size(), 0.0);
    for (size_t i = size_t (L); i < v.size(); ++i)
        d[i] = v[i - size_t (L)];
    return d;
}

// Synthetic noise program: band-limited noise with a slowly varying level and occasional bursts.
std::vector<std::vector<double>> noiseProgram (double fs, double seconds, double peakDb, unsigned seed)
{
    std::mt19937 rng (seed);
    std::normal_distribution<double> nd;
    const size_t n = size_t (fs * seconds);
    std::vector<std::vector<double>> x (2, std::vector<double> (n));
    double lp[2] = { 0.0, 0.0 };
    for (size_t i = 0; i < n; ++i)
    {
        const double t = double (i) / fs;
        const double env = dbToLin (peakDb - 20.0 + 10.0 * std::sin (2.0 * pi * 0.7 * t)
                                    + ((i / size_t (fs * 0.05)) % 7 == 3 ? 12.0 : 0.0));
        for (int ch = 0; ch < 2; ++ch)
        {
            lp[ch] = 0.6 * lp[ch] + 0.4 * nd (rng);
            x[size_t (ch)][i] = env * lp[ch];
        }
    }
    return x;
}
} // namespace

TEST_CASE ("Latency and boxcar lengths per sample rate (spec 3, 4.5)")
{
    struct Row { double fs; int L, A, B; };
    for (const Row r : { Row { 44100, 64, 38, 27 }, Row { 48000, 64, 38, 27 }, Row { 88200, 128, 75, 54 },
                         Row { 96000, 128, 75, 54 }, Row { 176400, 256, 150, 107 }, Row { 192000, 256, 150, 107 } })
    {
        const auto c = dsp::latencyConfigFor (r.fs);
        CHECK (c.latency == r.L);
        CHECK (c.boxcarA == r.A);
        CHECK (c.boxcarB == r.B);
        CHECK (c.boxcarA + c.boxcarB - 1 == c.latency);
        dsp::MasteringEngine e;
        e.prepare (r.fs);
        CHECK (e.getLatencySamples() == r.L);
    }
}

TEST_CASE ("Neutral settings: an impulse passes bit-exact, delayed by L (spec 9)")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0, 192000.0);
    const int L = dsp::latencyConfigFor (fs).latency;
    std::vector<double> x (4096, 0.0);
    x[1000] = 0.25;
    const auto y = runMono (fs, plainSettings(), x, 333);
    for (size_t i = 0; i < y.size(); ++i)
        REQUIRE (y[i] == (i == size_t (1000 + L) ? 0.25 : 0.0));

    // also a full-scale-safe program without overs at Threshold = Ceiling = 0 (24-bit grid input)
    std::vector<double> p (20000);
    for (size_t i = 0; i < p.size(); ++i)
        p[i] = std::round (0.9 * std::sin (0.013 * double (i)) * 8388608.0) / 8388608.0;
    const auto yp = runMono (fs, plainSettings(), p);
    for (size_t i = size_t (L); i < p.size(); ++i)
        REQUIRE (yp[i] == p[i - size_t (L)]);
}

TEST_CASE ("Static curve: 1 kHz staircase, Threshold -12, Ceiling 0 (spec 9)")
{
    const double fs = 48000.0;
    const int L = 64;
    std::vector<double> x;
    std::vector<double> stepLevel;
    for (double db = -40.0; db <= 12.0 + 1e-9; db += 2.0)
        for (int i = 0; i < 9600; ++i)
        {
            x.push_back (dbToLin (db) * std::sin (2.0 * pi * 1000.0 * double (x.size()) / fs));
            stepLevel.push_back (db);
        }
    const auto y = runMono (fs, plainSettings (-12.0, 0.0, 10.0), x);
    const double mk = dbToLin (12.0);
    double maxOut = 0.0;
    for (size_t i = size_t (L); i < y.size(); ++i)
    {
        maxOut = std::max (maxOut, std::abs (y[i]));
        const size_t n = i - size_t (L);
        // below threshold (allow 0.2 s of release after the last over): output = input x 12 dB
        if (stepLevel[n] < -12.0)
            REQUIRE (y[i] == Approx (x[n] * mk).margin (1.2e-7));
    }
    CHECK (maxOut <= 1.0);
    CHECK (maxOut > 0.999);

    // whole signal against the literal spec model
    const auto ref = referenceLimiter ({ x }, fs, -12.0, 0.0, 10.0)[0];
    CHECK (residualDb (y, delayed (ref, L), 2 * size_t (L)) < -100.0);
}

TEST_CASE ("Threshold -12, Ceiling -3: +9 dB below threshold, peaks at the ceiling (spec 9)")
{
    const double fs = 48000.0;
    std::vector<double> x (96000);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = (i < 48000 ? 0.05 : 0.9) * std::sin (2.0 * pi * 997.0 * double (i) / fs);
    const auto y = runMono (fs, plainSettings (-12.0, -3.0, 5.0), x);
    for (size_t i = 1000; i < 40000; ++i)
        REQUIRE (y[i + 64] == Approx (x[i] * dbToLin (9.0)).margin (1.2e-7));
    double peak = 0.0;
    for (double v : y)
        peak = std::max (peak, std::abs (v));
    CHECK (peak == Approx (dbToLin (-3.0)).margin (1.2e-7));
    CHECK (peak <= dbToLin (-3.0) + std::ldexp (1.0, -23));
}

TEST_CASE ("Click on a DC carrier: look-ahead start, S-shaped attack (spec 9)")
{
    const double fs = 48000.0;
    const size_t n0 = 5000;
    std::vector<double> peaks (12000, 0.01);
    peaks[n0] = 1.0;
    const auto g = gainTrajectory (fs, peaks, -12.0, 10.0, false);
    const double T = dbToLin (-12.0);

    CHECK (g[n0 - 65] == 1.0);
    CHECK (g[n0 - 64] < 1.0);                         // falls exactly 64 samples before the click
    CHECK (g[n0 - 1] == Approx (T).epsilon (1e-12)); // reaches 0.2512 one sample before it
    CHECK (g[n0] == Approx (T).epsilon (1e-12));
    CHECK (T == Approx (0.2512).margin (5e-5));

    // monotonic, S-shaped: slow start, fastest around the middle, slow end
    for (size_t i = n0 - 64; i < n0 - 1; ++i)
        REQUIRE (g[i + 1] <= g[i]);
    const double first = g[n0 - 65] - g[n0 - 64], mid = g[n0 - 33] - g[n0 - 32], last = g[n0 - 2] - g[n0 - 1];
    CHECK (mid > 5.0 * first);
    CHECK (mid > 5.0 * last);
    // exact two-boxcar shape: the attack follows the convolution of 38- and 27-tap rectangles
    double conv = 0.0;
    for (int k = 0; k < 64; ++k)
    {
        // number of (a, b) pairs with a + b == k, a < 38, b < 27
        int pairs = 0;
        for (int a = 0; a < 38; ++a)
            if (k - a >= 0 && k - a < 27)
                ++pairs;
        conv += pairs / (38.0 * 27.0);
        REQUIRE (g[n0 - 64 + size_t (k)] == Approx (1.0 - (1.0 - T) * conv).margin (1e-12));
    }
}

TEST_CASE ("Two clicks: recovery after the first targets the second's required gain (spec 4.4, 9)")
{
    const double fs = 48000.0;
    const size_t n1 = 4000, n2 = n1 + 64;
    std::vector<double> peaks (10000, 0.01);
    peaks[n1] = 2.0;
    peaks[n2] = 1.0;
    const double T = dbToLin (-12.0);
    const auto g = gainTrajectory (fs, peaks, -12.0, 10.0, false);
    CHECK (g[n1] == Approx (T / 2.0).epsilon (1e-12));
    CHECK (g[n2] <= T); // still smoothing up from the first click's gain
    for (size_t i = n1; i <= n2; ++i)
    {
        REQUIRE (g[i] <= T + 1e-12);         // never recovers past the second click's requirement
        REQUIRE (g[i] >= T / 2.0 - 1e-12);
    }
    CHECK (g[n2 - 1] > g[n1] + 0.01); // but it does move towards it

    std::vector<double> refGain;
    referenceLimiter ({ peaks }, fs, -12.0, 0.0, 10.0, &refGain);
    for (size_t i = 0; i < g.size(); ++i)
        REQUIRE (g[i] == Approx (refGain[i]).margin (1e-12));
}

TEST_CASE ("Sample-peak detection only: fs/4 sines (spec 4.1, 9)")
{
    const double fs = 48000.0;
    // amplitude 1.5 at 45 degrees: sample peaks 1.0607 -> output sample peaks exactly 1.0 at Threshold 0
    std::vector<double> x (48000), quiet (48000);
    for (size_t i = 0; i < x.size(); ++i)
    {
        x[i] = 1.5 * std::sin (pi / 2.0 * double (i) + pi / 4.0);
        quiet[i] = std::sin (pi / 2.0 * double (i) + pi / 4.0); // sample peaks 0.707, true peak 1.0
    }
    const auto y = runMono (fs, plainSettings (0.0, 0.0, 1.0, true), x);
    double peak = 0.0;
    for (size_t i = 64; i < y.size(); ++i)
        peak = std::max (peak, std::abs (y[i]));
    CHECK (peak == 1.0);
    for (size_t i = 2000; i < y.size(); ++i)
        REQUIRE (std::abs (y[i]) == Approx (1.0).margin (1e-6));

    // quantise the quiet input to 24 bit first so the transparency check is exact
    for (auto& v : quiet)
        v = std::round (v * 8388608.0) / 8388608.0;
    const auto yq = runMono (fs, plainSettings (0.0, 0.0, 1.0, true), quiet);
    for (size_t i = 64; i < yq.size(); ++i)
        REQUIRE (yq[i] == quiet[i - 64]);
}

TEST_CASE ("Fixed release: recovery after a 0.5 s DC overload, 12 dB over (spec 9)")
{
    struct Row { double releaseMs, t63, t90, t99; };
    const Row rows[] = { { 0.01, 0.75, 1.02, 1.21 }, { 1.0, 1.15, 1.71, 2.71 }, { 10.0, 4.98, 10.62, 20.62 },
                         { 100.0, 44.1, 100.6, 200.6 }, { 1000.0, 435.0, 1000.0, 2001.0 } };
    // the table is measured at 48 kHz; 96 kHz uses boxcars of the same duration (75 / 54 samples)
    const double fs = GENERATE (48000.0, 96000.0);
    for (const auto& row : rows)
    {
        CAPTURE (fs, row.releaseMs);
        const double T = dbToLin (-12.0);
        const size_t pre = size_t (0.1 * fs), over = size_t (0.5 * fs), post = size_t (2.5 * fs);
        std::vector<double> peaks (pre + over + post, 0.1);
        for (size_t i = pre; i < pre + over; ++i)
            peaks[i] = 4.0 * T; // exactly 12 dB over the threshold
        const auto g = gainTrajectory (fs, peaks, -12.0, row.releaseMs, false);
        const double d0 = 1.0 - 0.25;
        const size_t end = pre + over;
        const double t63 = recoveryTime (g, end, d0, std::exp (-1.0), fs) * 1e3;
        const double t90 = recoveryTime (g, end, d0, 0.1, fs) * 1e3;
        const double t99 = recoveryTime (g, end, d0, 0.01, fs) * 1e3;
        // the table is given to 0.01 ms (= 0.5 sample at 48 kHz) and three significant digits
        auto tol = [] (double t) { return std::max (0.03, 0.006 * t); };
        CHECK (t63 == Approx (row.t63).margin (tol (row.t63)));
        CHECK (t90 == Approx (row.t90).margin (tol (row.t90)));
        CHECK (t99 == Approx (row.t99).margin (tol (row.t99)));
    }
}

TEST_CASE ("Engine matches the literal spec model on a noise program (spec 10)")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    struct Cfg { double th, ce, rel; double limitDb; };
    for (const Cfg c : { Cfg { -12.0, 0.0, 10.0, -100.0 }, Cfg { -6.0, -0.3, 1.0, -100.0 }, Cfg { -20.0, -1.0, 0.05, -100.0 },
                         Cfg { -3.0, -3.0, 100.0, -100.0 }, Cfg { -12.0, -0.1, 500.0, -85.0 } })
    {
        CAPTURE (fs, c.th, c.ce, c.rel);
        const auto x = noiseProgram (fs, 1.5, 6.0, 42u);
        dsp::MasteringEngine e;
        e.setSettings (plainSettings (c.th, c.ce, c.rel));
        e.prepare (fs);
        const auto y = runEngine (e, x, 480);
        const auto ref = referenceLimiter (x, fs, c.th, c.ce, c.rel);
        const int L = e.getLatencySamples();
        for (int ch = 0; ch < 2; ++ch)
            CHECK (residualDb (y[size_t (ch)], delayed (ref[size_t (ch)], L), size_t (2 * L)) < c.limitDb);
    }
}

TEST_CASE ("Ceiling guarantee on hot material, every mode (spec 10)")
{
    const double fs = 48000.0;
    const auto x = noiseProgram (fs, 2.0, 18.0, 7u);
    for (double ce : { 0.0, -0.1, -1.0, -6.5 })
        for (bool adaptive : { false, true })
            for (int dither = 0; dither < 3; ++dither)
                for (int bits : { 24, 16 })
                {
                    auto s = plainSettings (-9.0, ce, 3.0, adaptive);
                    s.dither = dsp::Dither (dither);
                    s.shaping = dither == 0 ? dsp::Shaping::None : dsp::Shaping::Ultra;
                    s.wordLength = bits;
                    dsp::MasteringEngine e;
                    e.setSettings (s);
                    e.prepare (fs);
                    const auto y = runEngine (e, x);
                    const double lsb = std::ldexp (1.0, 1 - bits);
                    // requantisation error bound: 1/2 LSB rounding + dither peak, shaped by the NTF (sum |h|)
                    double hsum = 0.0;
                    for (int k = 0; k < dsp::ntf::numTaps; ++k)
                        hsum += std::abs (dsp::ntf::coefficients[1][2][k]);
                    const double bound = dbToLin (ce) + (dither == 0 ? 0.5 * lsb : (1.0 + 0.5) * lsb * hsum);
                    double peak = 0.0;
                    for (const auto& ch : y)
                        for (double v : ch)
                            peak = std::max (peak, std::abs (v));
                    CAPTURE (ce, adaptive, dither, bits);
                    CHECK (peak <= bound);
                    CHECK (peak <= 1.0);
                }
}

TEST_CASE ("Ceiling enforced exactly without requantisation error (spec 4.6)")
{
    // with a 24-bit transparent requantiser the limiter output never exceeds the ceiling by more than 1/2 LSB
    const double fs = 96000.0;
    const auto x = noiseProgram (fs, 1.0, 24.0, 99u);
    for (double ce : { 0.0, -0.5, -12.0 })
    {
        dsp::MasteringEngine e;
        e.setSettings (plainSettings (-24.0, ce, 0.2, false));
        e.prepare (fs);
        const auto y = runEngine (e, x, 64);
        for (const auto& ch : y)
            for (double v : ch)
                REQUIRE (std::abs (v) <= dbToLin (ce) + std::ldexp (1.0, -24));
    }
}

TEST_CASE ("Threshold and Ceiling changes are smoothed (spec 8)")
{
    const double fs = 48000.0;
    dsp::MasteringEngine e;
    e.setSettings (plainSettings (0.0, 0.0, 10.0));
    e.prepare (fs);
    std::vector<double> x (48000, 0.1); // DC well below every threshold
    double* p[1] = { x.data() };
    e.process (p, 1, 24000);
    auto s = plainSettings (-18.0, 0.0, 10.0); // make-up +18 dB
    e.setSettings (s);
    double* p2[1] = { x.data() + 24000 };
    e.process (p2, 1, 24000);
    // output moves from 0.1 to 0.1 * 7.94 over about 20 ms (plus latency), never in one jump
    double maxStep = 0.0;
    for (size_t i = 24000; i < 48000; ++i)
        maxStep = std::max (maxStep, std::abs (x[i] - x[i - 1]));
    CHECK (maxStep < 0.01);
    CHECK (x[47999] == Approx (0.1 * dbToLin (18.0)).margin (2e-7));
    CHECK (x[24000 + 64 + 480] > 0.2);
    CHECK (x[24000 + 64 + 480] < 0.7);
}

TEST_CASE ("Bypass: output is the input delayed by L after a short fade (latency stays constant)")
{
    const double fs = 48000.0;
    dsp::MasteringEngine e;
    e.setSettings (plainSettings (-20.0, 0.0, 1.0));
    e.prepare (fs);
    e.setBypassed (true);
    e.reset();
    std::vector<double> x (10000);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 0.9 * std::sin (0.05 * double (i));
    const auto y = runEngine (e, { x })[0];
    for (size_t i = 64; i < y.size(); ++i)
        REQUIRE (y[i] == x[i - 64]);
}

TEST_CASE ("Stereo detection is linked; mono works on one channel (spec 1, 4.1)")
{
    const double fs = 48000.0;
    std::vector<std::vector<double>> x (2, std::vector<double> (9600, 0.0));
    for (size_t i = 0; i < 9600; ++i)
    {
        x[0][i] = 0.1 * std::sin (0.03 * double (i));
        x[1][i] = (i > 4000 && i < 4100) ? 0.8 : 0.05;
    }
    dsp::MasteringEngine e;
    e.setSettings (plainSettings (-6.0, -6.0, 1.0));
    e.prepare (fs);
    const auto y = runEngine (e, x);
    // the left channel is attenuated by the right channel's over (common gain)
    const double gL = y[0][4050 + 64] / x[0][4050];
    const double gR = y[1][4050 + 64] / x[1][4050];
    CHECK (gL == Approx (gR).epsilon (1e-5));
    CHECK (gR == Approx (dbToLin (-6.0) / 0.8).epsilon (1e-5));
}
