// Craft Multiband - complete processor core: 4-band allpass-complementary crossover, linked per-band dynamics,
// solo / band bypass, output gain, parameter smoothing and host bypass (spec r2). Host-independent, no JUCE.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "BandDynamics.h"
#include "Crossover.h"
#include "Envelope.h"
#include "GainCurve.h"
#include "Parameters.h"

namespace craft::multiband
{

// One-pole parameter smoother that lands exactly on its target.
class Smoother
{
public:
    void prepare (double fs, double tauMs) noexcept { c = -std::expm1 (-1000.0 / (fs * tauMs)); }
    void reset (double v) noexcept { value = target = v; }
    void setTarget (double v) noexcept { target = v; }
    bool isMoving() const noexcept { return value != target; }
    double next() noexcept
    {
        if (value != target)
        {
            value += (target - value) * c;
            if (std::abs (target - value) < 1.0e-7)
                value = target;
        }
        return value;
    }
    double get() const noexcept { return value; }
    double getTarget() const noexcept { return target; }

private:
    double c = 1.0, value = 0.0, target = 0.0;
};

// Linear ramp 0 <-> 1 (crossfades of solo, band bypass and host bypass).
class Ramp
{
public:
    void prepare (double fs, double ms) noexcept { step = 1.0 / std::max (1.0, fs * ms / 1000.0); }
    void reset (double v) noexcept { value = target = v; }
    void setTarget (double v) noexcept { target = v; }
    double next() noexcept
    {
        if (value < target) value = std::min (target, value + step);
        else if (value > target) value = std::max (target, value - step);
        return value;
    }
    double get() const noexcept { return value; }
    bool isSettled() const noexcept { return value == target; }

private:
    double step = 0.01, value = 0.0, target = 0.0;
};

class MultibandEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double smoothingMs = 4.0;   // one-pole: 99 % after 18.4 ms (decision D2: <= 20 ms)
    static constexpr double switchFadeMs = 10.0; // solo / band bypass / host bypass crossfades
    static constexpr int coeffInterval = 4;      // crossover coefficient update interval while gliding

    void prepare (double sampleRate, int numChannels) noexcept
    {
        fs = sampleRate;
        channels = std::clamp (numChannels, 1, maxChannels);
        outGain.prepare (fs, smoothingMs);
        knee.prepare (fs, smoothingMs);
        q.prepare (fs, smoothingMs);
        for (auto& x : xoverLog2) x.prepare (fs, smoothingMs);
        for (int b = 0; b < numBands; ++b)
        {
            auto& st = band[size_t (b)];
            st.gain.prepare (fs, smoothingMs);
            st.range.prepare (fs, smoothingMs);
            st.threshold.prepare (fs, smoothingMs);
            st.weight.prepare (fs, switchFadeMs);
            st.bypassMix.prepare (fs, switchFadeMs);
            st.dyn.prepare (fs);
        }
        bypassMix.prepare (fs, switchFadeMs);
        snapToSettings();
        reset();
    }

    void setDynamicsLaw (const DynamicsLaw& law) noexcept
    {
        for (auto& st : band)
            st.dyn.setLaw (law);
    }

    void setSettings (const Settings& in) noexcept
    {
        const Settings s = sanitise (in);
        settings = s;
        outGain.setTarget (s.outputGainDb);
        knee.setTarget (std::clamp (s.knee, 0.0, 0.75));
        q.setTarget (std::clamp (s.q, 0.1, 0.75));
        for (int k = 0; k < numCrossovers; ++k)
            xoverLog2[size_t (k)].setTarget (std::log2 (std::max (1.0, s.crossoverHz[size_t (k)])));
        bool anySolo = false;
        for (const auto& b : s.bands)
            anySolo = anySolo || b.solo;
        for (int b = 0; b < numBands; ++b)
        {
            const auto& bs = s.bands[size_t (b)];
            auto& st = band[size_t (b)];
            st.gain.setTarget (bs.gainDb);
            st.range.setTarget (bs.rangeDb);
            st.threshold.setTarget (bs.thresholdDb);
            st.weight.setTarget (! anySolo || bs.solo ? 1.0 : 0.0);
            st.bypassMix.setTarget (bs.bypass ? 1.0 : 0.0);
        }
    }

    // Host bypass (bit-exact dry once the 10 ms crossfade has finished).
    void setBypassed (bool b) noexcept { bypassMix.setTarget (b ? 1.0 : 0.0); }

    // Jump all smoothers to their targets (start of playback).
    void snapToSettings() noexcept
    {
        setSettings (settings);
        outGain.reset (outGain.getTarget());
        knee.reset (knee.getTarget());
        q.reset (q.getTarget());
        for (auto& x : xoverLog2) x.reset (x.getTarget());
        bool anySolo = false;
        for (const auto& bs : settings.bands)
            anySolo = anySolo || bs.solo;
        for (int b = 0; b < numBands; ++b)
        {
            auto& st = band[size_t (b)];
            st.gain.reset (st.gain.getTarget());
            st.range.reset (st.range.getTarget());
            st.threshold.reset (st.threshold.getTarget());
            st.weight.reset (! anySolo || settings.bands[size_t (b)].solo ? 1.0 : 0.0);
            st.bypassMix.reset (settings.bands[size_t (b)].bypass ? 1.0 : 0.0);
        }
        updateCrossover();
    }

    void reset() noexcept
    {
        for (auto& sp : splitter) sp.reset();
        for (auto& st : band)
        {
            for (auto& e : st.env) e.reset();
            st.dyn.reset();
            st.gainDb = st.gain.get();
        }
        coeffCountdown = 0;
        resetMeters();
    }

    void resetMeters() noexcept
    {
        inPeak = { 0.0, 0.0 };
        outPeak = { 0.0, 0.0 };
        for (auto& st : band) st.envPeak = 0.0;
    }

    template <typename Sample>
    void process (Sample* const* io, int numSamples) noexcept
    {
        if (bypassMix.isSettled() && bypassMix.get() >= 1.0)
        {
            // Fully bypassed: the buffer is left untouched (bit-exact). Keep the detectors quiet and the smoothers
            // at their targets so that un-bypassing starts cleanly.
            if (! idle)
            {
                reset();
                idle = true;
            }
            snapToSettings();
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < numSamples; ++i)
                {
                    const double v = std::abs (double (io[c][i]));
                    inPeak[size_t (c)] = std::max (inPeak[size_t (c)], v);
                    outPeak[size_t (c)] = std::max (outPeak[size_t (c)], v);
                }
            for (auto& st : band) st.gainDb = 0.0;
            return;
        }

        idle = false;
        const bool opto = settings.opto, autoRel = settings.autoRelease;
        std::array<std::array<double, numBands>, maxChannels> bands {};
        for (int i = 0; i < numSamples; ++i)
        {
            // crossover glide
            bool moving = q.isMoving();
            for (auto& x : xoverLog2) moving = moving || x.isMoving();
            q.next();
            for (auto& x : xoverLog2) x.next();
            if (moving && --coeffCountdown <= 0)
            {
                updateCrossover();
                coeffCountdown = coeffInterval;
            }

            const double kneeNow = knee.next();
            const double outLin = std::pow (10.0, outGain.next() / 20.0);

            for (int c = 0; c < channels; ++c)
            {
                const double x = double (io[c][i]);
                inPeak[size_t (c)] = std::max (inPeak[size_t (c)], std::abs (x));
                splitter[size_t (c)].process (x, bands[size_t (c)]);
            }

            std::array<double, numBands> g {};
            for (int b = 0; b < numBands; ++b)
            {
                auto& st = band[size_t (b)];
                const double rangeDb = st.range.next();
                const double thrDb = st.threshold.next();
                const double gainDb = st.gain.next();
                double env2 = 0.0;
                for (int c = 0; c < channels; ++c)
                    env2 = std::max (env2, st.env[size_t (c)].processSquared (bands[size_t (c)][size_t (b)]));
                st.envPeak = std::max (st.envPeak, env2);
                st.curve.set (kneeNow, rangeDb);
                const auto& bs = settings.bands[size_t (b)];
                const double nu = st.dyn.process (env2, thrDb, rangeDb, bs.attackMs, bs.releaseMs, opto, autoRel, st.curve);
                const double dyn = gainFromNuFast (st, nu, rangeDb);
                const double active = std::pow (10.0, gainDb / 20.0) * dyn;
                const double by = st.bypassMix.next();
                const double total = (1.0 - by) * active + by;
                st.gainDb = 20.0 * std::log10 (std::max (total, 1.0e-12));
                g[size_t (b)] = total * st.weight.next() * outLin;
            }

            const double wet = 1.0 - bypassMix.next();
            for (int c = 0; c < channels; ++c)
            {
                const auto& bb = bands[size_t (c)];
                double y = g[0] * bb[0] + g[1] * bb[1] + g[2] * bb[2] + g[3] * bb[3];
                const double x = double (io[c][i]);
                if (wet < 1.0)
                    y = wet * y + (1.0 - wet) * x;
                if (! std::isfinite (y))
                {
                    y = 0.0;
                    corrupted = true;
                }
                outPeak[size_t (c)] = std::max (outPeak[size_t (c)], std::abs (y));
                io[c][i] = Sample (y);
            }
        }

        // Robustness (spec r2 section 6): a non-finite value (e.g. a non-finite input sample) must not poison the
        // filter or detector states - start them afresh so the processor stays responsive.
        if (corrupted)
        {
            corrupted = false;
            reset();
            snapToSettings();
        }
    }

    // Meters / display (read after process()).
    int getNumChannels() const noexcept { return channels; }
    double getInputPeak (int c) const noexcept { return inPeak[size_t (c)]; }
    double getOutputPeak (int c) const noexcept { return outPeak[size_t (c)]; }
    // Peak band level (sine-peak amplitude) since resetMeters().
    double getBandLevel (int b) const noexcept { return std::sqrt (band[size_t (b)].envPeak); }
    // Current total band gain in dB (Gain + dynamics, band bypass blended; without solo and output gain).
    double getBandGainDb (int b) const noexcept { return band[size_t (b)].gainDb; }
    double getSampleRate() const noexcept { return fs; }

private:
    struct BandState
    {
        Smoother gain, range, threshold;
        Ramp weight, bypassMix;
        std::array<HilbertEnvelope, maxChannels> env;
        GainCurve curve;
        BandDynamics dyn;
        double envPeak = 0.0, gainDb = 0.0;
        double lastRange = 1.0e9, grMinus1 = 0.0;
    };

    // Replace non-finite or out-of-range settings by safe values.
    static Settings sanitise (Settings s) noexcept
    {
        auto fix = [] (double v, double lo, double hi, double def) { return std::isfinite (v) ? std::clamp (v, lo, hi) : def; };
        s.outputGainDb = fix (s.outputGainDb, -18.0, 18.0, 0.0);
        s.knee = fix (s.knee, 0.0, 0.75, 0.5);
        s.q = fix (s.q, 0.1, 0.75, 0.6);
        for (int k = 0; k < numCrossovers; ++k)
            s.crossoverHz[size_t (k)] = fix (s.crossoverHz[size_t (k)], 16.0, 21357.0, param::defaultCrossover[size_t (k)]);
        for (auto& b : s.bands)
        {
            b.gainDb = fix (b.gainDb, -18.0, 18.0, 0.0);
            b.rangeDb = fix (b.rangeDb, -24.0, 18.0, 0.0);
            b.thresholdDb = fix (b.thresholdDb, -80.0, 0.0, -30.0);
            b.attackMs = fix (b.attackMs, 0.5, 500.0, 10.0);
            b.releaseMs = fix (b.releaseMs, 5.0, 5000.0, 50.0);
        }
        return s;
    }

    static double gainFromNuFast (BandState& st, double nu, double rangeDb) noexcept
    {
        if (rangeDb != st.lastRange)
        {
            st.lastRange = rangeDb;
            st.grMinus1 = std::pow (10.0, rangeDb / 20.0) - 1.0;
        }
        return 1.0 + nu * st.grMinus1;
    }

    void updateCrossover() noexcept
    {
        std::array<CrossoverSection, numCrossovers> secs;
        for (int k = 0; k < numCrossovers; ++k)
            secs[size_t (k)] = CrossoverSection::design (std::exp2 (xoverLog2[size_t (k)].get()), q.get(), fs);
        for (auto& sp : splitter)
            sp.setSections (secs);
    }

    double fs = 48000.0;
    int channels = 2;
    Settings settings = Settings::defaults();
    Smoother outGain, knee, q;
    std::array<Smoother, numCrossovers> xoverLog2;
    std::array<FourBandSplitter, maxChannels> splitter;
    std::array<BandState, numBands> band;
    Ramp bypassMix;
    int coeffCountdown = 0;
    bool idle = false, corrupted = false;
    std::array<double, maxChannels> inPeak {}, outPeak {};
};

} // namespace craft::multiband
