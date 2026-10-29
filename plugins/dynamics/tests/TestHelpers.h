// Craft Dynamics - test helpers.
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <vector>

#include "dsp/DynamicsEngine.h"

namespace testutil
{

using namespace craft::dynamics::dsp;

inline constexpr double pi = 3.14159265358979323846;

struct Stereo
{
    std::vector<double> l, r;
};

inline DynamicsEngine makeEngine (const Settings& s, double fs, Variant v = Variant::Full)
{
    DynamicsEngine e (v);
    e.setSettings (s);
    e.prepare (fs);
    return e;
}

inline void run (DynamicsEngine& e, std::vector<double>& l, std::vector<double>* r, int block = 256)
{
    const int n = int (l.size());
    for (int i = 0; i < n; i += block)
    {
        const int len = std::min (block, n - i);
        e.process (l.data() + i, r != nullptr ? r->data() + i : nullptr, len);
    }
}

inline Stereo runStereo (DynamicsEngine& e, std::vector<double> l, std::vector<double> r, int block = 256)
{
    run (e, l, &r, block);
    return { std::move (l), std::move (r) };
}

// Sine with a per-sample amplitude envelope (linear amplitude).
inline std::vector<double> sine (int n, double fs, double freq, const std::function<double (int)>& amp, double phase = 0.0)
{
    std::vector<double> v (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i)
        v[size_t (i)] = amp (i) * std::sin (2.0 * pi * freq * i / fs + phase);
    return v;
}

inline std::vector<double> sine (int n, double fs, double freq, double peakDb)
{
    const double a = dbToGain (peakDb);
    return sine (n, fs, freq, [a] (int) { return a; });
}

inline double rms (const std::vector<double>& v, int from, int to)
{
    double s = 0.0;
    for (int i = from; i < to; ++i)
        s += v[size_t (i)] * v[size_t (i)];
    return std::sqrt (s / std::max (1, to - from));
}

inline double ratioDb (const std::vector<double>& out, const std::vector<double>& in, int from, int to)
{
    return 20.0 * std::log10 (rms (out, from, to) / rms (in, from, to));
}

// Steady-state gain (dB) of a 1 kHz sine of the given peak level through a fresh engine (both channels equal).
inline double steadyGainDb (const Settings& s, double fs, double peakDb, double freq = 1000.0, double settleS = 0.25)
{
    auto e = makeEngine (s, fs);
    const int n = int (settleS * fs) + int (0.05 * fs);
    auto in = sine (n, fs, freq, peakDb);
    auto out = runStereo (e, in, in);
    const int cycle = int (std::round (fs / freq));
    const int m = (int (0.05 * fs) / cycle) * cycle; // whole cycles
    return ratioDb (out.l, in, n - m, n);
}

// Per-1-ms gain envelope (dB) of out/in (1 kHz sine: one cycle per frame).
inline std::vector<double> frameGainDb (const std::vector<double>& out, const std::vector<double>& in, double fs, int from, int frames)
{
    std::vector<double> g;
    const int m = int (std::round (fs * 0.001));
    for (int k = 0; k < frames; ++k)
        g.push_back (ratioDb (out, in, from + k * m, from + (k + 1) * m));
    return g;
}

inline std::vector<double> frameGainLin (const std::vector<double>& out, const std::vector<double>& in, double fs, int from, int frames)
{
    std::vector<double> g;
    const int m = int (std::round (fs * 0.001));
    for (int k = 0; k < frames; ++k)
        g.push_back (rms (out, from + k * m, from + (k + 1) * m) / rms (in, from + k * m, from + (k + 1) * m));
    return g;
}

// First frame index (ms, frame centre) at which a monotone transition from a to b has covered `fraction`.
inline double crossingMs (const std::vector<double>& g, double a, double b, double fraction)
{
    const double level = a + fraction * (b - a);
    for (size_t k = 0; k < g.size(); ++k)
        if ((b > a && g[k] >= level) || (b < a && g[k] <= level))
            return double (k) + 0.5;
    return 1.0e9;
}

inline std::vector<double> noise (int n, unsigned seed, double amplitude)
{
    std::mt19937 rng (seed);
    std::normal_distribution<double> d (0.0, amplitude);
    std::vector<double> v (static_cast<size_t> (n));
    for (auto& x : v)
        x = d (rng);
    return v;
}

} // namespace testutil
