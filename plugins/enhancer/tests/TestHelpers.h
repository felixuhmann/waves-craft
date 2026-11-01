// Craft Enhancer tests - shared helpers.
#pragma once

#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "dsp/EnhancerEngine.h"

namespace enh_test
{
using namespace craft::enhancer::dsp;
namespace t = craft::enhancer::tables;

inline constexpr std::array<double, 4> tableCrossovers { 88.0, 494.0, 2576.0, 9285.0 };
inline constexpr std::array<double, 4> set2Crossovers { 197.0, 990.0, 3978.0, 11977.0 };
inline constexpr double bandCentres[5] = { 45.0, 200.0, 1100.0, 5000.0, 14000.0 };

inline double toDb (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-30)); }

// Processed-only settings: one band at `gainDb`, direct Off, table crossovers.
inline Settings processedOnly (int band, double punch, double gainDb = 24.0)
{
    Settings s;
    s.crossoverHz = tableCrossovers;
    s.directDb = param::Direct::offValue;
    s.bandDb[(size_t) band] = gainDb;
    s.punch = punch;
    return s;
}

struct Stereo
{
    std::vector<double> l, r;
};

inline void runEngine (EnhancerEngine& e, Stereo& io)
{
    double* ch[2] = { io.l.data(), io.r.data() };
    const int n = (int) io.l.size();
    for (int i = 0; i < n; i += 512)
    {
        double* p[2] = { ch[0] + i, ch[1] + i };
        e.process (p, std::min (512, n - i));
    }
}

inline Stereo run (const Settings& s, Stereo in, double fs = 48000.0)
{
    auto e = std::make_unique<EnhancerEngine>();
    e->prepare (fs, 2);
    e->setSettings (s);
    e->snapSmoothers();
    e->reset();
    runEngine (*e, in);
    return in;
}

struct Segment
{
    double levelDb, seconds;
};

// Sine with piecewise constant level (phase continuous), identical in both channels.
inline Stereo tone (double f, const std::vector<Segment>& segs, double fs = 48000.0)
{
    Stereo x;
    double phase = 0.0;
    for (const auto& seg : segs)
    {
        const double a = std::pow (10.0, seg.levelDb / 20.0);
        const int n = (int) std::lround (seg.seconds * fs);
        for (int i = 0; i < n; ++i)
        {
            const double v = a * std::sin (phase);
            x.l.push_back (v);
            x.r.push_back (v);
            phase += 2.0 * pi * f / fs;
        }
    }
    return x;
}

// Amplitude of the component at f in y[start, start + len) (rectangular window over whole periods).
inline double amplitudeAt (const std::vector<double>& y, double f, double fs, std::size_t start, std::size_t len)
{
    double re = 0.0, im = 0.0;
    start = std::min (start, y.size());
    len = std::min (len, y.size() - start);
    const double w = 2.0 * pi * f / fs;
    for (std::size_t i = 0; i < len; ++i)
    {
        re += y[start + i] * std::cos (w * double (i));
        im += y[start + i] * std::sin (w * double (i));
    }
    return 2.0 * std::hypot (re, im) / std::max (1.0, double (len));
}

inline std::size_t wholePeriods (double f, double fs, double minSeconds)
{
    const double periods = std::max (1.0, std::ceil (minSeconds * f));
    return (std::size_t) std::lround (periods * fs / f);
}

// Hann-windowed amplitude (clean separation of harmonics in a long steady section).
inline double hannAmplitude (const std::vector<double>& y, double f, double fs, std::size_t start, std::size_t len)
{
    double re = 0.0, im = 0.0, sum = 0.0;
    const double w = 2.0 * pi * f / fs;
    for (std::size_t i = 0; i < len; ++i)
    {
        const double h = 0.5 - 0.5 * std::cos (2.0 * pi * double (i) / double (len));
        sum += h;
        re += h * y[start + i] * std::cos (w * double (i));
        im += h * y[start + i] * std::sin (w * double (i));
    }
    return 2.0 * std::hypot (re, im) / sum;
}

} // namespace enh_test
