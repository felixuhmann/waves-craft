// Craft Flanger - test helpers and an independent reference model of spec r1 sections 3-6.
#pragma once

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "dsp/FlangerEngine.h"

namespace testutil
{

using namespace craft::flanger::dsp;

inline constexpr double pi = 3.14159265358979323846;

struct Stereo
{
    std::vector<double> l, r;
};

// A prepared engine whose smoothers are settled on `s` (static behaviour, spec 7).
inline FlangerEngine makeEngine (const Settings& s, double fs, Variant v = Variant::Stereo)
{
    FlangerEngine e (v);
    e.prepare (fs);
    e.setSettings (s);
    e.reset();
    return e;
}

// Process two input signals through an engine in blocks of `block` samples.
inline Stereo run (FlangerEngine& e, const std::vector<double>& in1, const std::vector<double>& in2, int block = 256)
{
    Stereo o { in1, in2 };
    const int n = int (in1.size());
    for (int i = 0; i < n; i += block)
    {
        const int len = std::min (block, n - i);
        e.process (o.l.data() + i, o.r.data() + i, len);
    }
    return o;
}

inline Stereo render (const Settings& s, double fs, const std::vector<double>& in1, const std::vector<double>& in2,
                      Variant v = Variant::Stereo)
{
    auto e = makeEngine (s, fs, v);
    return run (e, in1, in2);
}

inline std::vector<double> impulse (int n, int at = 0, double amp = 1.0)
{
    std::vector<double> v (size_t (n), 0.0);
    v[size_t (at)] = amp;
    return v;
}

inline std::vector<double> noise (int n, unsigned seed, double amplitude = 0.5)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<double> d (-amplitude, amplitude);
    std::vector<double> v (static_cast<size_t> (n));
    for (auto& x : v)
        x = d (rng);
    return v;
}

inline double peakAbs (const std::vector<double>& v)
{
    double p = 0.0;
    for (double x : v)
        p = std::max (p, std::abs (x));
    return p;
}

inline double maxAbsDiff (const std::vector<double>& a, const std::vector<double>& b)
{
    double m = 0.0;
    for (size_t i = 0; i < std::min (a.size(), b.size()); ++i)
        m = std::max (m, std::abs (a[i] - b[i]));
    return m;
}

// error in dB relative to the peak of the reference
inline double errorDb (const std::vector<double>& got, const std::vector<double>& ref)
{
    const double e = maxAbsDiff (got, ref), p = peakAbs (ref);
    if (e == 0.0)
        return -400.0;
    return 20.0 * std::log10 (e / p);
}

// ---------------------------------------------------------------------------------------------
// Straight-line model written directly from the spec equations, sample index n from 0, absolute
// indexing into whole-signal arrays (no ring buffers, no smoothing, phase computed from n, not
// accumulated). Static settings only.
struct ReferenceModel
{
    static std::vector<double> kernel (double fc, double fs, bool highPass)
    {
        std::vector<double> h (9);
        double sum = 0.0;
        for (int n = -4; n <= 4; ++n)
        {
            const double x = 2.0 * fc * n / fs;
            const double sincv = n == 0 ? 1.0 : std::sin (pi * x) / (pi * x);
            const double w = std::cyl_bessel_i (0.0, 4.0 * std::sqrt (1.0 - (n / 4.0) * (n / 4.0))) / std::cyl_bessel_i (0.0, 4.0);
            h[size_t (n + 4)] = (2.0 * fc / fs) * sincv * w;
            sum += h[size_t (n + 4)];
        }
        for (auto& v : h)
            v = highPass ? -v : v / sum;
        if (highPass)
            h[4] += 1.0;
        return h;
    }

    static double tri (double theta)
    {
        // 0 -> +1 at pi/2, -1 at 3pi/2, linear in between
        double p = std::fmod (theta, 2.0 * pi);
        if (p < 0) p += 2.0 * pi;
        p /= 2.0 * pi;
        if (p < 0.25) return 4.0 * p;
        if (p < 0.75) return 2.0 - 4.0 * p;
        return 4.0 * p - 4.0;
    }

    // Returns the output of one channel processor with LFO phase offset phi (radians).
    static std::vector<double> channel (const Settings& s, double fs, const std::vector<double>& x, double phi,
                                        std::vector<double>* trajectory = nullptr)
    {
        const int N = int (x.size());
        const double Gin = std::pow (10.0, s.gainDb / 20.0);
        const int D = int (std::floor (std::round (s.delayMs * 10.0) / 10.0 * fs / 1000.0 + 1e-9));
        const double depth = std::round (s.depthPct) / 100.0;
        const double F = std::round (s.feedbackPct) / 100.0;
        const double Floop = std::min (F, 0.999) * (s.feedbackInvert ? -1.0 : 1.0);
        const double gwet = 1.0 - std::pow (F, 20.0 * std::round (s.delayMs * 10.0) / 10.0) / 2.0;
        const double m = std::round (s.mixPct) / 100.0;
        const double swet = s.wetInvert ? -1.0 : 1.0;
        double rate = s.rateHz < 0.01 - 1e-9 ? 0.0 : s.rateHz;
        if (s.tempoSync)
            rate *= s.bpm / 60.0;
        if (s.stop)
            rate = 0.0;
        const int c = int (std::floor (std::round (s.depthPct) * D / 100.0 + 1e-9));
        const auto h = kernel (s.filterHz, fs, s.filterHighPass);

        std::vector<double> v (size_t (N), 0.0), w (size_t (N), 0.0), y (size_t (N), 0.0);
        auto at = [] (const std::vector<double>& a, long i) { return i < 0 ? 0.0 : a[size_t (i)]; };
        for (int n = 0; n < N; ++n)
        {
            const double theta = 2.0 * pi * rate * n / fs + phi;
            const double L = s.triangle ? tri (theta) : std::sin (theta);
            double d = s.tape ? std::max (1.0, c + (c - 1.0) * L) : D + 0.95 * depth * D * L;
            d = std::max (1.0, d); // assumption documented in IMPLEMENTATION_NOTES (normal mode, tiny D)
            if (trajectory)
                trajectory->push_back (d);
            const long k = long (std::floor (d));
            const double a = d - double (k);
            w[size_t (n)] = (1.0 - a) * at (v, n - k) + a * at (v, n - k - 1);
            v[size_t (n)] = Gin * x[size_t (n)] + Floop * w[size_t (n)];

            double wf = w[size_t (n)];
            if (s.filterOn)
            {
                wf = 0.0;
                for (int j = 0; j < 9; ++j)
                    wf += h[size_t (j)] * at (w, n - j);
            }
            const double dry = s.tape ? Gin * at (x, n - std::max (1, c)) : Gin * x[size_t (n)];
            y[size_t (n)] = std::sqrt (1.0 - m) * dry + std::sqrt (m) * swet * gwet * wf;
        }
        return y;
    }

    static double rightOffset (const Settings& s)
    {
        return std::floor (std::round (s.stereoDeg) * 1024.0 / 360.0) * 2.0 * pi / 1024.0;
    }
};

} // namespace testutil
