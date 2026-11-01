// Shared helpers for the Craft Motion Filter unit tests.
#pragma once

#include <cmath>
#include <functional>
#include <vector>

#include "dsp/MotionFilter.h"

namespace mftest
{
using namespace craft::mf;

constexpr double pi = 3.14159265358979323846;

inline double db (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-20)); }
inline double fromDb (double d) { return std::pow (10.0, d / 20.0); }
inline double freqKnob (double hz) { return param::Freq::toNorm (hz); }

// Amplitude of the component at f in y[start, start + n) (quadrature correlation).
inline double toneAmplitude (const std::vector<double>& y, size_t start, size_t n, double f, double fs)
{
    double re = 0, im = 0;
    for (size_t i = start; i < start + n; ++i)
    {
        const double ph = 2.0 * pi * f * double (i) / fs;
        re += y[i] * std::cos (ph);
        im += y[i] * std::sin (ph);
    }
    return 2.0 * std::sqrt (re * re + im * im) / double (n);
}

// Number of samples of an integer number of periods of f closest to `seconds`.
inline size_t wholePeriods (double f, double fs, double seconds)
{
    const double periods = std::max (1.0, std::round (seconds * f));
    return size_t (std::round (periods * fs / f));
}

// Gain of a mono per-sample process at frequency f for a sine of the given amplitude.
inline double sineGain (const std::function<double (double)>& proc, double f, double fs, double amplitude = 0.0316,
                        double settleSeconds = 0.25, double measureSeconds = 0.25)
{
    const double settle = std::max (settleSeconds, 8.0 / f);
    const size_t s = size_t (settle * fs);
    const size_t n = wholePeriods (f, fs, std::max (measureSeconds, 10.0 / f));
    std::vector<double> y (s + n);
    for (size_t i = 0; i < y.size(); ++i)
        y[i] = proc (amplitude * std::sin (2.0 * pi * f * double (i) / fs));
    return toneAmplitude (y, s, n, f, fs) / amplitude;
}

// A prepared top-level processor (engine + oversampling) with the given settings.
inline MotionFilter makeEngine (const Settings& s, double fs)
{
    MotionFilter e;
    e.setSettings (s);
    e.prepare (fs);
    return e;
}

// Settings with the filter transparent (Amp type at 0 dB) - for delay / drive / chain tests.
inline Settings transparentSettings()
{
    Settings s;
    s.type = FilterType::Amp;
    s.freqNorm = 1.0;
    return s;
}

// Run the engine on a mono signal (fed to both channels); returns left and right outputs.
struct StereoOut
{
    std::vector<double> l, r;
};
inline StereoOut runEngine (MotionFilter& e, const std::vector<double>& in, const Transport& t = {}, int block = 256)
{
    StereoOut o { in, in };
    Transport tr = t;
    const double ppqPerSample = t.bpm / 60.0 / e.getSampleRate();
    for (size_t i = 0; i < in.size(); i += size_t (block))
    {
        const int n = int (std::min (in.size() - i, size_t (block)));
        e.process (o.l.data() + i, o.r.data() + i, (const double*) nullptr, (const double*) nullptr, n, tr);
        tr.ppq += ppqPerSample * n;
    }
    return o;
}

// Gain of the whole engine (left channel) at frequency f.
inline double engineGain (const Settings& s, double fs, double f, double amplitude = 0.0316, double settle = 0.3,
                          double measure = 0.2)
{
    auto e = makeEngine (s, fs);
    const double st = std::max (settle, 8.0 / f);
    const size_t ns = size_t (st * fs);
    const size_t n = wholePeriods (f, fs, std::max (measure, 10.0 / f));
    std::vector<double> x (ns + n);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = amplitude * std::sin (2.0 * pi * f * double (i) / fs);
    auto o = runEngine (e, x);
    return toneAmplitude (o.l, ns, n, f, fs) / amplitude;
}

// Process sample by sample (block size 1) and record a telemetry quantity after every sample.
inline std::vector<double> traceTelemetry (MotionFilter& e, size_t samples, const std::function<double (const MotionEngine::Telemetry&)>& what,
                                           const std::function<double (size_t)>& input = {}, Transport t = {})
{
    std::vector<double> out (samples);
    const double ppqPerSample = t.bpm / 60.0 / e.getSampleRate();
    for (size_t i = 0; i < samples; ++i)
    {
        double l = input ? input (i) : 0.0, r = l;
        e.process (&l, &r, (const double*) nullptr, (const double*) nullptr, 1, t);
        out[i] = what (e.getTelemetry());
        t.ppq += ppqPerSample;
    }
    return out;
}

// Base-rate latency of the top-level processor at fs.
inline int latencyFor (double fs)
{
    MotionFilter m;
    m.prepare (fs);
    return m.getLatencySamples();
}

// Left-channel impulse response of the complete processor.
inline std::vector<double> impulseResponse (const Settings& s, double fs, double seconds, double amp = 0.0316)
{
    auto e = makeEngine (s, fs);
    std::vector<double> x (size_t (seconds * fs), 0.0);
    x[0] = amp;
    auto o = runEngine (e, x, {}, 1024);
    for (auto& v : o.l)
        v /= amp;
    return o.l;
}

// |DFT| of y at frequency f (complex response of an impulse response).
inline double dftMagnitude (const std::vector<double>& y, double f, double fs)
{
    double re = 0, im = 0;
    const double w = 2.0 * pi * f / fs;
    // rotate with a recursive oscillator for speed
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

inline double octavesFrom (double hz, double refHz) { return std::log2 (hz / refHz); }

} // namespace mftest
