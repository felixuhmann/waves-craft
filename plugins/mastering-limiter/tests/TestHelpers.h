// Shared helpers for the Craft Mastering Limiter unit tests.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#include "dsp/GainComputer.h"
#include "dsp/MasteringEngine.h"

namespace mltest
{
namespace dsp = craft::mastering_limiter::dsp;

constexpr double pi = 3.14159265358979323846;

inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }

// Settings with the requantiser transparent at 24 bit (spec 9 default for the test vectors).
inline dsp::Settings plainSettings (double thresholdDb = 0.0, double ceilingDb = 0.0, double releaseMs = 1.0,
                                    bool adaptive = false)
{
    dsp::Settings s;
    s.thresholdDb = thresholdDb;
    s.ceilingDb = ceilingDb;
    s.releaseMs = releaseMs;
    s.adaptiveRelease = adaptive;
    s.wordLength = 24;
    s.dither = dsp::Dither::None;
    s.shaping = dsp::Shaping::None;
    return s;
}

// Runs the engine over a whole signal (1 or 2 channels) in blocks; returns the output (same length).
inline std::vector<std::vector<double>> runEngine (dsp::MasteringEngine& e, std::vector<std::vector<double>> sig,
                                                   int block = 512)
{
    const int n = int (sig[0].size());
    const int nch = int (sig.size());
    for (int start = 0; start < n; start += block)
    {
        const int len = std::min (block, n - start);
        double* ptrs[2] = { nullptr, nullptr };
        for (int ch = 0; ch < std::min (nch, 2); ++ch)
            ptrs[ch] = sig[size_t (ch)].data() + start;
        e.process (ptrs, nch, len);
    }
    return sig;
}

inline std::vector<double> runMono (double fs, const dsp::Settings& s, const std::vector<double>& x, int block = 512)
{
    dsp::MasteringEngine e;
    e.setSettings (s);
    e.prepare (fs);
    return runEngine (e, { x }, block)[0];
}

// ---------------------------------------------------------------------------------------------
// Literal transcription of spec section 4 in aligned time (direct formulas, no streaming tricks).
// Returns v[n] for every channel; r beyond the end counts as 1, q[-1] = 1, boxcar history = 1.
inline std::vector<std::vector<double>> referenceLimiter (const std::vector<std::vector<double>>& x, double fs,
                                                          double thresholdDb, double ceilingDb, double releaseMs,
                                                          std::vector<double>* gainOut = nullptr)
{
    const auto cfg = dsp::latencyConfigFor (fs);
    const int L = cfg.latency, A = cfg.boxcarA, B = cfg.boxcarB;
    const int n = int (x[0].size());
    const double T = dbToLin (thresholdDb);
    const size_t N = size_t (n);
    std::vector<double> r (N, 1.0), q (N), s1 (N), s (N), g (N);
    for (int i = 0; i < n; ++i)
    {
        double p = 0.0;
        for (const auto& ch : x)
            p = std::max (p, std::abs (ch[size_t (i)]));
        r[size_t (i)] = std::min (1.0, T / p);
    }
    const double c = std::exp (-std::log (10.0) / (releaseMs * 1.0e-3 * fs));
    double prev = 1.0;
    for (int i = 0; i < n; ++i)
    {
        double h = 1.0;
        for (int k = i + 1; k <= i + L && k < n; ++k)
            h = std::min (h, r[size_t (k)]);
        prev = h < prev ? h : h - (h - prev) * c;
        q[size_t (i)] = prev;
    }
    auto ma = [] (const std::vector<double>& in, std::vector<double>& out, int len) {
        for (size_t i = 0; i < in.size(); ++i)
        {
            double acc = 0.0;
            for (int k = 0; k < len; ++k)
                acc += i >= size_t (k) ? in[i - size_t (k)] : 1.0;
            out[i] = acc / len;
        }
    };
    ma (q, s1, A);
    ma (s1, s, B);
    for (int i = 0; i < n; ++i)
        g[size_t (i)] = std::min (s[size_t (i)], r[size_t (i)]);
    const double mk = dbToLin (ceilingDb - thresholdDb);
    std::vector<std::vector<double>> v = x;
    for (auto& ch : v)
        for (int i = 0; i < n; ++i)
            ch[size_t (i)] *= g[size_t (i)] * mk;
    if (gainOut != nullptr)
        *gainOut = g;
    return v;
}

// Feeds a peak sequence through a GainComputer and returns g[n] in aligned time (n = 0 .. N-1).
inline std::vector<double> gainTrajectory (double fs, const std::vector<double>& peaks, double thresholdDb,
                                           double releaseMs, bool adaptive)
{
    dsp::GainComputer gc;
    gc.setRelease (releaseMs, adaptive);
    gc.prepare (fs);
    const int L = gc.getLatency();
    const double T = dbToLin (thresholdDb);
    std::vector<double> g;
    g.reserve (peaks.size());
    for (size_t t = 0; t < peaks.size() + size_t (L); ++t)
    {
        const auto o = gc.process (t < peaks.size() ? peaks[t] : 0.0, T);
        if (t >= size_t (L))
            g.push_back (o.gain);
    }
    return g;
}

// Time (seconds, linearly interpolated) after sample `start` at which the deficit 1 - g first falls to
// `fraction` of `deficit0`. Returns -1 if it never does.
inline double recoveryTime (const std::vector<double>& g, size_t start, double deficit0, double fraction, double fs)
{
    const double level = deficit0 * fraction;
    double prevD = 1.0 - g[start > 0 ? start - 1 : 0];
    for (size_t i = start; i < g.size(); ++i)
    {
        const double d = 1.0 - g[i];
        if (d <= level)
        {
            const double frac = prevD > d ? (prevD - level) / (prevD - d) : 1.0;
            return (double (i) - 1.0 + frac - double (start)) / fs;
        }
        prevD = d;
    }
    return -1.0;
}

// ---------------------------------------------------------------------------------------------
// Minimal radix-2 FFT and Welch PSD (Hann window), for the dither / noise-shaping statistics.
inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * pi / double (len);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// One-sided PSD normalised so that white noise of variance s2 gives s2 in every bin (bins 0 .. seg/2).
inline std::vector<double> welchPsd (const std::vector<double>& x, size_t seg)
{
    std::vector<double> win (seg), psd (seg / 2 + 1, 0.0);
    double wpow = 0.0;
    for (size_t i = 0; i < seg; ++i)
    {
        win[i] = 0.5 - 0.5 * std::cos (2.0 * pi * double (i) / double (seg));
        wpow += win[i] * win[i];
    }
    size_t count = 0;
    std::vector<std::complex<double>> buf (seg);
    for (size_t start = 0; start + seg <= x.size(); start += seg / 2)
    {
        for (size_t i = 0; i < seg; ++i)
            buf[i] = x[start + i] * win[i];
        fft (buf);
        for (size_t k = 0; k <= seg / 2; ++k)
            psd[k] += std::norm (buf[k]);
        ++count;
    }
    for (auto& p : psd)
        p /= double (count) * wpow;
    return psd;
}

// Mean PSD over the bins within +-halfWidth Hz of f.
inline double bandMean (const std::vector<double>& psd, double fs, double f, double halfWidth)
{
    const double binHz = fs / double (2 * (psd.size() - 1));
    const int k0 = std::max (1, int (std::floor ((f - halfWidth) / binHz)));
    const int k1 = std::min (int (psd.size()) - 2, int (std::ceil ((f + halfWidth) / binHz)));
    double acc = 0.0;
    for (int k = k0; k <= k1; ++k)
        acc += psd[size_t (k)];
    return acc / double (k1 - k0 + 1);
}

inline double rms (const std::vector<double>& x, size_t from = 0)
{
    double acc = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        acc += x[i] * x[i];
    return std::sqrt (acc / double (x.size() - from));
}

} // namespace mltest
