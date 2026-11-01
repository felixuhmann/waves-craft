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

// radix-2 FFT (in place)
void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -twoPi / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// |L(f)|^2 + |R(f)|^2 for 0 .. Nyquist
std::vector<double> powerSpectrum (const std::vector<double>& l, const std::vector<double>& r)
{
    size_t n = 1;
    while (n < l.size()) n <<= 1;
    std::vector<std::complex<double>> a (n), b (n);
    for (size_t i = 0; i < l.size(); ++i) { a[i] = l[i]; b[i] = r[i]; }
    fft (a);
    fft (b);
    std::vector<double> p (n / 2);
    for (size_t i = 0; i < n / 2; ++i) p[i] = std::norm (a[i]) + std::norm (b[i]);
    return p;
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
TEST_CASE ("Reverb: RT60 per octave band, pre-delay, energy, octave-band energy profile, decorrelation", "[colour][reverb]")
{
    struct Profile { double size; double rel[8]; }; // 63, 125, 250, 500, 2k, 4k, 8k, 16k re 1 kHz (spec r2 7.1)
    const Profile profiles[] = { { 1.0, { -1.0, 0.5, -0.7, -0.2, -2.2, -4.3, -6.5, -9.3 } },
                                 { 50.0, { 0.1, 0.8, -0.6, 0.2, -1.8, -3.5, -5.4, -7.8 } },
                                 { 100.0, { -0.1, 1.0, 0.3, 0.4, -1.3, -3.3, -5.1, -7.2 } } };
    const double profileHz[8] = { 63.0, 125.0, 250.0, 500.0, 2000.0, 4000.0, 8000.0, 16000.0 };

    for (double rate : { 48000.0, 44100.0, 96000.0 })
        for (double size : { 1.0, 25.0, 50.0, 75.0, 100.0 })
        {
            Reverb rv;
            rv.prepare (rate);
            rv.setSize (size);
            rv.snapParameters();
            rv.reset();
            const int n = (int) (rate * 7.0);
            const auto out = runEffect (rv, impulse (n), std::vector<double> ((size_t) n, 0.0));

            // energy E = sum(hL^2 + hR^2) for a left-only impulse
            double el = 0.0, er = 0.0;
            for (int i = 0; i < n; ++i) { el += out.l[(size_t) i] * out.l[(size_t) i]; er += out.r[(size_t) i] * out.r[(size_t) i]; }
            const double e = 10.0 * std::log10 (el + er);
            INFO ("fs " << rate << " size " << size << ": E " << e << " dB, target " << Reverb::energyTargetDb (size)
                        << " (L " << 10.0 * std::log10 (el) << ", R " << 10.0 * std::log10 (er) << ")");
            CHECK (e == Approx (Reverb::energyTargetDb (size)).margin (1.5));

            // octave-band energy profile of E re the 1 kHz band (+-2 dB), as mean power density per band
            for (const auto& pr : profiles)
                if (pr.size == size)
                {
                    // mean power density of E in each octave band (FFT of the impulse responses)
                    const auto P = powerSpectrum (out.l, out.r);
                    const double df = rate / (2.0 * (double) P.size());
                    auto bandE = [&] (double fc) {
                        double acc = 0.0;
                        int cnt = 0;
                        for (size_t i = (size_t) (fc / std::sqrt (2.0) / df); i < P.size() && (double) i * df < fc * std::sqrt (2.0); ++i)
                        {
                            acc += P[i];
                            ++cnt;
                        }
                        return 10.0 * std::log10 (acc / cnt);
                    };
                    const double ref = bandE (1000.0);
                    for (int b = 0; b < 8; ++b)
                    {
                        if (profileHz[b] > 0.4 * rate) continue;
                        const double rel = bandE (profileHz[b]) - ref;
                        INFO ("band " << profileHz[b] << " Hz: " << rel << " dB re 1 kHz, target " << pr.rel[b]);
                        CHECK (rel == Approx (pr.rel[b]).margin (2.0));
                    }
                }

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
// Distortion with a tone on the left only (or on both channels); returns {left gain, right gain} in dB re input.
std::pair<double, double> distGain (double rate, double send, double freq, double levelDb, bool both = false, double width = 0.0)
{
    Distortion d;
    d.prepare (rate);
    d.setSend (send);
    d.setWidth (width);
    d.snapParameters();
    d.reset();
    const int n = (int) (rate * 0.5);
    const double amp = std::pow (10.0, levelDb / 20.0);
    const auto x = sine (freq, amp, n, rate);
    const auto out = runEffect (d, x, both ? x : std::vector<double> ((size_t) n, 0.0));
    const size_t from = (size_t) (rate * 0.2);
    return { db (toneAmplitude (out.l, freq, from, (size_t) n, rate) / amp), db (toneAmplitude (out.r, freq, from, (size_t) n, rate) / amp) };
}

double distHarmonicDb (double rate, double send, double levelDb, int harmonic)
{
    Distortion d;
    d.prepare (rate);
    d.setSend (send);
    d.snapParameters();
    d.reset();
    const int n = (int) (rate * 0.5);
    const auto out = runEffect (d, sine (1000.0, std::pow (10.0, levelDb / 20.0), n, rate), std::vector<double> ((size_t) n, 0.0));
    const size_t from = (size_t) (rate * 0.1);
    return db (toneAmplitude (out.l, 1000.0 * harmonic, from, (size_t) n, rate) / toneAmplitude (out.l, 1000.0, from, (size_t) n, rate));
}

const double rates[] = { 48000.0, 44100.0, 96000.0 };
} // namespace

namespace
{
// End-to-end measurement as in spec r3 7.2: Delay 100 ms, Feedback 0, Dry/Wet 100 %, effect at Level 100 %, effect
// output isolated by subtracting the same render with the effect off. Returns {left, right} gain (dB re input) of the
// tone at `freq` (or of harmonic `harmonic` re the fundamental when harmonic > 1).
std::pair<double, double> e2e (double rate, bool dist, double param, double freq, double levelDb, bool both = true,
                               int harmonic = 1)
{
    Settings s;
    s.hostSync = false;
    s.delayMs = { 100.0, 100.0 };
    s.feedbackPct = { 0.0, 0.0 };
    s.dryWetPct = 100.0;
    if (dist)
    {
        s.distOn = true;
        s.distLevel = 100.0;
        s.distSend = param;
    }
    else
    {
        s.doublerOn = true;
        s.doublerLevel = 100.0;
        s.doublerCents = param;
    }
    Settings off = s;
    off.distOn = off.doublerOn = false;
    const int n = (int) (rate * (dist ? 0.8 : 3.0));
    const double amp = std::pow (10.0, levelDb / 20.0);
    const auto x = sine (freq, amp, n, rate);
    const std::vector<double> z ((size_t) n, 0.0);
    const auto a = render (s, x, both ? x : z, rate), b = render (off, x, both ? x : z, rate);
    std::vector<double> l ((size_t) n), r ((size_t) n);
    for (size_t i = 0; i < l.size(); ++i) { l[i] = a.l[i] - b.l[i]; r[i] = a.r[i] - b.r[i]; }
    const size_t from = (size_t) (rate * (dist ? 0.4 : 0.6));
    const double fOut = dist ? freq : freq * std::pow (2.0, param / 1200.0);
    if (harmonic > 1)
        return { db (toneAmplitude (l, fOut * harmonic, from, (size_t) n, rate) / toneAmplitude (l, fOut, from, (size_t) n, rate)), 0.0 };
    return { db (toneAmplitude (l, fOut, from, (size_t) n, rate) / amp), db (toneAmplitude (r, fOut, from, (size_t) n, rate) / amp) };
}
} // namespace

TEST_CASE ("Distortion end-to-end: small-signal response at Send 25 / 50 / 75 / 100 % (spec r3 7.2)", "[colour][distortion]")
{
    struct Row { double send, tol; std::vector<std::pair<double, double>> pts; };
    const Row rows[] = {
        { 50.0, 2.0, { { 100, 12.1 }, { 200, 14.7 }, { 500, 13.9 }, { 1000, 11.7 }, { 2000, 6.4 }, { 4000, -2.7 } } },
        { 75.0, 3.0, { { 100, 23.2 }, { 1000, 24.4 }, { 4000, 10.8 } } },
        { 100.0, 3.0, { { 100, 26.1 }, { 1000, 28.7 }, { 2000, 30.8 }, { 4000, 24.3 }, { 8000, 11.3 } } },
        { 25.0, 3.0, { { 100, -5.7 }, { 1000, -1.7 }, { 2000, -10.8 }, { 4000, -23.7 }, { 8000, -40.6 } } },
    };
    for (double rate : rates)
        for (const auto& row : rows)
            for (const auto& p : row.pts)
            {
                const double g = e2e (rate, true, row.send, p.first, -40.0).first;
                INFO ("fs " << rate << " Send " << row.send << " % " << p.first << " Hz: " << g << " dB, expected " << p.second);
                CHECK (g == Approx (p.second).margin (row.tol));
            }
}

TEST_CASE ("Distortion end-to-end: 1 kHz gain vs input level (+-3 dB)", "[colour][distortion]")
{
    struct Row { double send; std::vector<std::pair<double, double>> pts; };
    const Row rows[] = {
        { 100.0, { { -40, 28.7 }, { -32, 21.3 }, { -24, 13.5 }, { -16, 5.5 }, { -8, -2.5 }, { 0, -10.5 } } },
        { 50.0, { { -40, 11.7 }, { -28, 10.9 }, { -24, 8.7 }, { -16, 2.3 }, { -8, -4.9 }, { 0, -12.3 } } },
        { 25.0, { { -40, -1.8 }, { -32, -2.0 }, { -24, -3.7 }, { -16, -7.9 }, { -8, -13.7 }, { 0, -19.3 } } },
        { 10.0, { { -28, -10.7 }, { -24, -11.8 }, { -20, -13.6 }, { -16, -16.0 }, { -12, -18.8 }, { -8, -21.8 }, { -4, -24.7 }, { 0, -27.4 } } },
    };
    for (double rate : rates)
    {
        for (const auto& row : rows)
            for (const auto& p : row.pts)
            {
                const double g = e2e (rate, true, row.send, 1000.0, p.first).first;
                INFO ("fs " << rate << " Send " << row.send << " % at " << p.first << " dBFS: " << g << " dB, expected " << p.second);
                CHECK (g == Approx (p.second).margin (3.0));
            }
        // test vector: Send 10 % silent at -32 dBFS (both channels)
        const auto q = e2e (rate, true, 10.0, 1000.0, -32.0);
        CHECK (q.first < -200.0);
        CHECK (q.second < -200.0);
    }
}

TEST_CASE ("Distortion end-to-end: odd-harmonic profile at 1 kHz (+-4 dB) and no even harmonics", "[colour][distortion]")
{
    for (double rate : rates)
    {
        const double pts[][3] = { { 100, -40, -9 }, { 100, 0, -5 }, { 50, -24, -20 }, { 50, 0, -11 } };
        for (const auto& p : pts)
        {
            const double h3 = e2e (rate, true, p[0], 1000.0, p[1], true, 3).first;
            INFO ("fs " << rate << " Send " << p[0] << " %, " << p[1] << " dBFS: H3 " << h3 << " dB, expected " << p[2]);
            CHECK (h3 == Approx (p[2]).margin (4.0));
        }
        CHECK (distHarmonicDb (rate, 100, 0, 2) < -95.0);
        CHECK (distHarmonicDb (rate, 50, -24, 2) < -95.0);
    }
}

TEST_CASE ("Distortion: per-channel routing, gate thresholds per channel (+-4 dB)", "[colour][distortion]")
{
    for (double rate : rates)
    {
        // test vector (end-to-end): Send 50 %, 1 kHz -40 dBFS on L only: +11.7 dB left, right silent; both: +11.7 dB each
        const auto e1 = e2e (rate, true, 50.0, 1000.0, -40.0, false), e2 = e2e (rate, true, 50.0, 1000.0, -40.0, true);
        CHECK (e1.first == Approx (11.7).margin (2.0));
        CHECK (e1.second < e1.first - 90.0);
        CHECK (e2.first == Approx (11.7).margin (2.0));
        CHECK (e2.second == Approx (e2.first).margin (1e-6));
        // component level
        const auto one = distGain (rate, 50.0, 1000.0, -40.0);
        CHECK (one.first == Approx (11.7).margin (2.0));
        CHECK (one.second < one.first - 90.0);
        const auto both = distGain (rate, 50.0, 1000.0, -40.0, true);
        CHECK (both.first == Approx (11.7).margin (2.0));
        CHECK (both.second == Approx (both.first).margin (1e-9));
        // no leakage at Width 100 % either, and with loud input
        CHECK (distGain (rate, 100.0, 1000.0, -6.0, false, 100.0).second < -150.0);

        auto outRms = [rate] (double send, double levelDb, bool right) {
            Distortion d;
            d.prepare (rate);
            d.setSend (send);
            d.snapParameters();
            d.reset();
            const int n = (int) (rate * 0.5);
            const auto x = sine (1000.0, std::pow (10.0, levelDb / 20.0), n, rate);
            const std::vector<double> z ((size_t) n, 0.0);
            const auto out = right ? runEffect (d, z, x) : runEffect (d, x, z);
            return rms (right ? out.r : out.l, (size_t) (0.1 * rate), (size_t) n);
        };
        for (bool right : { false, true })
        {
            CHECK (outRms (10.0, -32.0, right) == 0.0);
            CHECK (outRms (10.0, -28.0, right) > 0.0);
            CHECK (outRms (0.0, -28.0, right) == 0.0);
            CHECK (outRms (0.0, -24.0, right) > 0.0);
            CHECK (outRms (25.0, -40.0, right) > 0.0);
        }
    }
}

TEST_CASE ("Distortion: width 0 % identical, 100 % partially decorrelated at equal level; latency about 6.8 ms", "[colour][distortion]")
{
    for (double rate : rates)
    {
        const int n = (int) rate;
        const auto x = sine (1000.0, 0.01, n, rate);
        for (double width : { 0.0, 100.0 })
        {
            Distortion d;
            d.prepare (rate);
            d.setSend (50.0);
            d.setWidth (width);
            d.snapParameters();
            d.reset();
            const auto out = runEffect (d, x, x);
            if (width == 0.0)
            {
                CHECK (out.l == out.r);
            }
            else
            {
                const double rho = correlation (out.l, out.r, (size_t) (0.1 * rate), (size_t) n);
                INFO ("fs " << rate << " width 100 % correlation " << rho);
                CHECK (rho == Approx (0.6).margin (0.1));
                CHECK (db (rms (out.l, (size_t) (0.1 * rate), (size_t) n) / rms (out.r, (size_t) (0.1 * rate), (size_t) n)) == Approx (0.0).margin (0.5));
            }
        }

        // latency: cross-correlation of a quiet (linear) band-limited burst
        Distortion d;
        d.prepare (rate);
        d.setSend (50.0);
        d.snapParameters();
        d.reset();
        auto burst = noise ((int) (0.2 * rate), 0.01, 5u);
        burst.resize ((size_t) (0.5 * rate), 0.0);
        const auto out = runEffect (d, burst, std::vector<double> (burst.size(), 0.0));
        const double lagMs = xcorrLag (burst, out.l, (int) (0.02 * rate)) * 1000.0 / rate;
        INFO ("distortion latency " << lagMs << " ms");
        CHECK (lagMs == Approx (6.8).margin (1.0));
    }
}

// ---------------------------------------------------------------------------------------------
TEST_CASE ("Doubler: pitch shift exactly Detune cents (+-0.5), level +13.0 dB re its input (+-1), latency <= 15 ms", "[colour][doubler]")
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
        CHECK (lvl == Approx (13.0).margin (1.0));
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
        CHECK (lvl == Approx (13.0).margin (1.5));
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

TEST_CASE ("Test vector: doubler end-to-end, Level 100 %, +8 cents (spec r3 11)", "[colour][doubler][vector]")
{
    for (double rate : rates)
    {
        // 1 kHz tone on both channels at -40 / -20 / -8 / 0 dBFS: +13.0 / +9.3 / +1.2 / -4.5 dB (follows the compressor)
        const double pts[][2] = { { -40, 13.0 }, { -20, 9.3 }, { -8, 1.2 }, { 0, -4.5 } };
        for (const auto& p : pts)
        {
            const auto g = e2e (rate, false, 8.0, 1000.0, p[0]);
            INFO ("fs " << rate << " tone " << p[0] << " dBFS: L " << g.first << " R " << g.second << " dB, expected " << p[1]);
            CHECK (g.first == Approx (p[1]).margin (1.0));
            CHECK (g.second == Approx (p[1]).margin (1.0));
        }

        // -40 dBFS one-sided log sweep 100 Hz - 10 kHz: +12.9 dB (energy ratio), nothing on the right
        Settings s;
        s.hostSync = false;
        s.delayMs = { 100.0, 100.0 };
        s.feedbackPct = { 0.0, 0.0 };
        s.doublerOn = true;
        s.doublerLevel = 100.0;
        s.doublerCents = 8.0;
        Settings off = s;
        off.doublerOn = false;
        const int n = (int) (rate * 3.0);
        std::vector<double> y ((size_t) n), z ((size_t) n, 0.0);
        const double f1 = 100.0, f2 = 10000.0, T = n / rate, Lr = T / std::log (f2 / f1);
        for (int i = 0; i < n; ++i)
            y[(size_t) i] = 0.01 * std::sin (twoPi * f1 * Lr * (std::exp ((i / rate) / Lr) - 1.0));
        const auto a = render (s, y, z, rate), b = render (off, y, z, rate);
        std::vector<double> fx ((size_t) n);
        for (size_t i = 0; i < fx.size(); ++i) fx[i] = a.l[i] - b.l[i];
        const double lvl = db (rms (fx, 0, (size_t) n) / rms (y, 0, (size_t) n));
        INFO ("doubler -40 dBFS sweep " << lvl << " dB");
        CHECK (lvl == Approx (12.9).margin (1.0));
        CHECK (a.r == b.r); // nothing on the right
    }
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
