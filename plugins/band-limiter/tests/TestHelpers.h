// Band limiter tests: common signal and measurement helpers.
#pragma once

#include <cmath>
#include <utility>
#include <vector>

#include "dsp/BandLimiterEngine.h"

namespace helpers
{
using namespace craft::band_limiter::dsp;

inline constexpr double twoPi = 6.283185307179586;

inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
inline double linToDb (double v) { return 20.0 * std::log10 (std::max (v, 1.0e-15)); }

// Settings without requantisation effects beyond 24-bit rounding.
inline Settings neutral()
{
    Settings s;
    s.dither = Dither::None;
    s.shaping = Shaping::None;
    return s;
}

struct Rig
{
    Rig (Variant v, double fs, const Settings& s) : engine (v)
    {
        engine.setSettings (s);
        engine.prepare (fs);
    }

    std::vector<double> run (const std::vector<double>& x, std::vector<double>* right = nullptr)
    {
        std::vector<double> l = x, r = right != nullptr ? *right : x;
        double* ch[2] = { l.data(), r.data() };
        for (size_t i = 0; i < l.size(); i += 256)
        {
            double* p[2] = { ch[0] + i, ch[1] + i };
            engine.process (p, 2, int (std::min<size_t> (256, l.size() - i)));
        }
        if (right != nullptr)
            *right = r;
        return l;
    }

    BandLimiterEngine engine;
};

using Tone = std::pair<double, double>; // Hz, dBFS

inline std::vector<double> tones (const std::vector<Tone>& list, double fs, int n)
{
    std::vector<double> x (size_t (n), 0.0);
    for (const auto& [f, db] : list)
    {
        const double a = dbToLin (db);
        for (int i = 0; i < n; ++i)
            x[size_t (i)] += a * std::sin (twoPi * f * double (i) / fs);
    }
    return x;
}

// Amplitude of frequency f over [start, start + len), Hann windowed.
inline double toneAmplitude (const std::vector<double>& y, double f, double fs, int start, int len)
{
    double re = 0.0, im = 0.0, wsum = 0.0;
    for (int i = 0; i < len; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (twoPi * (double (i) + 0.5) / double (len));
        const double p = twoPi * f * double (start + i) / fs;
        re += w * y[size_t (start + i)] * std::cos (p);
        im += w * y[size_t (start + i)] * std::sin (p);
        wsum += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

inline double peak (const std::vector<double>& y, int start, int len)
{
    double p = 0.0;
    for (int i = start; i < start + len && i < int (y.size()); ++i)
        p = std::max (p, std::abs (y[size_t (i)]));
    return p;
}

} // namespace helpers
