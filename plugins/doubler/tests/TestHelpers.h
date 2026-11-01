// Shared helpers for the doubler tests: engine setup, signals and the measurement tools of spec
// section 11 (FFT-based analytic signal, phase-slope pitch, 1/3-octave band levels).
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <random>
#include <vector>

#include "../source/dsp/DoublerEngine.h"

namespace testutil
{
using namespace craft::doubler;
using cd = std::complex<double>;

// "voice 1 only, gain 0 dB, Detune 0, Depth 0, EQ off, direct off" (spec section 11.1)
inline Settings soloSettings()
{
    Settings s = defaultSettings (4);
    s.directOn = false;
    s.eqLowOn = s.eqHighOn = false;
    for (auto& v : s.voices)
        v.on = false;
    auto& v = s.voices[0];
    v.on = true;
    v.gainDb = 0.0;
    v.panDeg = 0.0;
    v.delayMs = 20.0;
    v.feedbackPct = 0.0;
    v.octave = false;
    v.detuneCents = 0.0;
    v.depthCents = 0.0;
    v.rateHz = 1.0;
    return s;
}

// direct path only
inline Settings directSettings()
{
    Settings s = soloSettings();
    s.voices[0].on = false;
    s.directOn = true;
    s.directGainDb = 0.0;
    s.directPanDeg = 0.0;
    return s;
}

inline DoublerEngine makeEngine (const Settings& s, double fs, int voices = 4)
{
    DoublerEngine e (voices);
    e.prepare (fs);
    e.setSettings (s);
    e.reset();
    return e;
}

struct Stereo
{
    std::vector<double> l, r;
};

inline Stereo run (DoublerEngine& e, std::vector<double> l, std::vector<double> r)
{
    e.process (l.data(), r.data(), int (l.size()));
    return { std::move (l), std::move (r) };
}

inline std::vector<double> noise (int n, unsigned seed, double amplitude = 0.5)
{
    std::mt19937 rng (seed);
    std::normal_distribution<double> d (0.0, amplitude * 0.3);
    std::vector<double> v (static_cast<size_t> (n));
    for (auto& x : v)
        x = d (rng);
    return v;
}

inline std::vector<double> sine (int n, double f, double fs, double amplitude = 0.5)
{
    std::vector<double> v (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i)
        v[size_t (i)] = amplitude * std::sin (2.0 * pi * f * i / fs);
    return v;
}

inline double rms (const std::vector<double>& v, size_t from, size_t to)
{
    double acc = 0.0;
    for (size_t i = from; i < to; ++i)
        acc += v[i] * v[i];
    return std::sqrt (acc / double (to - from));
}

inline double maxAbsDiff (const std::vector<double>& a, const std::vector<double>& b)
{
    double m = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        m = std::max (m, std::abs (a[i] - b[i]));
    return m;
}

inline double peakAbs (const std::vector<double>& a)
{
    double m = 0.0;
    for (auto x : a)
        m = std::max (m, std::abs (x));
    return m;
}

// --- radix-2 FFT ------------------------------------------------------------------------------
inline void fft (std::vector<cd>& a, bool inverse)
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
        const double ang = 2.0 * pi / double (len) * (inverse ? 1.0 : -1.0);
        const cd wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            cd w (1.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const cd u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    if (inverse)
        for (auto& x : a)
            x /= double (n);
}

// Analytic signal (Hilbert transform) of x[from .. from + 2^k).
inline std::vector<cd> analytic (const std::vector<double>& x, size_t from, size_t n)
{
    std::vector<cd> a (n);
    for (size_t i = 0; i < n; ++i)
        a[i] = x[from + i];
    fft (a, false);
    for (size_t i = 1; i < n / 2; ++i)
        a[i] *= 2.0;
    for (size_t i = n / 2 + 1; i < n; ++i)
        a[i] = 0.0;
    fft (a, true);
    return a;
}

inline double percentile (std::vector<double> v, double p)
{
    std::sort (v.begin(), v.end());
    return v[size_t (std::clamp (p, 0.0, 1.0) * double (v.size() - 1))];
}

// Pitch in cents relative to fRef from the phase slope of the output: complex demodulation at fRef,
// 5 ms block averages, unwrapped phase, least-squares slope.
inline double pitchCents (const std::vector<double>& y, size_t from, size_t to, double fRef, double fs)
{
    const size_t block = size_t (0.005 * fs);
    std::vector<double> t, ph;
    double prev = 0.0, unwrap = 0.0;
    bool first = true;
    for (size_t b = from; b + block <= to; b += block)
    {
        cd acc = 0.0;
        for (size_t i = b; i < b + block; ++i)
            acc += y[i] * std::polar (1.0, -2.0 * pi * fRef * double (i) / fs);
        double p = std::arg (acc);
        if (! first)
        {
            double d = p - prev;
            while (d > pi) d -= 2.0 * pi;
            while (d < -pi) d += 2.0 * pi;
            unwrap += d;
        }
        else
            unwrap = p;
        first = false;
        prev = p;
        t.push_back ((double (b) + 0.5 * double (block)) / fs);
        ph.push_back (unwrap);
    }
    double mt = 0, mp = 0;
    for (size_t i = 0; i < t.size(); ++i)
    {
        mt += t[i];
        mp += ph[i];
    }
    mt /= double (t.size());
    mp /= double (t.size());
    double num = 0, den = 0;
    for (size_t i = 0; i < t.size(); ++i)
    {
        num += (t[i] - mt) * (ph[i] - mp);
        den += (t[i] - mt) * (t[i] - mt);
    }
    const double slopeHz = num / den / (2.0 * pi);
    return 1200.0 * std::log2 ((fRef + slopeHz) / fRef);
}

// Welch power spectrum (Hann, 50 % overlap) of x[from, to).
inline std::vector<double> powerSpectrum (const std::vector<double>& x, size_t from, size_t to, size_t n)
{
    std::vector<double> acc (n / 2 + 1, 0.0), win (n);
    for (size_t i = 0; i < n; ++i)
        win[i] = 0.5 - 0.5 * std::cos (2.0 * pi * double (i) / double (n));
    std::vector<cd> buf (n);
    for (size_t s = from; s + n <= to; s += n / 2)
    {
        for (size_t i = 0; i < n; ++i)
            buf[i] = x[s + i] * win[i];
        fft (buf, false);
        for (size_t k = 0; k <= n / 2; ++k)
            acc[k] += std::norm (buf[k]);
    }
    return acc;
}

// 1/3-octave band powers (IEC centre frequencies 100 Hz .. 16 kHz) from a power spectrum.
inline std::vector<double> thirdOctaveBands (const std::vector<double>& ps, double fs, size_t n)
{
    std::vector<double> out;
    for (int k = 20; k <= 42; ++k) // 10^(k/10): 100 Hz .. 15.85 kHz
    {
        const double fc = std::pow (10.0, k / 10.0);
        const double lo = fc / std::pow (2.0, 1.0 / 6.0), hi = fc * std::pow (2.0, 1.0 / 6.0);
        double p = 0.0;
        for (size_t b = 0; b < ps.size(); ++b)
        {
            const double f = double (b) * fs / double (n);
            if (f >= lo && f < hi)
                p += ps[b];
        }
        out.push_back (p);
    }
    return out;
}

} // namespace testutil
