// Craft Console E - channel strip engine (spec r1 section 3): input trim, polarity, fixed saturation, filters,
// equaliser and dynamics in the selected order, output fader and the "analogue" noise floor. Zero latency.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "Dynamics.h"
#include "Equaliser.h"
#include "Parameters.h"
#include "Smoothing.h"

namespace craft::consolee::dsp
{

// Complete control state; continuous controls are host-normalised r in [0, 1] (spec section 2).
struct Settings
{
    double inputTrim = param::defaults::inputTrim;
    double outputFader = param::defaults::outputFader;
    bool polarity = false;
    bool analogue = false;

    EqSettings eq;
    bool split = false;          // filters before the dynamics
    bool eqBypass = false;
    bool eqToSidechain = false;

    double compThreshold = param::defaults::compThreshold;
    double compRatio = param::defaults::compRatio;
    double compRelease = param::defaults::compRelease;
    bool compFastAttack = false;
    double expThreshold = param::defaults::expThreshold;
    double expRange = param::defaults::expRange;
    double expRelease = param::defaults::expRelease;
    bool expFastAttack = false;
    bool gate = false;
    bool dynamicsBypass = false;
    bool channelOut = false;     // dynamics after the EQ
};

// Fixed saturation (section 9): y = x + 0.0032 x^3 (+0.021 dB and H3 -62 dBc at 0 dBFS). It is kept odd so that
// polarity inversion stays exact (section 3); the reference's H2 of about -94 dBc is not reproduced.
inline constexpr double saturationCubic = 0.0032;
inline double saturate (double x) noexcept { return x + saturationCubic * x * x * x; }

// Analogue switch (section 9): -110.7 dBFS rms noise per channel and -69 dB inter-channel leakage.
inline constexpr double analogueNoiseDbfs = -110.7;
inline constexpr double analogueLeakageDb = -69.0;

class ConsoleEngine
{
public:
    static constexpr int maxChannels = 2;

    explicit ConsoleEngine (int numChannels = 2) : channels (numChannels == 1 ? 1 : 2) {}

    int getNumChannels() const noexcept { return channels; }
    static constexpr int getLatencySamples() noexcept { return 0; }

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        active = pending = target;
        eq.prepare (fs, active.eq);
        dyn.setSettings (dynamicsSettings (active)); // before prepare: the gate/expander starts settled
        dyn.prepare (fs);
        for (auto* r : { &inGain, &outGain, &analogueMix })
            r->setRampTime (0.010, fs);
        inGain.snap (dbToGain (param::inputTrim (active.inputTrim)));
        outGain.snap (dbToGain (param::outputFader (active.outputFader)));
        analogueMix.snap (active.analogue ? 1.0 : 0.0);
        for (auto* r : { &rThr, &rRatio, &rRel, &rEThr, &rRange, &rERel })
            r->setRampTime (0.010, fs);
        snapDynamicsRamps (active);
        fadeLength = std::max (1, int (0.005 * fs));
        fadePos = 0;
        fadeState = FadeState::idle;
        noiseScale = std::pow (10.0, analogueNoiseDbfs / 20.0);
        leakage = std::pow (10.0, analogueLeakageDb / 20.0);
        dynCountdown = 0;
        resetMeters();
    }

    void reset() noexcept
    {
        eq.reset();
        dyn.reset();
    }

    // Called once per block (or whenever controls change); thread-safe hand-over is the caller's business.
    void setSettings (const Settings& s) noexcept
    {
        target = s;
        if (structuralDiffers (s, fadeState == FadeState::idle ? active : pending))
        {
            pending = s;
            if (fadeState == FadeState::idle || fadeState == FadeState::in)
            {
                // fade out from the current gain (fadePos / fadeLength), swap at silence, fade back in
                if (fadeState == FadeState::idle)
                    fadePos = fadeLength;
                fadeState = FadeState::out;
            }
        }
        else
        {
            pending = s;
            if (fadeState == FadeState::idle)
                applyContinuous (s);
        }
    }

    // In-place processing of numCh channel pointers (numCh == channels).
    template <typename Sample>
    void process (Sample* const* io, int numSamples) noexcept
    {
        for (int n = 0; n < numSamples; ++n)
        {
            advanceFade();
            eq.tick();
            tickDynamicsRamps();

            const double gIn = inGain.next(), gOut = outGain.next(), aMix = analogueMix.next();
            const double fade = currentFade();

            double x[maxChannels] {}, sc[maxChannels] {};
            for (int c = 0; c < channels; ++c)
            {
                double v = double (io[c][n]) * gIn;
                if (active.polarity)
                    v = -v;
                v = saturate (v);
                inPeak = std::max (inPeak, std::abs (v));
                if (active.split && ! active.eqBypass)
                    v = eq.processFilters (c, v);
                x[c] = v;
            }

            const bool eqOn = ! active.eqBypass;
            const bool eqInAudio = eqOn && ! active.eqToSidechain;
            const bool dynOn = ! active.dynamicsBypass;
            auto eqProcess = [this] (int c, double v) {
                if (! active.split)
                    v = eq.processFilters (c, v);
                return eq.processBands (c, v);
            };

            if (active.channelOut)
            {
                // EQ -> dynamics
                for (int c = 0; c < channels; ++c)
                {
                    if (eqOn)
                    {
                        const double e = eqProcess (c, x[c]);
                        if (eqInAudio) x[c] = e;
                        sc[c] = e;
                    }
                    else
                        sc[c] = x[c];
                }
                applyDynamics (x, sc, dynOn);
            }
            else
            {
                // dynamics -> EQ
                if (eqOn && active.eqToSidechain)
                    for (int c = 0; c < channels; ++c)
                        sc[c] = eqProcess (c, x[c]);
                else
                    for (int c = 0; c < channels; ++c)
                        sc[c] = x[c];
                applyDynamics (x, sc, dynOn);
                if (eqInAudio)
                    for (int c = 0; c < channels; ++c)
                        x[c] = eqProcess (c, x[c]);
            }

            double y[maxChannels] {};
            for (int c = 0; c < channels; ++c)
                y[c] = x[c] * gOut * fade;
            if (aMix > 0.0)
            {
                if (channels == 2)
                {
                    const double l = y[0], r = y[1];
                    y[0] += aMix * leakage * r;
                    y[1] += aMix * leakage * l;
                }
                for (int c = 0; c < channels; ++c)
                    y[c] += aMix * noiseScale * gaussian();
            }
            for (int c = 0; c < channels; ++c)
            {
                outPeak = std::max (outPeak, std::abs (y[c]));
                io[c][n] = Sample (y[c]);
            }
        }
    }

    // Meter values since the previous call (linear peaks; dB for the dynamics).
    struct Meters
    {
        double inputPeak = 0, outputPeak = 0;
        double compReductionDb = 0; // <= 0 (deepest)
        double expanderGainDb = 0;  // <= 0 (deepest)
    };
    Meters takeMeters() noexcept
    {
        Meters m { inPeak, outPeak, compMin, expMin };
        resetMeters();
        return m;
    }

    const Settings& getActiveSettings() const noexcept { return active; }
    const Dynamics& getDynamics() const noexcept { return dyn; }

    static double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }

    static DynamicsSettings dynamicsSettings (const Settings& s) noexcept
    {
        DynamicsSettings d;
        d.compThresholdDb = param::compThreshold (s.compThreshold);
        d.compInvRatio = param::ratioInverse (s.compRatio);
        d.compReleaseS = param::compRelease (s.compRelease);
        d.compFastAttack = s.compFastAttack;
        d.expThresholdDb = param::expThreshold (s.expThreshold);
        d.expRangeDb = param::expRange (s.expRange);
        d.expReleaseS = param::expRelease (s.expRelease);
        d.expFastAttack = s.expFastAttack;
        d.gate = s.gate;
        return d;
    }

private:
    enum class FadeState { idle, out, in };

    static bool structuralDiffers (const Settings& a, const Settings& b) noexcept
    {
        return a.polarity != b.polarity || a.split != b.split || a.eqBypass != b.eqBypass || a.eqToSidechain != b.eqToSidechain
            || a.dynamicsBypass != b.dynamicsBypass || a.channelOut != b.channelOut || a.eq.lfBell != b.eq.lfBell
            || a.eq.hfBell != b.eq.hfBell || (a.eq.hp > 0) != (b.eq.hp > 0) || (a.eq.lp > 0) != (b.eq.lp > 0)
            || a.gate != b.gate;
    }

    void applyContinuous (const Settings& s) noexcept
    {
        const bool eqStructural = (s.eq.hp > 0) != (active.eq.hp > 0) || (s.eq.lp > 0) != (active.eq.lp > 0)
                               || s.eq.lfBell != active.eq.lfBell || s.eq.hfBell != active.eq.hfBell;
        active = s;
        if (eqStructural)
            eq.snapTo (s.eq);
        else
            eq.setTargets (s.eq);
        inGain.setTarget (dbToGain (param::inputTrim (s.inputTrim)));
        outGain.setTarget (dbToGain (param::outputFader (s.outputFader)));
        analogueMix.setTarget (s.analogue ? 1.0 : 0.0);
        rThr.setTarget (s.compThreshold);
        rRatio.setTarget (s.compRatio);
        rRel.setTarget (s.compRelease);
        rEThr.setTarget (s.expThreshold);
        rRange.setTarget (s.expRange);
        rERel.setTarget (s.expRelease);
        dynCountdown = 0;
    }

    void snapDynamicsRamps (const Settings& s) noexcept
    {
        rThr.snap (s.compThreshold);
        rRatio.snap (s.compRatio);
        rRel.snap (s.compRelease);
        rEThr.snap (s.expThreshold);
        rRange.snap (s.expRange);
        rERel.snap (s.expRelease);
    }

    void tickDynamicsRamps() noexcept
    {
        if (dynCountdown > 0)
        {
            --dynCountdown;
            return;
        }
        constexpr int interval = 16;
        bool moving = false;
        for (auto* r : { &rThr, &rRatio, &rRel, &rEThr, &rRange, &rERel })
            moving |= ! r->isSettled();
        Settings s = active;
        s.compThreshold = rThr.skip (interval);
        s.compRatio = rRatio.skip (interval);
        s.compRelease = rRel.skip (interval);
        s.expThreshold = rEThr.skip (interval);
        s.expRange = rRange.skip (interval);
        s.expRelease = rERel.skip (interval);
        if (moving || ! dynInitialised)
        {
            dyn.setSettings (dynamicsSettings (s));
            dynInitialised = true;
        }
        dynCountdown = interval - 1;
    }

    void advanceFade() noexcept
    {
        if (fadeState == FadeState::out)
        {
            if (--fadePos <= 0)
            {
                // at silence: switch the structure; clear the filter/detector states the new structure inherits
                const bool routingChanged = pending.channelOut != active.channelOut || pending.eqToSidechain != active.eqToSidechain
                                         || pending.split != active.split || pending.eqBypass != active.eqBypass;
                applyContinuous (pending);
                if (routingChanged)
                    eq.reset();
                fadeState = FadeState::in;
                fadePos = 0;
            }
        }
        else if (fadeState == FadeState::in)
        {
            if (++fadePos >= fadeLength)
            {
                fadeState = FadeState::idle;
                if (structuralDiffers (pending, active))
                    setSettings (pending);
                else
                    applyContinuous (pending);
            }
        }
    }

    double currentFade() const noexcept
    {
        switch (fadeState)
        {
            case FadeState::idle: return 1.0;
            case FadeState::out:
            case FadeState::in: return double (fadePos) / double (fadeLength);
        }
        return 1.0;
    }

    template <size_t N>
    void applyDynamics (double (&x)[N], const double (&sc)[N], bool on) noexcept
    {
        const double gDb = dyn.process (sc, channels);
        compMin = std::min (compMin, dyn.getCompressorReductionDb());
        expMin = std::min (expMin, dyn.getExpanderGainDb());
        if (! on)
            return;
        const double g = gDb == 0.0 ? 1.0 : std::pow (10.0, gDb / 20.0);
        for (int c = 0; c < channels; ++c)
            x[c] *= g;
    }

    double gaussian() noexcept
    {
        if (haveSpare)
        {
            haveSpare = false;
            return spare;
        }
        double u, v, s;
        do
        {
            u = uniform() * 2.0 - 1.0;
            v = uniform() * 2.0 - 1.0;
            s = u * u + v * v;
        } while (s >= 1.0 || s == 0.0);
        const double m = std::sqrt (-2.0 * std::log (s) / s);
        spare = v * m;
        haveSpare = true;
        return u * m;
    }

    double uniform() noexcept
    {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        return double (rng >> 11) * (1.0 / 9007199254740992.0);
    }

    void resetMeters() noexcept
    {
        inPeak = outPeak = 0.0;
        compMin = expMin = 0.0;
    }

    const int channels;
    double fs = 48000.0;
    Settings target, active, pending;
    Equaliser eq;
    Dynamics dyn;
    LinearRamp inGain, outGain, analogueMix;
    LinearRamp rThr, rRatio, rRel, rEThr, rRange, rERel;
    int dynCountdown = 0;
    bool dynInitialised = false;

    FadeState fadeState = FadeState::idle;
    int fadeLength = 240, fadePos = 0;

    double noiseScale = 0, leakage = 0;
    std::uint64_t rng = 0x9E3779B97F4A7C15ull;
    double spare = 0;
    bool haveSpare = false;

    double inPeak = 0, outPeak = 0, compMin = 0, expMin = 0;
};

} // namespace craft::consolee::dsp
