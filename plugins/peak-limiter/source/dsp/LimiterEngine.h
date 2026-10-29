// Craft Peak Limiter - complete look-ahead limiter (spec r1 sections 3-6, decisions D1/D2).
// Host-independent, header only: no JUCE dependency. Internal arithmetic is double precision.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Parameters.h"
#include "Primitives.h"
#include "ReleaseStage.h"

namespace craft::limiter
{

class LimiterEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double parameterSmoothingSeconds = 0.005; // one-pole tau; ~99 % settled after 23 ms (section 6)
    static constexpr double bypassFadeSeconds = 0.010;

    // Reported and actual latency for a sample rate (spec section 3).
    static int latencyForSampleRate (double fs) noexcept { return LookaheadConfig::forSampleRate (fs).latency; }

    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        channels = std::clamp (numChannels, 1, maxChannels);
        config = LookaheadConfig::forSampleRate (fs);

        hold.prepare (config.latency);
        boxA.prepare (config.boxA);
        boxB.prepare (config.boxB);
        for (auto& d : audioDelay)
            d.prepare (config.latency);
        requiredDelay.prepare (config.latency);
        thresholdDelay.prepare (config.latency);
        release.prepare (fs);

        thresholdLin.setTimeConstant (parameterSmoothingSeconds, fs);
        ceilingLin.setTimeConstant (parameterSmoothingSeconds, fs);
        bypassStep = 1.0 / std::max (1.0, bypassFadeSeconds * fs);
        applySettings();
        reset();
    }

    int getLatencySamples() const noexcept { return config.latency; }
    int getNumChannels() const noexcept { return channels; }
    const LookaheadConfig& getConfig() const noexcept { return config; }

    // D2: stream start with q = 1 and an all-pass history.
    void reset() noexcept
    {
        thresholdLin.snap();
        ceilingLin.snap();
        hold.reset (1.0);
        boxA.reset (1.0);
        boxB.reset (1.0);
        for (auto& d : audioDelay)
            d.reset (0.0);
        requiredDelay.reset (1.0);
        thresholdDelay.reset (thresholdLin.getCurrent());
        release.reset();
        wetMix = bypassed ? 0.0 : 1.0;
        resetMeters();
    }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        applySettings();
    }
    const Settings& getSettings() const noexcept { return settings; }

    void setBypassed (bool b) noexcept { bypassed = b; }

    // Process `numSamples` samples in place; `data[c]` for c < getNumChannels().
    template <typename Sample>
    void process (Sample* const* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double T = thresholdLin.next();
            const double C = ceilingLin.next();

            // 1-2: sample peak (linked) and required gain
            double in[maxChannels] = { 0.0, 0.0 };
            double p = 0.0;
            for (int c = 0; c < channels; ++c)
            {
                in[c] = static_cast<double> (data[c][i]);
                p = std::max (p, std::abs (in[c]));
            }
            const double r = p > T ? T / p : 1.0;

            // 3: hold over the following L samples (current aligned sample excluded)
            const double h = hold.push (r);

            // delayed (aligned) sample n = t - L
            const double rN = requiredDelay.push (r);
            const double tN = thresholdDelay.push (T);

            // 4-5: release, two cascaded boxcars
            const double q = release.process (h);
            const double s = boxB.push (boxA.push (q));

            // 6 (D1): clamp to the sample's own required gain, then makeup to the ceiling
            const double g = std::min (s, rN);
            minGain = std::min (minGain, g);
            const double makeup = C / tN;

            if (wetMix != (bypassed ? 0.0 : 1.0))
                wetMix = bypassed ? std::max (0.0, wetMix - bypassStep) : std::min (1.0, wetMix + bypassStep);

            for (int c = 0; c < channels; ++c)
            {
                const double x = audioDelay[static_cast<std::size_t> (c)].push (in[c]);
                double y = x * g * makeup;
                if (wetMix < 1.0)
                    y = wetMix <= 0.0 ? x : wetMix * y + (1.0 - wetMix) * x;
                data[c][i] = static_cast<Sample> (y);
                outPeakCh[static_cast<std::size_t> (c)] = std::max (outPeakCh[static_cast<std::size_t> (c)], std::abs (y));
                inPeakCh[static_cast<std::size_t> (c)] = std::max (inPeakCh[static_cast<std::size_t> (c)], std::abs (in[c]));
            }
        }
    }

    // Metering since the last resetMeters(): input/output sample peaks per channel and the smallest gain
    // (without makeup) that was applied.
    double getInputPeak (int c) const noexcept { return inPeakCh[static_cast<std::size_t> (std::clamp (c, 0, 1))]; }
    double getOutputPeak (int c) const noexcept { return outPeakCh[static_cast<std::size_t> (std::clamp (c, 0, 1))]; }
    double getMinGain() const noexcept { return minGain; }
    void resetMeters() noexcept
    {
        minGain = 1.0;
        inPeakCh = { 0.0, 0.0 };
        outPeakCh = { 0.0, 0.0 };
    }

private:
    void applySettings() noexcept
    {
        thresholdLin.setTarget (std::pow (10.0, settings.thresholdDb / 20.0));
        ceilingLin.setTarget (std::pow (10.0, settings.ceilingDb / 20.0));
        release.setReleaseMs (settings.releaseMs);
        release.setAutoRelease (settings.autoRelease);
    }

    double fs = 48000.0;
    int channels = 2;
    LookaheadConfig config;
    Settings settings;

    SlidingMin hold;
    Boxcar boxA, boxB;
    std::array<DelayLine<double>, maxChannels> audioDelay;
    DelayLine<double> requiredDelay, thresholdDelay;
    ReleaseStage release;
    Smoother thresholdLin, ceilingLin;

    bool bypassed = false;
    double wetMix = 1.0, bypassStep = 0.01;

    double minGain = 1.0;
    std::array<double, maxChannels> inPeakCh { 0.0, 0.0 }, outPeakCh { 0.0, 0.0 };
};

} // namespace craft::limiter
