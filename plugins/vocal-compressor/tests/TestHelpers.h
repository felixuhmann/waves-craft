#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "dsp/VocalCompressorEngine.h"

namespace testutil
{
using namespace craft::vocal;

constexpr double pi = 3.14159265358979323846;
// Constant probe on the right channel: far below every test level, so it never drives the linked detector, and
// y_R / probe is the applied gain of each aligned sample.
constexpr double probe = 1.0e-7;

inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
inline double linToDb (double g) { return 20.0 * std::log10 (g); }

// A level schedule for a 1 kHz sine: `levels[i]` (dBFS peak, -999 = silence) holds for `samples[i]` samples.
struct Segment
{
    double levelDb;
    long samples;
};

inline std::vector<double> sineSchedule (const std::vector<Segment>& segs, double fs, double freq = 1000.0)
{
    std::vector<double> x;
    long n = 0;
    for (const auto& s : segs)
    {
        const double amp = s.levelDb < -900.0 ? 0.0 : dbToLin (s.levelDb);
        for (long i = 0; i < s.samples; ++i, ++n)
            x.push_back (amp * std::sin (2.0 * pi * freq * double (n) / fs));
    }
    return x;
}

inline long ms (double milliseconds, double fs) { return std::lround (milliseconds * 1.0e-3 * fs); }

struct Result
{
    std::vector<double> y;    // aligned output of the signal channel
    std::vector<double> gain; // aligned applied gain (from the probe channel), output factor 0.99 included
};

// Runs a stereo engine (signal on L, probe on R) in blocks and returns the latency-compensated output.
inline Result run (const std::vector<double>& x, double fs, const Settings& s, int block = 512)
{
    VocalCompressorEngine e;
    e.prepare (fs, 2);
    e.setSettings (s);
    e.reset();
    const int H = e.getLatencySamples();
    const std::size_t n = x.size() + std::size_t (H);
    std::vector<double> l (n, 0.0), r (n, probe);
    std::copy (x.begin(), x.end(), l.begin());
    for (std::size_t pos = 0; pos < n; pos += std::size_t (block))
    {
        const int len = int (std::min<std::size_t> (std::size_t (block), n - pos));
        double* ptrs[2] = { l.data() + pos, r.data() + pos };
        e.process (ptrs, len);
    }
    Result res;
    res.y.assign (l.begin() + H, l.begin() + H + std::ptrdiff_t (x.size()));
    res.gain.resize (x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        res.gain[i] = r[i + std::size_t (H)] / probe;
    return res;
}

// Gain in dB without the 0.99 output factor.
inline double gainDb (const Result& r, long i) { return linToDb (r.gain[std::size_t (i)] / VocalCompressorEngine::outputFactor); }

// Static compressor law of section 9.1 (0.99 excluded), level L in dBFS.
inline double staticGainDb (double compressionDb, double levelDb)
{
    const double k = dbToLin (-compressionDb);
    return linToDb (k / (1.0 + (k - 1.0) * dbToLin (levelDb)));
}

inline Settings settings (double comp, double gate = param::Gate::offValue, double gain = 0.0)
{
    Settings s;
    s.compressionDb = comp;
    s.gateDb = gate;
    s.gainDb = gain;
    return s;
}

// Release-table tolerance (section 8): +-1.5 dB or +-25 % of the tabulated value, whichever is larger.
inline double releaseTolerance (double ref) { return std::max (1.5, 0.25 * std::abs (ref)); }

} // namespace testutil
