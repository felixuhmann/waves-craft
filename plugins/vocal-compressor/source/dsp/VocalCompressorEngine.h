// Craft Vocal Compressor - complete processor core (spec r1 sections 4-6).
// Host-independent, header only: no JUCE dependency. Internal arithmetic is double precision.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Gate.h"
#include "Parameters.h"
#include "Primitives.h"
#include "ReleaseStage.h"

namespace craft::vocal
{

class VocalCompressorEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double outputFactor = 0.99;              // section 4.5 / D2
    static constexpr double parameterSmoothingSeconds = 0.005; // one-pole tau in dB, ~99 % settled after 23 ms (D6)
    static constexpr double bypassFadeSeconds = 0.010;
    static constexpr double gateOffDb = -140.0; // smoothing target of a switched-off gate; at this value gg == 1

    static int latencyForSampleRate (double fs) noexcept { return LookaheadConfig::forSampleRate (fs).lookahead; }

    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        channels = std::clamp (numChannels, 1, maxChannels);
        config = LookaheadConfig::forSampleRate (fs);

        forwardMax.prepare (config.lookahead + 1); // A[n] = max(a[n .. n+H])
        boxA.prepare (config.boxA);
        boxB.prepare (config.boxB);
        for (auto& d : audioDelay)
            d.prepare (config.lookahead);
        gateDetector.prepare (fs);
        release.prepare (fs);

        compressionDb.setTimeConstant (parameterSmoothingSeconds, fs);
        gateDb.setTimeConstant (parameterSmoothingSeconds, fs);
        gainDb.setTimeConstant (parameterSmoothingSeconds, fs);
        bypassStep = 1.0 / std::max (1.0, bypassFadeSeconds * fs);
        applySettings();
        reset();
    }

    // Development / fitting aid: replaces the release constants; call before prepare().
    void setReleaseLaw (const ReleaseLaw& l) { release.setLaw (l); }

    int getLatencySamples() const noexcept { return config.lookahead; }
    int getNumChannels() const noexcept { return channels; }
    const LookaheadConfig& getConfig() const noexcept { return config; }

    // Stream start: silence history, all gains at rest.
    void reset() noexcept
    {
        compressionDb.snap();
        gateDb.snap();
        gainDb.snap();
        updateDerived (true);
        forwardMax.reset (0.0);
        boxA.reset (1.0);
        boxB.reset (1.0);
        for (auto& d : audioDelay)
            d.reset (0.0);
        gateDetector.reset();
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
            updateDerived (false);

            // linked detection level a = max |x_c|
            double in[maxChannels] = { 0.0, 0.0 };
            double a = 0.0;
            for (int c = 0; c < channels; ++c)
            {
                in[c] = static_cast<double> (data[c][i]);
                a = std::max (a, std::abs (in[c]));
                energySum += in[c] * in[c] / channels;
            }

            // the newest input is n + H; everything below refers to the aligned sample n
            const double A = forwardMax.push (a);

            // 4.2 gate (expander 1:2)
            const double d = gateDetector.process (A);
            const double gg = gateActive ? GateDetector::gain (d, gateThresholdDb) : 1.0;

            // 4.3 / 4.4 compressor target (from the ungated level, D4) and program-dependent release
            const double q = release.process (A, km1);

            // 4.5 combination and the two boxcars
            const double s = q * gg;
            const double s2 = boxB.push (boxA.push (s));
            minGain = std::min (minGain, s2);

            const double g = k * s2 * outputFactor * gainLin;

            if (wetMix != (bypassed ? 0.0 : 1.0))
                wetMix = bypassed ? std::max (0.0, wetMix - bypassStep) : std::min (1.0, wetMix + bypassStep);

            for (int c = 0; c < channels; ++c)
            {
                const auto ch = static_cast<std::size_t> (c);
                const double x = audioDelay[ch].push (in[c]);
                double y = x * g;
                if (wetMix < 1.0)
                    y = wetMix <= 0.0 ? x : wetMix * y + (1.0 - wetMix) * x;
                data[c][i] = static_cast<Sample> (y);
                outPeakCh[ch] = std::max (outPeakCh[ch], std::abs (y));
            }
        }
    }

    // Metering since the last resetMeters(): output sample peaks per channel, the smallest smoothed gain s2
    // (compressor x gate, without makeup) and the summed input energy (mean square over channels per sample).
    double getOutputPeak (int c) const noexcept { return outPeakCh[static_cast<std::size_t> (std::clamp (c, 0, 1))]; }
    double getMinGain() const noexcept { return minGain; }
    double getInputEnergySum() const noexcept { return energySum; }
    void resetMeters() noexcept
    {
        minGain = 1.0;
        energySum = 0.0;
        outPeakCh = { 0.0, 0.0 };
    }

    // Current makeup factor k and normalised compressor gain state (for tests).
    double getMakeup() const noexcept { return k; }
    const ReleaseStage& getReleaseStage() const noexcept { return release; }

private:
    void applySettings() noexcept
    {
        compressionDb.setTarget (std::min (0.0, settings.compressionDb));
        gateDb.setTarget (settings.gateOff() ? gateOffDb : settings.gateDb);
        gainDb.setTarget (settings.gainDb);
    }

    // Recomputes the linear quantities when a smoothed value moves (exact values once settled).
    void updateDerived (bool force) noexcept
    {
        if (force || ! compressionDb.isSettled())
        {
            k = std::pow (10.0, -compressionDb.next() / 20.0);
            km1 = k - 1.0;
        }
        if (force || ! gateDb.isSettled())
        {
            const double db = gateDb.next();
            gateActive = db > gateOffDb;
            gateThresholdDb = db;
        }
        if (force || ! gainDb.isSettled())
            gainLin = std::pow (10.0, gainDb.next() / 20.0);
    }

    double fs = 48000.0;
    int channels = 2;
    LookaheadConfig config;
    Settings settings;

    SlidingMax forwardMax;
    Boxcar boxA, boxB;
    std::array<DelayLine<double>, maxChannels> audioDelay;
    GateDetector gateDetector;
    ReleaseStage release;
    Smoother compressionDb, gateDb, gainDb;

    double k = 1.0, km1 = 0.0, gateThresholdDb = gateOffDb, gainLin = 1.0;
    bool gateActive = false;

    bool bypassed = false;
    double wetMix = 1.0, bypassStep = 0.01;

    double minGain = 1.0, energySum = 0.0;
    std::array<double, maxChannels> outPeakCh { 0.0, 0.0 };
};

} // namespace craft::vocal
