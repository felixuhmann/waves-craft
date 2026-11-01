#pragma once

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "dsp/LimiterEngine.h"

namespace testutil
{
using namespace craft::limiter;

inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
inline double linToDb (double g) { return 20.0 * std::log10 (g); }

struct Signal
{
    std::vector<double> l, r; // r empty for mono
    bool stereo() const { return ! r.empty(); }
    std::size_t size() const { return l.size(); }
};

// Run the engine over a whole signal (blocks of `block` samples); returns the output in real time
// (i.e. delayed by the latency) with `extra` samples of zero input appended to flush the look-ahead.
inline Signal runEngine (const Signal& in, double fs, const Settings& s, int extra = -1, int block = 256)
{
    LimiterEngine e;
    const int ch = in.stereo() ? 2 : 1;
    e.prepare (fs, ch);
    e.setSettings (s);
    e.reset();
    if (extra < 0)
        extra = e.getLatencySamples();

    Signal out;
    out.l = in.l;
    out.l.resize (in.size() + std::size_t (extra), 0.0);
    if (in.stereo())
    {
        out.r = in.r;
        out.r.resize (in.size() + std::size_t (extra), 0.0);
    }
    const int n = int (out.l.size());
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = std::min (block, n - pos);
        double* ptrs[2] = { out.l.data() + pos, in.stereo() ? out.r.data() + pos : nullptr };
        e.process (ptrs, len);
    }
    return out;
}

// Output aligned to the input: drops the first L samples of the real-time output.
inline Signal aligned (const Signal& realTime, int latency, std::size_t length)
{
    Signal a;
    a.l.assign (realTime.l.begin() + latency, realTime.l.begin() + latency + std::ptrdiff_t (length));
    if (realTime.stereo())
        a.r.assign (realTime.r.begin() + latency, realTime.r.begin() + latency + std::ptrdiff_t (length));
    return a;
}

// Straight-line implementation of spec section 4 (fixed release) in aligned time, written independently of
// the engine (brute-force window minimum and boxcar sums). D1: g = min(s, r). D2: q[-1] = 1 and an all-pass
// boxcar history. Returns the gain g[n] (without makeup).
// Stream start (D2): the model starts at the real-time stream start, i.e. at aligned time n = -L, with L samples
// of silence before the first input sample, q = 1 and an all-pass boxcar history; so a peak within the first L
// samples is already anticipated by a (partial) ramp, exactly as in a running stream.
inline std::vector<double> referenceGain (const Signal& in, double fs, double thresholdDb, double releaseMs)
{
    int L = 64, A = 38, B = 27;
    if (fs > 50000.0) { L = 128; A = 75; B = 54; }
    if (fs > 100000.0) { L = 256; A = 150; B = 107; }

    const std::size_t pad = std::size_t (L);
    const std::size_t N = in.size() + pad;
    const double T = std::pow (10.0, thresholdDb / 20.0);
    std::vector<double> r (N + std::size_t (L), 1.0);
    for (std::size_t n = pad; n < N; ++n)
    {
        double p = std::abs (in.l[n - pad]);
        if (in.stereo())
            p = std::max (p, std::abs (in.r[n - pad]));
        r[n] = p > T ? T / p : 1.0;
    }
    const double c = std::exp (-std::log (10.0) / (releaseMs * 1.0e-3 * fs));
    std::vector<double> q (N);
    double prev = 1.0;
    for (std::size_t n = 0; n < N; ++n)
    {
        double h = 1.0;
        for (int k = 1; k <= L; ++k)
            h = std::min (h, r[n + std::size_t (k)]);
        prev = h < prev ? h : h - (h - prev) * c;
        q[n] = prev;
    }
    auto boxcar = [] (const std::vector<double>& x, int len) {
        std::vector<double> y (x.size());
        for (std::size_t n = 0; n < x.size(); ++n)
        {
            double s = 0.0;
            for (int k = 0; k < len; ++k)
                s += (n >= std::size_t (k)) ? x[n - std::size_t (k)] : 1.0;
            y[n] = s / len;
        }
        return y;
    };
    const auto s = boxcar (boxcar (q, A), B);
    std::vector<double> g (in.size());
    for (std::size_t n = 0; n < in.size(); ++n)
        g[n] = std::min (s[n + pad], r[n + pad]);
    return g;
}

inline Signal noise (std::size_t n, bool stereo, unsigned seed, double amplitude = 1.0)
{
    std::mt19937 rng (seed);
    std::normal_distribution<double> d (0.0, amplitude);
    Signal s;
    s.l.resize (n);
    for (auto& v : s.l)
        v = d (rng);
    if (stereo)
    {
        s.r.resize (n);
        for (auto& v : s.r)
            v = d (rng);
    }
    return s;
}

// Programme-like test signal: noise with a slowly modulated envelope plus occasional clicks and bursts.
inline Signal programme (std::size_t n, bool stereo, unsigned seed, double fs)
{
    Signal s = noise (n, stereo, seed, 0.25);
    std::mt19937 rng (seed + 17);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double env = 0.4 + 1.2 * std::pow (std::sin (2.0 * 3.14159265358979 * 1.7 * double (i) / fs), 2.0);
        s.l[i] *= env;
        if (stereo)
            s.r[i] *= env * 0.8;
        if (u (rng) < 0.0007)
        {
            const double click = (u (rng) < 0.5 ? -1.0 : 1.0) * (1.0 + 6.0 * u (rng));
            (u (rng) < 0.5 || ! stereo ? s.l[i] : s.r[i]) = click;
        }
    }
    return s;
}

// Gain trajectory of the engine for a stereo-identical DC input: out / in / makeup in aligned time.
inline std::vector<double> dcGain (const std::vector<double>& x, double fs, const Settings& s)
{
    Signal in;
    in.l = x;
    in.r = x;
    LimiterEngine probe;
    probe.prepare (fs, 2);
    const int L = probe.getLatencySamples();
    const auto out = aligned (runEngine (in, fs, s), L, x.size());
    const double makeup = std::pow (10.0, (s.ceilingDb - s.thresholdDb) / 20.0);
    std::vector<double> g (x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        g[i] = out.l[i] / x[i] / makeup;
    return g;
}

} // namespace testutil
