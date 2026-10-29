// Craft Punch Compressor - test helpers: run the engine offline and measure like the spec's test procedures.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <random>
#include <vector>

#include "dsp/PunchEngine.h"

namespace testutil
{
using namespace craft::punch;

inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
inline double linToDb (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-30)); }

struct Signal
{
    std::vector<double> l, r; // r empty: mono
    bool stereo() const { return ! r.empty(); }
    size_t size() const { return l.size(); }
};

// Settings of the spec's measurement setups: Ratio 1 (neutral), Mix 100, Output 0, Analog Off, Punch 0.
inline Settings neutralSettings()
{
    Settings s;
    s.ratio = 1.0;
    s.mixPct = 100.0;
    s.outputDb = 0.0;
    s.analog = 0;
    s.punch = 0.0;
    return s;
}

// Section 7.4 setup: T -24, R 4, Mix 100, Analog Off, Output 0, Attack 7, Release 100, Punch 0.
inline Settings trajectorySettings()
{
    Settings s = neutralSettings();
    s.thresholdDb = -24.0;
    s.ratio = 4.0;
    s.attackMs = 7.0;
    s.releaseMs = 100.0;
    return s;
}

// Run the engine over a whole signal and return the output aligned to the input (latency removed).
inline Signal runAligned (const Signal& in, double fs, const Settings& s, int block = 512, PunchEngine* engineOut = nullptr)
{
    PunchEngine local;
    PunchEngine& e = engineOut != nullptr ? *engineOut : local;
    const int ch = in.stereo() ? 2 : 1;
    e.prepare (fs, ch);
    e.setSettings (s);
    e.reset();
    const int L = e.getLatencySamples();
    Signal buf;
    buf.l = in.l;
    buf.l.resize (in.size() + size_t (L), 0.0);
    if (in.stereo())
    {
        buf.r = in.r;
        buf.r.resize (in.size() + size_t (L), 0.0);
    }
    const int n = int (buf.l.size());
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = std::min (block, n - pos);
        double* ptrs[2] = { buf.l.data() + pos, in.stereo() ? buf.r.data() + pos : nullptr };
        e.process (ptrs, len);
    }
    Signal out;
    out.l.assign (buf.l.begin() + L, buf.l.end());
    if (in.stereo())
        out.r.assign (buf.r.begin() + L, buf.r.end());
    return out;
}

inline Signal mono (std::vector<double> x)
{
    Signal s;
    s.l = std::move (x);
    return s;
}
inline Signal stereo (std::vector<double> x)
{
    Signal s;
    s.l = x;
    s.r = std::move (x);
    return s;
}

inline std::vector<double> sine (double fs, double f, double amp, size_t n, double phase = 0.0)
{
    std::vector<double> x (n);
    for (size_t i = 0; i < n; ++i)
        x[i] = amp * std::sin (2.0 * pi * f * double (i) / fs + phase);
    return x;
}

inline double rms (const std::vector<double>& x, size_t a, size_t b)
{
    double s = 0.0;
    for (size_t i = a; i < b; ++i)
        s += x[i] * x[i];
    return std::sqrt (s / double (std::max<size_t> (1, b - a)));
}

// Amplitude of the component at frequency f over [a, b) (exact for an integer number of periods).
inline double toneAmplitude (const std::vector<double>& x, size_t a, size_t b, double f, double fs)
{
    std::complex<double> acc { 0.0, 0.0 };
    for (size_t i = a; i < b; ++i)
        acc += x[i] * std::polar (1.0, -2.0 * pi * f * double (i) / fs);
    return 2.0 * std::abs (acc) / double (b - a);
}

struct ToneResult
{
    double gainDb, h2Dbc, h3Dbc;
};

// Steady tone through the engine: fundamental gain re input, H2 and H3 re fundamental. Analysis window: the last
// `analysisSeconds` (rounded to whole periods) after `settleSeconds`.
inline ToneResult measureTone (double fs, const Settings& s, double f, double amp, double settleSeconds = 0.5,
                               double analysisSeconds = 1.0, bool useStereo = true)
{
    const size_t periods = size_t (std::round (analysisSeconds * f));
    const size_t win = size_t (std::round (double (periods) * fs / f));
    const size_t n = size_t (settleSeconds * fs) + win;
    const auto x = sine (fs, f, amp, n);
    const auto y = runAligned (useStereo ? stereo (x) : mono (x), fs, s).l;
    const size_t a = n - win;
    const double h1 = toneAmplitude (y, a, n, f, fs);
    return { linToDb (h1 / amp), linToDb (toneAmplitude (y, a, n, 2.0 * f, fs) / h1),
             linToDb (toneAmplitude (y, a, n, 3.0 * f, fs) / h1) };
}

// Section 7.4 stimulus: 1 kHz sine -40 dBFS (0.6 s) -> -6 dBFS (0.6 s) -> -40 dBFS (1.4 s).
struct Trajectory
{
    std::vector<double> x, y;
    size_t up = 0, down = 0;
    double fs = 48000.0;
    // per-millisecond RMS gain (output re input) over [t, t + 1 ms) after the step
    double gainAt (size_t step, double ms) const
    {
        const size_t a = step + size_t (std::lround (ms * fs / 1000.0));
        const size_t b = a + size_t (std::lround (fs / 1000.0));
        return linToDb (rms (y, a, b) / rms (x, a, b));
    }
};

inline Trajectory runTrajectory (double fs, const Settings& s)
{
    Trajectory t;
    t.fs = fs;
    const size_t n = size_t (2.6 * fs);
    t.up = size_t (0.6 * fs);
    t.down = size_t (1.2 * fs);
    t.x.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double amp = (i >= t.up && i < t.down) ? dbToLin (-6.0) : dbToLin (-40.0);
        t.x[i] = amp * std::sin (2.0 * pi * 1000.0 * double (i) / fs);
    }
    t.y = runAligned (stereo (t.x), fs, s).l;
    return t;
}

inline std::vector<double> gaussianNoise (size_t n, double rmsValue, std::uint32_t seed)
{
    std::mt19937 rng (seed);
    std::normal_distribution<double> nd (0.0, rmsValue);
    std::vector<double> x (n);
    for (auto& v : x)
        v = nd (rng);
    return x;
}

inline double maxAbs (const std::vector<double>& x)
{
    double m = 0.0;
    for (double v : x)
        m = std::max (m, std::abs (v));
    return m;
}

} // namespace testutil
