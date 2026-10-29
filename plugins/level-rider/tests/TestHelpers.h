// Craft Level Rider - stimuli and measurement helpers shared by the unit tests (spec r1 section 12).
#pragma once

#include <cmath>
#include <random>
#include <utility>
#include <vector>

#include "dsp/RiderEngine.h"

namespace craft::levelrider::test
{

using namespace craft::levelrider::dsp;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) { return 20.0 * std::log10 (g); }

// Gaussian numbers from std::mt19937 via Box-Muller: unlike std::normal_distribution the sequence is the same
// with every standard library, so the noise stimuli (and the transitional table cells) match on all platforms.
class Gaussian
{
public:
    explicit Gaussian (unsigned seed) : rng (seed) {}
    double operator()()
    {
        if (haveSpare)
        {
            haveSpare = false;
            return spare;
        }
        const double u1 = (double (rng()) + 1.0) / 4294967297.0, u2 = double (rng()) / 4294967296.0;
        const double r = std::sqrt (-2.0 * std::log (u1));
        spare = r * std::sin (2.0 * pi * u2);
        haveSpare = true;
        return r * std::cos (2.0 * pi * u2);
    }

private:
    std::mt19937 rng;
    double spare = 0.0;
    bool haveSpare = false;
};

// RBJ cookbook biquad (only used to shape test stimuli).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    static Biquad make (bool highPass, double f, double q, double fs)
    {
        const double w = 2.0 * pi * f / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q), a0 = 1.0 + alpha;
        Biquad bq;
        if (highPass)
        {
            bq.b0 = (1.0 + cw) / 2.0 / a0;
            bq.b1 = -(1.0 + cw) / a0;
        }
        else
        {
            bq.b0 = (1.0 - cw) / 2.0 / a0;
            bq.b1 = (1.0 - cw) / a0;
        }
        bq.b2 = bq.b0;
        bq.a1 = -2.0 * cw / a0;
        bq.a2 = (1.0 - alpha) / a0;
        return bq;
    }
    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// "Band noise" of section 12: Gaussian noise band-passed 300-3000 Hz with 4th-order Butterworth high- and
// low-pass slopes, normalised to unit RMS. Continuous stream; the caller scales it per segment.
class BandNoise
{
public:
    BandNoise (double fs, unsigned seed = 7) : nd (seed)
    {
        const double qs[2] = { 0.54119610014619698, 1.3065629648763766 };
        for (int i = 0; i < 2; ++i)
        {
            hp[i] = Biquad::make (true, 300.0, qs[i], fs);
            lp[i] = Biquad::make (false, 3000.0, qs[i], fs);
        }
        // measure the RMS once (long run), then restart the stream
        double s = 0.0;
        const int n = int (fs * 20.0);
        for (int i = 0; i < n; ++i)
        {
            const double v = raw();
            if (i > int (fs))
                s += v * v;
        }
        norm = 1.0 / std::sqrt (s / double (n - int (fs) - 1));
    }
    double next() { return raw() * norm; }

private:
    double raw()
    {
        double v = nd();
        for (auto& b : hp)
            v = b.process (v);
        for (auto& b : lp)
            v = b.process (v);
        return v;
    }
    Gaussian nd;
    Biquad hp[2], lp[2];
    double norm = 1.0;
};

struct Segment
{
    double levelDb;  // RMS dBFS
    double seconds;
};

enum class Stimulus { BandNoise, Sine };

// Runs the engine on a stereo (L = R) stimulus of consecutive level segments and returns the gain in dB
// measured as 20 log10(rms(out) / rms(in)) over consecutive 5 ms windows (section 12).
struct GainTrace
{
    double windowSeconds = 0.005;
    std::vector<double> gainDb;     // one value per window
    std::vector<double> segmentEnd; // end time (s) of every segment

    // gain of the window that ends at time t (s)
    double at (double t) const
    {
        const auto idx = size_t (std::max (0.0, std::round (t / windowSeconds) - 1.0));
        return gainDb[std::min (idx, gainDb.size() - 1)];
    }
    // mean gain over [t0, t1)
    double mean (double t0, double t1) const
    {
        const auto i1 = std::min (gainDb.size(), size_t (std::round (t1 / windowSeconds)));
        const auto i0 = std::min (i1 - 1, size_t (std::round (t0 / windowSeconds)));
        double s = 0.0;
        for (size_t i = i0; i < i1; ++i)
            s += gainDb[i];
        return s / double (i1 - i0);
    }
};

inline GainTrace runSegments (RiderEngine& e, const std::vector<Segment>& segs, Stimulus stim = Stimulus::BandNoise,
                              double sineHz = 1000.0, unsigned seed = 7)
{
    const double fs = e.getSampleRate();
    BandNoise noise (fs, seed);
    GainTrace tr;
    const int win = int (std::lround (fs * tr.windowSeconds));
    tr.windowSeconds = double (win) / fs; // exact window length at this rate (220.5 -> 221 samples at 44.1 kHz)
    std::vector<double> l (static_cast<size_t> (win)), r (static_cast<size_t> (win)), in (static_cast<size_t> (win));
    double t = 0.0;
    long long n = 0;
    for (const auto& s : segs)
    {
        t += s.seconds;
        tr.segmentEnd.push_back (t);
    }
    const long long total = (long long) std::llround (t * fs);
    size_t seg = 0;
    double segEndSamples = tr.segmentEnd[0] * fs;
    while (n + win <= total)
    {
        for (int i = 0; i < win; ++i, ++n)
        {
            while (seg + 1 < segs.size() && double (n) >= segEndSamples)
                segEndSamples = tr.segmentEnd[++seg] * fs;
            const double a = segs[seg].levelDb <= -300.0 ? 0.0 : dbToGain (segs[seg].levelDb);
            const double v = stim == Stimulus::BandNoise ? a * noise.next()
                                                         : a * std::sqrt (2.0) * std::sin (2.0 * pi * sineHz * double (n) / fs);
            in[size_t (i)] = l[size_t (i)] = r[size_t (i)] = v;
        }
        double* ch[2] = { l.data(), r.data() };
        e.process (ch, 2, (const double* const*) nullptr, 0, win);
        double si = 0.0, so = 0.0;
        for (int i = 0; i < win; ++i)
        {
            si += in[size_t (i)] * in[size_t (i)];
            so += l[size_t (i)] * l[size_t (i)];
        }
        tr.gainDb.push_back (si > 0.0 ? 10.0 * std::log10 (so / si) : e.getAppliedGainDb());
    }
    return tr;
}

inline RiderEngine makeEngine (double fs, double minDb, double maxDb, bool fast = false, double targetDb = -21.0,
                               double idleDb = 0.0, double vocalSens = 0.0)
{
    RiderEngine e;
    Settings s;
    s.rangeMinDb = minDb;
    s.rangeMaxDb = maxDb;
    s.fast = fast;
    s.targetDb = targetDb;
    s.idleDb = idleDb;
    s.vocalSensDb = vocalSens;
    e.setSettings (s);
    e.prepare (fs);
    return e;
}

// Inverse of the static law (section 5) inside the range: detector level L (dB) from a steady gain g.
inline double levelFromGain (double gDb, double targetDb, double minDb, double maxDb)
{
    const double w = 0.5 * (maxDb - minDb), c = 0.5 * (maxDb + minDb);
    const double rest = std::pow (10.0, (gDb - c) / 20.0) - StaticLaw::a * std::pow (10.0, -w / 20.0);
    const double u = -20.0 * std::log10 (rest / StaticLaw::b) - c;
    return targetDb + u;
}

// ---- spec tables ------------------------------------------------------------------------------
inline const std::vector<double>& t2Levels()
{
    static const std::vector<double> v { -21, -9, -21, -15, -21, -27, -21, -33, -33, -27, -9, -15, -9, -3, -33, -39, -21, -21 };
    return v;
}

} // namespace craft::levelrider::test
