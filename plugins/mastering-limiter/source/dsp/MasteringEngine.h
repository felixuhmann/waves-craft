// Craft Mastering Limiter - complete host-independent signal path (spec r1 section 1):
// input -> look-ahead limiter gain x make-up -> requantiser -> output, delayed by the latency L.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "GainComputer.h"
#include "Parameters.h"
#include "Requantiser.h"

namespace craft::mastering_limiter::dsp
{

// Linear ramp in dB that lands exactly on its target (spec 8: click-free Threshold / Ceiling changes).
class DbRamp
{
public:
    void prepare (double sampleRate, double rampSeconds) { steps = std::max (1, int (std::lround (sampleRate * rampSeconds))); }
    void snapTo (double v) noexcept
    {
        cur = target = v;
        left = 0;
    }
    void setTarget (double v) noexcept
    {
        if (v == target)
            return;
        target = v;
        left = steps;
        inc = (target - cur) / double (steps);
    }
    bool isRamping() const noexcept { return left > 0; }
    double next() noexcept
    {
        if (left > 0)
            cur = --left == 0 ? target : cur + inc;
        return cur;
    }
    double current() const noexcept { return cur; }

private:
    double cur = 0.0, target = 0.0, inc = 0.0;
    int steps = 1, left = 0;
};

class MasteringEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double parameterRampSeconds = 0.020; // spec 8
    static constexpr double bypassFadeSeconds = 0.010;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        gain.prepare (fs);
        L = gain.getLatency();
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            xDelay[size_t (ch)].assign (size_t (L), 0.0);
            requant[size_t (ch)].prepare (fs, 0x5eed0000ull + 7919ull * std::uint64_t (ch));
        }
        makeupDelay.assign (size_t (L), 1.0);
        thresholdRamp.prepare (fs, parameterRampSeconds);
        ceilingRamp.prepare (fs, parameterRampSeconds);
        bypassFadeStep = 1.0 / std::max (1.0, fs * bypassFadeSeconds);
        applySettings (settings, true);
        reset();
    }

    void reset()
    {
        gain.reset();
        for (auto& d : xDelay)
            std::fill (d.begin(), d.end(), 0.0);
        std::fill (makeupDelay.begin(), makeupDelay.end(), 1.0);
        pos = 0;
        for (auto& r : requant)
            r.reset();
        thresholdRamp.snapTo (settings.thresholdDb);
        ceilingRamp.snapTo (settings.ceilingDb);
        updateStatic();
        bypassMix = bypassed ? 1.0 : 0.0;
        resetMeters();
    }

    // Threshold / Ceiling ramp, Release / Adaptive immediately, requantiser format at this block boundary.
    void setSettings (const Settings& s) { applySettings (s, false); }
    const Settings& getSettings() const noexcept { return settings; }

    void setBypassed (bool b) noexcept { bypassed = b; }

    int getLatencySamples() const noexcept { return L; }
    double getSampleRate() const noexcept { return fs; }

    // In-place processing of 1 or 2 channels (stereo: linked detection, one common gain).
    template <typename Sample>
    void process (Sample* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::clamp (numChannels, 1, maxChannels);
        for (int i = 0; i < numSamples; ++i)
        {
            if (thresholdRamp.isRamping() || ceilingRamp.isRamping())
            {
                thresholdRamp.next();
                ceilingRamp.next();
                updateStatic();
            }

            double x[maxChannels] = { 0.0, 0.0 };
            double peak = 0.0;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                x[ch] = double (channels[ch][i]);
                peak = std::max (peak, std::abs (x[ch]));
                inPeak[size_t (ch)] = std::max (inPeak[size_t (ch)], std::abs (x[ch]));
            }

            const auto gc = gain.process (peak, thresholdLin);

            const double mk = makeupDelay[size_t (pos)];
            makeupDelay[size_t (pos)] = makeupLin;
            minGain = std::min (minGain, gc.gain);

            const double fadeTarget = bypassed ? 1.0 : 0.0;
            if (bypassMix != fadeTarget)
                bypassMix = bypassMix < fadeTarget ? std::min (fadeTarget, bypassMix + bypassFadeStep)
                                                   : std::max (fadeTarget, bypassMix - bypassFadeStep);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& line = xDelay[size_t (ch)];
                const double xn = line[size_t (pos)];
                line[size_t (pos)] = x[ch];

                double out;
                if (bypassMix >= 1.0)
                    out = xn;
                else
                {
                    const double v = xn * gc.gain * mk; // step 7
                    const double y = requant[size_t (ch)].process (v);
                    out = bypassMix > 0.0 ? y + (xn - y) * bypassMix : y;
                }
                channels[ch][i] = Sample (out);
                outPeak[size_t (ch)] = std::max (outPeak[size_t (ch)], std::abs (out));
            }
            if (++pos == L)
                pos = 0;
        }
    }

    // Meter data accumulated since the last resetMeters() (audio thread).
    double getInputPeak (int ch) const noexcept { return inPeak[size_t (ch)]; }
    double getOutputPeak (int ch) const noexcept { return outPeak[size_t (ch)]; }
    double getMinGain() const noexcept { return minGain; }
    void resetMeters() noexcept
    {
        inPeak.fill (0.0);
        outPeak.fill (0.0);
        minGain = 1.0;
    }

    const Requantiser& getRequantiser (int ch) const noexcept { return requant[size_t (ch)]; }

private:
    void applySettings (const Settings& s, bool force)
    {
        const bool formatChanged = force || s.wordLength != settings.wordLength || s.dither != settings.dither
                                   || s.shaping != settings.shaping;
        settings = s;
        settings.thresholdDb = param::Level::snap (s.thresholdDb);
        settings.ceilingDb = param::Level::snap (s.ceilingDb);
        thresholdRamp.setTarget (settings.thresholdDb);
        ceilingRamp.setTarget (settings.ceilingDb);
        gain.setRelease (s.releaseMs, s.adaptiveRelease);
        if (formatChanged)
            for (auto& r : requant)
                r.setFormat (s.wordLength, s.dither, s.shaping);
    }

    void updateStatic() noexcept
    {
        const double t = thresholdRamp.current(), c = ceilingRamp.current();
        thresholdLin = std::pow (10.0, t / 20.0);
        makeupLin = std::pow (10.0, (c - t) / 20.0);
    }

    double fs = 48000.0;
    int L = 64;
    Settings settings;
    GainComputer gain;
    std::array<std::vector<double>, maxChannels> xDelay;
    std::vector<double> makeupDelay;
    int pos = 0;
    std::array<Requantiser, maxChannels> requant;
    DbRamp thresholdRamp, ceilingRamp;
    double thresholdLin = 1.0, makeupLin = 1.0;

    bool bypassed = false;
    double bypassMix = 0.0, bypassFadeStep = 0.01;

    std::array<double, maxChannels> inPeak {}, outPeak {};
    double minGain = 1.0;
};

} // namespace craft::mastering_limiter::dsp
