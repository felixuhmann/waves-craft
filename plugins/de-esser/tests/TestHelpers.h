// Craft De-Esser - shared helpers for the unit tests.
#pragma once

#include <cmath>
#include <complex>
#include <random>
#include <vector>

#include "dsp/DeEsserEngine.h"

namespace craft::deesser::test
{

using namespace craft::deesser::dsp;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) { return 20.0 * std::log10 (g); }

struct Stereo
{
    std::vector<double> l, r;
    explicit Stereo (size_t n = 0) : l (n, 0.0), r (n, 0.0) {}
    size_t size() const { return l.size(); }
};

// Sine with piecewise constant level: segments of (peak dBFS, length in samples). Phase runs continuously.
inline std::vector<double> steppedSine (double freq, double fs, const std::vector<std::pair<double, size_t>>& segments,
                                        double phase = 0.0)
{
    std::vector<double> x;
    size_t n = 0;
    for (const auto& seg : segments)
    {
        const double a = dbToGain (seg.first);
        for (size_t i = 0; i < seg.second; ++i, ++n)
            x.push_back (a * std::sin (2.0 * pi * freq * double (n) / fs + phase));
    }
    return x;
}

// Runs a stereo signal through an engine, returns the output (double path).
inline Stereo runStereo (DeEsserEngine& e, const Stereo& in, int blockSize = 256)
{
    Stereo out = in;
    for (size_t pos = 0; pos < out.size(); pos += size_t (blockSize))
    {
        const int n = int (std::min (size_t (blockSize), out.size() - pos));
        double* ch[2] = { out.l.data() + pos, out.r.data() + pos };
        e.process (ch, n);
    }
    return out;
}

// Runs a stereo signal and records the gain applied at every sample.
inline std::vector<double> recordGain (DeEsserEngine& e, const Stereo& in)
{
    std::vector<double> g (in.size());
    for (size_t i = 0; i < in.size(); ++i)
    {
        double l = in.l[i], r = in.r[i];
        double* ch[2] = { &l, &r };
        e.process (ch, 1);
        g[i] = e.getCurrentGain();
    }
    return g;
}

inline DeEsserEngine makeEngine (double fs, double thresholdDb, double freq, SidechainMode sc = SidechainMode::HighPass,
                                 AudioMode audio = AudioMode::Wideband, MonitorMode mon = MonitorMode::Output, int channels = 2)
{
    DeEsserEngine e (channels);
    Settings s;
    s.thresholdDb = thresholdDb;
    s.frequencyHz = freq;
    s.sidechain = sc;
    s.audio = audio;
    s.monitor = mon;
    e.setSettings (s);
    e.prepare (fs);
    return e;
}

// Complex amplitude of a sinusoid of frequency f in x[from, to) by least squares on sin/cos.
inline std::complex<double> phasor (const std::vector<double>& x, double f, double fs, size_t from, size_t to)
{
    double ss = 0, cc = 0, sc = 0, xs = 0, xc = 0;
    for (size_t n = from; n < to; ++n)
    {
        const double w = 2.0 * pi * f * double (n) / fs;
        const double s = std::sin (w), c = std::cos (w);
        ss += s * s;
        cc += c * c;
        sc += s * c;
        xs += x[n] * s;
        xc += x[n] * c;
    }
    const double det = ss * cc - sc * sc;
    const double a = (xs * cc - xc * sc) / det; // sine coefficient
    const double b = (xc * ss - xs * sc) / det; // cosine coefficient
    // x ~ a sin + b cos = Im{(a + j b) e^{jw}}
    return { a, b };
}

inline double rms (const std::vector<double>& x, size_t from, size_t to)
{
    double s = 0.0;
    for (size_t n = from; n < to; ++n)
        s += x[n] * x[n];
    return std::sqrt (s / double (to - from));
}

// White noise high-passed by an 8th-order Butterworth at fc (four cookbook HP sections with the
// Butterworth Q values), normalised to unit RMS.
inline std::vector<double> highPassedNoise (size_t n, double fc, double fs, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::normal_distribution<double> nd;
    std::vector<double> x (n);
    for (auto& v : x)
        v = nd (rng);
    const double qs[4] = { 0.50979557910415917, 0.60134488693504528, 0.89997622313641570, 2.56291544774150616 };
    for (double q : qs)
    {
        const auto c = cookbookHighPass (fc, q, fs);
        Biquad b;
        for (auto& v : x)
            v = b.process (c, v);
    }
    const double r = rms (x, n / 10, n);
    for (auto& v : x)
        v /= r;
    return x;
}

} // namespace craft::deesser::test
