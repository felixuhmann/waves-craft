// Craft Multiband - test helpers: stimuli and engine runs.
#pragma once

#include <cmath>
#include <random>
#include <utility>
#include <vector>

#include "dsp/MultibandEngine.h"

namespace craft::multiband::test
{

inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
inline double linToDb (double v) { return 20.0 * std::log10 (std::max (v, 1.0e-30)); }

// Concatenated constant-level sine segments (duration s, level dBFS sine peak), continuous phase.
inline std::vector<double> toneSegments (double fs, double freq, const std::vector<std::pair<double, double>>& segs)
{
    std::vector<double> x;
    double ph = 0.0;
    const double inc = 2.0 * pi * freq / fs;
    for (const auto& [dur, db] : segs)
    {
        const int n = int (std::lround (dur * fs));
        const double a = dbToLin (db);
        for (int i = 0; i < n; ++i)
        {
            x.push_back (a * std::sin (ph));
            ph += inc;
        }
    }
    return x;
}

// Settings of the section 5 test vectors: band 2 dynamic (Range, Threshold -30, Knee), the other bands neutral.
inline Settings band2Settings (double rangeDb, double knee, double attackMs, double releaseMs, bool opto, bool autoRelease)
{
    Settings s = Settings::defaults();
    s.knee = knee;
    s.opto = opto;
    s.autoRelease = autoRelease;
    for (auto& b : s.bands)
    {
        b.rangeDb = 0.0;
        b.gainDb = 0.0;
    }
    auto& b2 = s.bands[1];
    b2.rangeDb = rangeDb;
    b2.thresholdDb = -30.0;
    b2.attackMs = attackMs;
    b2.releaseMs = releaseMs;
    return s;
}

// Runs a mono signal through a stereo engine (same signal on both channels unless `right` is given) and records the
// band gain (dB) of band `band` after every sample.
inline std::vector<double> runBandGain (const Settings& s, double fs, const std::vector<double>& left, int band,
                                        const std::vector<double>* right = nullptr, int channels = 2)
{
    MultibandEngine e;
    e.setSettings (s);
    e.prepare (fs, channels);
    std::vector<double> g (left.size());
    for (size_t i = 0; i < left.size(); ++i)
    {
        double l = left[i], r = right != nullptr ? (*right)[i] : left[i];
        double* io[2] = { &l, &r };
        e.process (io, 1);
        g[i] = e.getBandGainDb (band);
    }
    return g;
}

// Runs a whole stereo signal through an engine in blocks; returns the outputs.
inline std::pair<std::vector<double>, std::vector<double>> runStereo (MultibandEngine& e, std::vector<double> l,
                                                                      std::vector<double> r, int block = 256)
{
    for (size_t i = 0; i < l.size(); i += size_t (block))
    {
        const int n = int (std::min (l.size() - i, size_t (block)));
        double* io[2] = { l.data() + i, r.data() + i };
        e.process (io, n);
    }
    return { l, r };
}

inline std::vector<double> whiteNoise (size_t n, unsigned seed, double scale = 1.0)
{
    std::mt19937 rng (seed);
    std::normal_distribution<double> nd;
    std::vector<double> x (n);
    for (auto& v : x)
        v = nd (rng) * scale;
    return x;
}

} // namespace craft::multiband::test
