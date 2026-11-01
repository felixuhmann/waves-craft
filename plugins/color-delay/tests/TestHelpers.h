// Shared helpers for the Craft Colour Delay tests.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

#include "dsp/ColourDelayEngine.h"

namespace cdtest
{
using namespace craft::cdl;

inline constexpr double twoPi = 6.283185307179586476925;

struct Stereo
{
    std::vector<double> l, r;
};

// Settings for measuring the wet path in isolation: ms mode, delay 0 ms (1 sample), no feedback, 100 % wet.
inline Settings plainWet()
{
    Settings s;
    s.hostSync = false;
    s.delayMs = { 0.0, 0.0 };
    s.feedbackPct = { 0.0, 0.0 };
    s.dryWetPct = 100.0;
    return s;
}

inline Stereo render (const Settings& s, const std::vector<double>& inL, const std::vector<double>& inR, double fs = 48000.0,
                      double bpm = 120.0)
{
    ColourDelayEngine e;
    e.setSettings (s);
    e.prepare (fs);
    e.setTempo (bpm);
    e.reset();
    Stereo out { inL, inR };
    out.r.resize (out.l.size(), 0.0);
    const int block = 256;
    for (size_t i = 0; i < out.l.size(); i += block)
    {
        const int n = (int) std::min<size_t> (block, out.l.size() - i);
        e.process (out.l.data() + i, out.r.data() + i, n);
    }
    return out;
}

inline std::vector<double> sine (double freq, double amp, int n, double fs = 48000.0, double phase = 0.0)
{
    std::vector<double> x ((size_t) n);
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = amp * std::sin (twoPi * freq * i / fs + phase);
    return x;
}

inline std::vector<double> impulse (int n, double amp = 1.0, int at = 0)
{
    std::vector<double> x ((size_t) n, 0.0);
    x[(size_t) at] = amp;
    return x;
}

// deterministic white noise in [-amp, amp]
inline std::vector<double> noise (int n, double amp, uint32_t seed = 12345u)
{
    std::vector<double> x ((size_t) n);
    for (auto& v : x)
    {
        seed = seed * 1664525u + 1013904223u;
        v = amp * ((double) (seed >> 8) / 8388608.0 - 1.0);
    }
    return x;
}

inline double rms (const std::vector<double>& x, size_t from, size_t to)
{
    double acc = 0.0;
    for (size_t i = from; i < to; ++i)
        acc += x[i] * x[i];
    return std::sqrt (acc / (double) std::max<size_t> (1, to - from));
}

inline double db (double v) { return 20.0 * std::log10 (std::max (v, 1.0e-300)); }

// DTFT of x at frequency f (Hz)
inline std::complex<double> dtft (const std::vector<double>& x, double f, double fs = 48000.0, size_t from = 0, size_t to = 0)
{
    if (to == 0) to = x.size();
    std::complex<double> acc = 0.0;
    const double w = twoPi * f / fs;
    for (size_t i = from; i < to; ++i)
        acc += x[i] * std::polar (1.0, -w * (double) (i - from));
    return acc;
}

// amplitude of a sinusoid at f in x[from, to) (Hann-windowed DTFT, normalised)
inline double toneAmplitude (const std::vector<double>& x, double f, size_t from, size_t to, double fs = 48000.0)
{
    std::complex<double> acc = 0.0;
    double wsum = 0.0;
    const double w = twoPi * f / fs;
    const size_t n = to - from;
    for (size_t i = 0; i < n; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (twoPi * (double) i / (double) n);
        acc += win * x[from + i] * std::polar (1.0, -w * (double) i);
        wsum += win;
    }
    return 2.0 * std::abs (acc) / wsum;
}

inline double correlation (const std::vector<double>& a, const std::vector<double>& b, size_t from, size_t to)
{
    double ab = 0.0, aa = 0.0, bb = 0.0;
    for (size_t i = from; i < to; ++i)
    {
        ab += a[i] * b[i];
        aa += a[i] * a[i];
        bb += b[i] * b[i];
    }
    return ab / std::sqrt (aa * bb + 1.0e-300);
}

} // namespace cdtest
