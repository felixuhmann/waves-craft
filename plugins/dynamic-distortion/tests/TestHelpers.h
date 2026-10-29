// Shared measurement helpers for the Craft Dynamic Distortion unit tests and the calibration tool.
#pragma once

#include <cmath>
#include <functional>
#include <vector>

#include "dsp/DynamicDistortion.h"

namespace ddtest
{
using namespace craft::dd;

constexpr double pi = 3.14159265358979323846;

inline double db (double g) { return 20.0 * std::log10 (std::max (std::abs (g), 1.0e-30)); }
inline double fromDb (double d) { return std::pow (10.0, d / 20.0); }
// Peak amplitude of a sine whose RMS level is `dbRms` dBFS.
inline double sinePeak (double dbRms) { return std::sqrt (2.0) * fromDb (dbRms); }

// Amplitude of the component at f in y[start, start + n) (quadrature correlation).
inline double toneAmplitude (const std::vector<double>& y, size_t start, size_t n, double f, double fs)
{
    // quadrature correlation with a recursive oscillator (re-seeded every 4096 samples against drift)
    double re = 0, im = 0;
    const double w = 2.0 * pi * f / fs, cw = std::cos (w), sw = std::sin (w);
    double c = 0, s = 0;
    for (size_t i = start; i < start + n; ++i)
    {
        if ((i - start) % 4096 == 0)
        {
            c = std::cos (w * double (i));
            s = std::sin (w * double (i));
        }
        re += y[i] * c;
        im += y[i] * s;
        const double nc = c * cw - s * sw;
        s = s * cw + c * sw;
        c = nc;
    }
    return 2.0 * std::sqrt (re * re + im * im) / double (n);
}

inline double rms (const std::vector<double>& y, size_t start, size_t n)
{
    double s = 0;
    for (size_t i = start; i < start + n; ++i)
        s += y[i] * y[i];
    return std::sqrt (s / double (std::max<size_t> (n, 1)));
}

// Number of samples of an integer number of periods of f closest to `seconds`.
inline size_t wholePeriods (double f, double fs, double seconds)
{
    const double periods = std::max (1.0, std::round (seconds * f));
    return size_t (std::round (periods * fs / f));
}

// Settings of the section 6.1 measurements: Direct 0 %, Distortion Level 100 %, EQ neutral, LP 21 kHz.
inline Settings distortionOnly (double drivePercent, double attackMs = 1.0, double releaseMs = 31.8)
{
    Settings s = neutralSettings();
    s.directPercent = 0.0;
    s.distPercent = 100.0;
    s.drivePercent = drivePercent;
    s.attackMs = attackMs;
    s.releaseMs = releaseMs;
    return s;
}

// Development overrides used by the calibration tool (the tests use the built-in values).
struct Tweaks
{
    const expansion::Curve* curve = nullptr;
    double holdMs = -1.0;
};
inline Tweaks& tweaks()
{
    static Tweaks t;
    return t;
}

inline void prepareEngine (DynamicDistortion& e, const Settings& s, double fs)
{
    if (tweaks().curve != nullptr)
        e.setCurve (*tweaks().curve);
    if (tweaks().holdMs >= 0.0)
        e.setDetectorHold (tweaks().holdMs);
    e.setSettings (s);
    e.prepare (fs);
}

// Runs a prepared engine on one channel (block-wise) and returns the output.
inline std::vector<double> run (DynamicDistortion& e, const std::vector<double>& in, int block = 256)
{
    std::vector<double> out (in);
    for (size_t i = 0; i < out.size(); i += size_t (block))
    {
        const int n = int (std::min (out.size() - i, size_t (block)));
        double* ptr[1] = { out.data() + i };
        e.process (ptr, 1, n);
    }
    return out;
}

inline std::vector<double> render (const Settings& s, double fs, size_t n, const std::function<double (size_t)>& input)
{
    DynamicDistortion e;
    prepareEngine (e, s, fs);
    std::vector<double> x (n);
    for (size_t i = 0; i < n; ++i)
        x[i] = input (i);
    return run (e, x);
}

struct ToneResult
{
    double gainDb;  // output fundamental / input (dB)
    double h3Dbc;   // third harmonic re fundamental (dBc)
    double h2Dbc;
    double outRmsDb; // total output RMS (dBFS)
};

// Steady-state measurement of a sine at `levelDbRms` (RMS dBFS) and frequency f.
inline ToneResult measureTone (const Settings& s, double fs, double f, double levelDbRms, double settleSeconds = 0.6,
                               double measureSeconds = 0.25)
{
    const double amp = sinePeak (levelDbRms);
    const size_t settle = size_t (settleSeconds * fs);
    const size_t n = wholePeriods (f, fs, std::max (measureSeconds, 10.0 / f));
    auto y = render (s, fs, settle + n, [&] (size_t i) { return amp * std::sin (2.0 * pi * f * double (i) / fs); });
    const double a1 = toneAmplitude (y, settle, n, f, fs);
    const double a2 = 2.0 * f < 0.5 * fs ? toneAmplitude (y, settle, n, 2.0 * f, fs) : 0.0;
    const double a3 = 3.0 * f < 0.5 * fs ? toneAmplitude (y, settle, n, 3.0 * f, fs) : 0.0;
    return { db (a1 / amp), db (a3 / a1), db (a2 / a1), db (rms (y, settle, n)) };
}

// Small-signal gain (dB) of the processor at f (sine at -50 dBFS unless given).
inline double smallSignalDb (const Settings& s, double fs, double f, double levelDbRms = -50.0)
{
    return measureTone (s, fs, f, levelDbRms, 0.4, 0.2).gainDb;
}

// Section 6.2 step test: 1 kHz sine, -50 dBFS for `pre` seconds, -20 dBFS for 1 s, then -50 dBFS for 2 s.
// Returns the distortion-path gain (dB, output RMS / input RMS) over the 1 ms window ending at each
// requested time after the level change (latency compensated).
struct StepTrace
{
    std::vector<double> up, down;
};
inline StepTrace stepResponse (const Settings& s, double fs, const std::vector<double>& upMs,
                               const std::vector<double>& downMs, double preSeconds = 1.0)
{
    DynamicDistortion e;
    prepareEngine (e, s, fs);
    const size_t pre = size_t (preSeconds * fs), hi = size_t (1.0 * fs), post = size_t (2.0 * fs);
    const size_t n = pre + hi + post;
    std::vector<double> x (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double lvl = (i >= pre && i < pre + hi) ? -20.0 : -50.0;
        x[i] = sinePeak (lvl) * std::sin (2.0 * pi * 1000.0 * double (i) / fs);
    }
    auto y = run (e, x);
    const size_t lat = size_t (e.getLatencySamples());
    const size_t win = size_t (std::round (fs * 0.001));
    auto gainAt = [&] (size_t changeAt, double ms) {
        const size_t endIn = changeAt + size_t (std::round (ms * 1.0e-3 * fs));
        const size_t startIn = endIn - win;
        return db (rms (y, startIn + lat, win) / rms (x, startIn, win));
    };
    StepTrace t;
    for (double ms : upMs)
        t.up.push_back (gainAt (pre, ms));
    for (double ms : downMs)
        t.down.push_back (gainAt (pre + hi, ms));
    return t;
}

// Linear response of the complete processor (left channel) from an impulse small enough to stay below the
// expansion onset and the clipper.
inline std::vector<double> impulseResponse (const Settings& s, double fs, double seconds = 1.0, double amp = 0.002)
{
    std::vector<double> x (size_t (seconds * fs), 0.0);
    x[0] = amp;
    DynamicDistortion e;
    prepareEngine (e, s, fs);
    auto y = run (e, x, 1024);
    for (auto& v : y)
        v /= amp;
    return y;
}

inline double dftMagnitude (const std::vector<double>& y, double f, double fs)
{
    double re = 0, im = 0;
    const double w = 2.0 * pi * f / fs;
    double c = 1.0, sn = 0.0;
    const double cw = std::cos (w), sw = std::sin (w);
    for (size_t i = 0; i < y.size(); ++i)
    {
        re += y[i] * c;
        im -= y[i] * sn;
        const double nc = c * cw - sn * sw;
        sn = sn * cw + c * sw;
        c = nc;
    }
    return std::sqrt (re * re + im * im);
}

// Magnitude (dB) of an impulse response at f.
inline double responseDb (const std::vector<double>& ir, double f, double fs) { return db (dftMagnitude (ir, f, fs)); }

} // namespace ddtest
