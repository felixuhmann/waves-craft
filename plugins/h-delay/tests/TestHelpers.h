#pragma once

#include <cmath>
#include <complex>
#include <vector>

#include "dsp/HybridDelayEngine.h"

namespace testutil
{
using namespace craft::hdelay;

// Settings for the deterministic core of spec section 4: Analog Off, LoFi off, Depth 0, filters off.
inline Settings coreSettings (double delayMs = 100.0, double feedback = 60.0)
{
    Settings s;
    s.sync = param::Sync::Ms;
    s.delayMs = s.delayRMs = delayMs;
    s.feedbackPct = feedback;
    s.analog = 0;
    s.lofi = false;
    s.modDepthPct = 0.0;
    s.mixPct = 100.0;
    s.outputDb = 0.0;
    return s;
}

inline HybridDelayEngine makeEngine (const Settings& s, double fs, InputMode mode = InputMode::Stereo)
{
    HybridDelayEngine e;
    e.setInputMode (mode);
    e.setSettings (s);
    e.prepare (fs);
    e.reset();
    return e;
}

struct Stereo
{
    std::vector<double> l, r;
};

// Runs the engine on the given input (copied), returns the output.
inline Stereo run (HybridDelayEngine& e, std::vector<double> l, std::vector<double> r)
{
    e.process (l.data(), r.data(), int (l.size()));
    return { std::move (l), std::move (r) };
}

inline Stereo impulse (HybridDelayEngine& e, int length, double ampL, double ampR = 0.0)
{
    std::vector<double> l (size_t (length), 0.0), r (size_t (length), 0.0);
    l[0] = ampL;
    r[0] = ampR;
    return run (e, std::move (l), std::move (r));
}

inline double db (double x) { return 20.0 * std::log10 (std::abs (x)); }

// Exact recursion of spec section 4 for an impulse a: echo k (k >= 1) amplitude.
inline std::vector<double> modelEchoes (double a, double g, int count)
{
    std::vector<double> e;
    double v = a;
    for (int k = 0; k < count; ++k)
    {
        const double w = v / (1.0 + std::abs (v));
        e.push_back (w);
        v = g * w;
    }
    return e;
}

// Single-bin DFT amplitude of x[n0 .. n0+len) at frequency f (len should hold whole periods).
inline std::complex<double> bin (const std::vector<double>& x, size_t n0, size_t len, double f, double fs)
{
    std::complex<double> acc { 0.0, 0.0 };
    const double w = 2.0 * pi * f / fs;
    for (size_t n = 0; n < len; ++n)
        acc += x[n0 + n] * std::polar (1.0, -w * double (n));
    return acc * (2.0 / double (len));
}

// DTFT of a finite sequence.
inline std::complex<double> dtft (const std::vector<double>& h, double f, double fs)
{
    std::complex<double> acc { 0.0, 0.0 };
    const double w = 2.0 * pi * f / fs;
    for (size_t n = 0; n < h.size(); ++n)
        acc += h[n] * std::polar (1.0, -w * double (n));
    return acc;
}

inline std::vector<double> sine (double amp, double f, double fs, size_t length)
{
    std::vector<double> x (length);
    for (size_t n = 0; n < length; ++n)
        x[n] = amp * std::sin (2.0 * pi * f * double (n) / fs);
    return x;
}

// Steady-state amplitude of harmonics 1..3 of the wet output for a sine input
// (feedback 0, given analog mode). Returns { fundamental, h2, h3 } as linear amplitudes.
struct Harmonics
{
    double h1, h2, h3;
};

inline Harmonics toneHarmonics (int analogMode, double amp, double f, double fs, double settleSeconds = 0.6,
                                int periods = 40)
{
    auto s = coreSettings (1.0, 0.0); // 1 ms delay, no feedback
    s.analog = analogMode;
    auto e = makeEngine (s, fs);
    const double samplesPerPeriod = fs / f;
    const size_t len = size_t (std::llround (samplesPerPeriod * periods));
    const size_t n0 = size_t (settleSeconds * fs);
    auto x = sine (amp, f, fs, n0 + len + 100);
    auto y = run (e, x, x);
    return { std::abs (bin (y.l, n0, len, f, fs)), std::abs (bin (y.l, n0, len, 2 * f, fs)),
             std::abs (bin (y.l, n0, len, 3 * f, fs)) };
}

} // namespace testutil
