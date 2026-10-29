// Measurement helpers shared by the unit tests and the calibration tool: they reproduce the spec's black-box
// measurement procedures (steady sines, bursts on a -50 dBFS bed, equal-RMS noise) against the DSP engine.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

#include "dsp/LevelerEngine.h"

namespace optotest
{
namespace dsp = craft::opto_leveler::dsp;
namespace spec = craft::opto_leveler::spec;

constexpr double pi = 3.14159265358979323846;

inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
inline double linToDb (double v) { return 20.0 * std::log10 (std::max (v, 1.0e-300)); }

inline dsp::Settings makeSettings (double pr, dsp::Mode mode = dsp::Mode::Compress, double hiFreq = 100.0,
                                   double gain = 32.28)
{
    dsp::Settings s;
    s.peakReduction = pr;
    s.mode = mode;
    s.hiFreq = hiFreq;
    s.gain = gain;
    s.analog = dsp::AnalogNoise::Type::Off;
    s.mixPercent = 100.0;
    s.trimDb = 0.0;
    return s;
}

struct Engine
{
    dsp::LevelerEngine e;
    explicit Engine (const dsp::Settings& s, double fs = 48000.0, dsp::EngineTuning t = dsp::LevelerEngine::defaultTuning())
        : e (t)
    {
        e.setSettings (s);
        e.prepare (fs);
    }
};

// Steady-state GR (dB) of the cell for a steady sine: mean cell gain (incl. waveform following) over the last
// `avgSeconds` of `seconds`.
inline double steadyGrSine (const dsp::Settings& s, double freq, double peakDb, double fs = 48000.0,
                            double seconds = 6.0, dsp::EngineTuning t = dsp::LevelerEngine::defaultTuning())
{
    Engine en (s, fs, t);
    const int n = int (seconds * fs);
    const int avgFrom = n - int (0.2 * fs);
    const double a = dbToLin (peakDb), w = 2.0 * pi * freq / fs;
    double sum = 0.0;
    int cnt = 0;
    double buf[2];
    for (int i = 0; i < n; ++i)
    {
        buf[0] = a * std::sin (w * i);
        double* p[1] = { buf };
        en.e.process (p, 1, 1);
        if (i >= avgFrom)
        {
            sum += en.e.getCellGain();
            ++cnt;
        }
    }
    return -linToDb (sum / cnt);
}

// Single-bin DFT amplitude of x at frequency f (x must span whole cycles).
inline double toneAmplitude (const std::vector<double>& x, double f, double fs)
{
    std::complex<double> acc {};
    const double w = 2.0 * pi * f / fs;
    for (size_t i = 0; i < x.size(); ++i)
        acc += x[i] * std::complex<double> (std::cos (w * double (i)), -std::sin (w * double (i)));
    return 2.0 * std::abs (acc) / double (x.size());
}

struct Harmonics
{
    double gainDb = 0.0;
    double hDbc[6] {}; // index k = harmonic number (2 ... 5)
};

// Output harmonics of a steady sine through the whole engine (after `seconds` of settling).
inline Harmonics measureHarmonics (const dsp::Settings& s, double freq, double peakDb, double fs = 48000.0,
                                   double seconds = 4.0, dsp::EngineTuning t = dsp::LevelerEngine::defaultTuning())
{
    Engine en (s, fs, t);
    const double a = dbToLin (peakDb), w = 2.0 * pi * freq / fs;
    const int settle = int (seconds * fs);
    // analysis window: a whole number of cycles of length ~0.5 s with an integer number of samples per period
    const int cycles = std::max (1, int (std::round (0.5 * freq)));
    const int len = int (std::round (cycles * fs / freq));
    std::vector<double> out;
    out.reserve (size_t (len));
    const int block = 256;
    std::vector<double> buf (block);
    for (int start = 0; start < settle + len; start += block)
    {
        const int m = std::min (block, settle + len - start);
        for (int i = 0; i < m; ++i)
            buf[size_t (i)] = a * std::sin (w * double (start + i));
        double* p[1] = { buf.data() };
        en.e.process (p, 1, m);
        for (int i = 0; i < m; ++i)
            if (start + i >= settle)
                out.push_back (buf[size_t (i)]);
    }
    const double f0 = cycles * fs / double (len); // exact bin frequency of the analysed tone
    Harmonics h;
    const double h1 = toneAmplitude (out, f0, fs);
    h.gainDb = linToDb (h1 / a);
    for (int k = 2; k <= 5; ++k)
        h.hDbc[k] = k * f0 < 0.5 * fs ? linToDb (toneAmplitude (out, k * f0, fs) / h1) : -999.0;
    return h;
}

// T3 procedure: 1 kHz bed at -50 dBFS, burst of `burstDb` for `burstSeconds`, then the bed again. Records the
// cell GR (dB) at the given times after onset (attack) and after the burst end (release).
struct Trajectory
{
    std::vector<double> attack, release;
};

inline Trajectory measureBurst (const dsp::Settings& s, double burstDb, double burstSeconds,
                                const std::vector<double>& attackTimes, const std::vector<double>& releaseTimes,
                                double fs = 48000.0, dsp::EngineTuning t = dsp::LevelerEngine::defaultTuning())
{
    Engine en (s, fs, t);
    const double bed = dbToLin (-50.0), burst = dbToLin (burstDb), w = 2.0 * pi * 1000.0 / fs;
    const double pre = 1.0;
    const int onset = int (std::round (pre * fs));
    const int end = onset + int (std::round (burstSeconds * fs));
    double lastRelease = 0.0;
    for (double rt : releaseTimes)
        lastRelease = std::max (lastRelease, rt);
    const int total = end + int ((lastRelease + 0.01) * fs);

    std::vector<int> aIdx, rIdx;
    for (double at : attackTimes)
        aIdx.push_back (at <= burstSeconds + 1e-9 ? onset + int (std::round (at * fs)) - 1 : -1);
    for (double rt : releaseTimes)
        rIdx.push_back (end + int (std::round (rt * fs)) - 1);
    Trajectory tr;
    tr.attack.assign (attackTimes.size(), std::nan (""));
    tr.release.assign (releaseTimes.size(), std::nan (""));
    double buf;
    for (int i = 0; i < total; ++i)
    {
        const double amp = (i >= onset && i < end) ? burst : bed;
        buf = amp * std::sin (w * double (i));
        double* p[1] = { &buf };
        en.e.process (p, 1, 1);
        const double gr = -linToDb (en.e.getCell().gain());
        for (size_t k = 0; k < aIdx.size(); ++k)
            if (aIdx[k] == i)
                tr.attack[k] = gr;
        for (size_t k = 0; k < rIdx.size(); ++k)
            if (rIdx[k] == i)
                tr.release[k] = gr;
    }
    return tr;
}

// Deterministic white Gaussian noise
struct Gauss
{
    uint64_t s = 0x2545F4914F6CDD1Dull;
    bool has = false;
    double spare = 0.0;
    double uniform()
    {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return (double (s >> 11) + 0.5) * (1.0 / 9007199254740992.0);
    }
    double next()
    {
        if (has)
        {
            has = false;
            return spare;
        }
        const double u1 = uniform(), u2 = uniform();
        const double r = std::sqrt (-2.0 * std::log (u1));
        spare = r * std::sin (2.0 * pi * u2);
        has = true;
        return r * std::cos (2.0 * pi * u2);
    }
};

// Steady-state GR (dB) measured from the output level: output RMS change (vs. the PR 0 path) for a stimulus.
// kind 0 = 1 kHz sine, 1 = white noise; both at the given RMS level.
inline double steadyGrRms (const dsp::Settings& s, int kind, double rmsDb, double fs = 48000.0, double seconds = 6.0,
                           dsp::EngineTuning t = dsp::LevelerEngine::defaultTuning())
{
    auto run = [&] (const dsp::Settings& st) {
        Engine en (st, fs, t);
        Gauss g;
        const int n = int (seconds * fs), from = n - int (2.0 * fs);
        const double rms = dbToLin (rmsDb), w = 2.0 * pi * 1000.0 / fs;
        double acc = 0.0;
        double buf;
        for (int i = 0; i < n; ++i)
        {
            buf = kind == 0 ? rms * std::sqrt (2.0) * std::sin (w * i) : rms * g.next();
            double* p[1] = { &buf };
            en.e.process (p, 1, 1);
            if (i >= from)
                acc += buf * buf;
        }
        return 10.0 * std::log10 (acc / double (n - from));
    };
    auto ref = s;
    ref.peakReduction = 0.0;
    return run (ref) - run (s);
}

} // namespace optotest
