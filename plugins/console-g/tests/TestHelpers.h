// Craft Console G - shared test helpers.
#pragma once

#include <cmath>
#include <functional>
#include <vector>

#include "dsp/ConsoleEngine.h"

namespace cg = craft::consoleg::dsp;
namespace cgt = craft::consoleg::spec_tables;

namespace testing
{

inline const cg::Tapers& tapers() { return cg::Tapers::get(); }
inline double rOf (cg::TaperId id, double value) { return tapers()[id].rForValue (value); }

// Engine prepared at fs with the given settings (defaults otherwise).
inline cg::ConsoleEngine makeEngine (double fs, const cg::Settings& s = cg::Settings::defaults())
{
    cg::ConsoleEngine e;
    e.setSettings (s);
    e.prepare (fs);
    return e;
}

inline double rms (const std::vector<double>& v, size_t from, size_t to)
{
    double a = 0.0;
    for (size_t i = from; i < to; ++i)
        a += v[i] * v[i];
    return std::sqrt (a / double (std::max<size_t> (1, to - from)));
}

// Runs a mono or stereo signal through an engine (block size 256).
inline void run (cg::ConsoleEngine& e, std::vector<double>& l, std::vector<double>* r = nullptr)
{
    const size_t n = l.size();
    for (size_t pos = 0; pos < n; pos += 256)
    {
        const int len = int (std::min<size_t> (256, n - pos));
        double* ptrs[2] = { l.data() + pos, r != nullptr ? r->data() + pos : nullptr };
        e.process (ptrs, r != nullptr ? 2 : 1, len);
    }
}

// Steady-state magnitude (dB) of the whole strip at frequency f, sine at levelDb dBFS (peak).
inline double stripMagnitudeDb (const cg::Settings& s, double fs, double f, double levelDb = -30.0)
{
    auto e = makeEngine (fs, s);
    const double a = cg::dbToGain (levelDb);
    const size_t settle = size_t (fs * 0.25), measure = size_t (fs * 0.1);
    std::vector<double> x (settle + measure), y;
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = a * std::sin (2.0 * cg::pi * f * double (i) / fs);
    y = x;
    run (e, y);
    return cg::gainToDb (rms (y, settle, y.size()) / rms (x, settle, x.size()));
}

// Rising staircase of 1 kHz sine steps (spec 9.2): 0.3 s silence, then 2 dB steps of 0.3 s from `from` to `to` dBFS
// (peak); per step the output/input gain in dB over its last 0.1 s. `down` runs the levels in reverse order.
inline std::vector<double> staircase (const cg::Settings& s, double fs, double from = -54.0, double to = 0.0,
                                      bool down = false, int channels = 1)
{
    auto e = makeEngine (fs, s);
    std::vector<double> levels;
    for (double L = from; L <= to + 1e-9; L += 2.0)
        levels.push_back (L);
    if (down)
        std::reverse (levels.begin(), levels.end());
    const size_t step = size_t (fs * 0.3), tail = size_t (fs * 0.1);
    std::vector<double> x (step * (levels.size() + 1), 0.0);
    for (size_t k = 0; k < levels.size(); ++k)
    {
        const double a = cg::dbToGain (levels[k]);
        for (size_t i = 0; i < step; ++i)
        {
            const size_t n = step * (k + 1) + i;
            x[n] = a * std::sin (2.0 * cg::pi * 1000.0 * double (n) / fs);
        }
    }
    std::vector<double> y = x, y2 = x;
    run (e, y, channels == 2 ? &y2 : nullptr);
    std::vector<double> gains (levels.size());
    for (size_t k = 0; k < levels.size(); ++k)
    {
        const size_t end = step * (k + 2), begin = end - tail;
        const size_t idx = down ? levels.size() - 1 - k : k; // result ordered by ascending level
        gains[idx] = cg::gainToDb (rms (y, begin, end) / rms (x, begin, end));
    }
    return gains;
}

// Display-domain settings helpers.
inline cg::Settings comp (double T, double R, bool fast = false, double release = 0.4)
{
    auto s = cg::Settings::defaults();
    s.compThreshold = rOf (cg::TaperId::compThreshold, T);
    s.compRatio = rOf (cg::TaperId::compRatio, R);
    s.compFast = fast;
    s.compRelease = rOf (cg::TaperId::compRelease, release);
    return s;
}

inline cg::Settings expander (cg::Settings s, double T, double range, bool gate = false)
{
    s.expThreshold = rOf (cg::TaperId::expThreshold, T);
    s.expRange = rOf (cg::TaperId::expRange, range);
    s.expGate = gate;
    return s;
}

// The magnitude response of a section design at a digital frequency.
inline double sectionDb (const cg::BiquadCoeffs& c, double f, double fs) { return c.magnitudeDb (f, fs); }

inline const std::vector<double>& vectorFreqs()
{
    static const std::vector<double> f { 30, 60, 100, 200, 500, 1000, 2000, 3500, 5000, 8000, 12000, 16000, 20000 };
    return f;
}

} // namespace testing
