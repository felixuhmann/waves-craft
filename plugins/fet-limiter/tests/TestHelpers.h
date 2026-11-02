// Shared helpers for the Craft FET Limiter unit tests.
#pragma once

#include <cmath>
#include <complex>
#include <string>
#include <vector>

#include "dsp/FetEngine.h"

namespace fettest
{
using namespace craft::fet_limiter::dsp;

inline constexpr double twoPi = 6.283185307179586476925;

inline double dbToAmp (double db) { return std::pow (10.0, db / 20.0); }
inline double ampToDb (double a) { return 20.0 * std::log10 (std::max (a, 1.0e-300)); }

inline Settings neutralSettings()
{
    Settings s;
    s.analog = Analog::Off;
    return s;
}

// Runs a mono / stereo signal through a freshly prepared engine. Channels are processed in place.
struct Rig
{
    FetEngine engine;
    double fs;

    explicit Rig (const Settings& s, double sampleRate = 48000.0) : fs (sampleRate)
    {
        engine.setSettings (s);
        engine.prepare (fs);
    }

    void run (std::vector<double>& mono, int block = 256)
    {
        for (size_t i = 0; i < mono.size(); i += size_t (block))
        {
            double* p = mono.data() + i;
            engine.process (&p, 1, int (std::min<size_t> (size_t (block), mono.size() - i)));
        }
    }

    void run (std::vector<double>& l, std::vector<double>& r, int block = 256)
    {
        for (size_t i = 0; i < l.size(); i += size_t (block))
        {
            double* p[2] = { l.data() + i, r.data() + i };
            engine.process (p, 2, int (std::min<size_t> (size_t (block), l.size() - i)));
        }
    }
};

inline std::vector<double> sine (double freq, double peakDb, double seconds, double fs, double phase = 0.0)
{
    std::vector<double> x (size_t (seconds * fs));
    const double a = dbToAmp (peakDb);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = a * std::sin (twoPi * freq * double (i) / fs + phase);
    return x;
}

// Complex amplitude (peak) of frequency f in x[start, start + n) (n should hold an integer number of cycles).
inline std::complex<double> tone (const std::vector<double>& x, double f, double fs, size_t start, size_t n)
{
    std::complex<double> acc = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double ph = twoPi * f * double (start + i) / fs;
        acc += x[start + i] * std::complex<double> (std::cos (ph), -std::sin (ph));
    }
    return acc * (2.0 / double (n));
}

inline double toneDb (const std::vector<double>& x, double f, double fs, size_t start, size_t n)
{
    return ampToDb (std::abs (tone (x, f, fs, start, n)));
}

// Number of samples holding an integer number of cycles of f closest to (and not above) maxSamples.
inline size_t wholeCycles (double f, double fs, size_t maxSamples)
{
    const double period = fs / f;
    const double cycles = std::floor (double (maxSamples) / period);
    return size_t (std::llround (cycles * period));
}

inline double rmsDb (const std::vector<double>& x, size_t start, size_t n)
{
    double s = 0.0;
    for (size_t i = start; i < start + n; ++i)
        s += x[i] * x[i];
    return ampToDb (std::sqrt (s / double (n)));
}

} // namespace fettest
