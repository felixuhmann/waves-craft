// Craft Punch Compressor - complete engine (spec r1 sections 3-12). Host-independent, header only (no JUCE).
// Internal arithmetic is double precision. Signal flow per sample (section 3), stereo-linked:
//
//   detector p = max |x| over the newest H+1 samples (forward peak hold for the H-delayed audio)
//   -> D = 20 log10 p -> punch delay P ms -> static curve GR(D) -> gain smoother (attack / release)
//   -> makeup -> mix with the H-delayed dry signal -> Output gain -> analog mode -> output peak stage.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "Analog.h"
#include "GainSmoother.h"
#include "OutputStage.h"
#include "Parameters.h"
#include "Primitives.h"
#include "StaticCurve.h"

namespace craft::punch
{

class PunchEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double parameterSmoothingSeconds = 0.004; // one-pole tau: 99.3 % after 20 ms (decision D5)
    static constexpr double fadeSeconds = 0.010;               // analog mode change / bypass crossfade
    static constexpr double maxPunchMs = 30.0;

    static int latencyForSampleRate (double fs) noexcept { return lookaheadForSampleRate (fs); }

    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate > 0.0 ? sampleRate : 48000.0;
        channels = std::clamp (numChannels, 1, maxChannels);
        H = lookaheadForSampleRate (fs);

        detector.prepare (H + 1);
        punchDelay.prepare (int (std::ceil (maxPunchMs * 0.001 * fs)) + 1);
        for (auto& d : audioDelay)
            d.prepare (H);
        smoother.prepare (fs);
        outputStage.prepare (fs, H);

        for (auto* s : { &thresholdSm, &ratioUSm, &makeupSm, &mixSm, &outputSm })
            s->setTimeConstant (parameterSmoothingSeconds, fs);
        fadeStep = 1.0 / std::max (1.0, fadeSeconds * fs);
        for (int c = 0; c < maxChannels; ++c)
            noise[size_t (c)].seed (0x2545f491u + 0x9e3779b9u * std::uint32_t (c + 1));

        applySettings (true);
        reset();
    }

    int getLatencySamples() const noexcept { return H; }
    int getNumChannels() const noexcept { return channels; }
    double getSampleRate() const noexcept { return fs; }

    void reset() noexcept
    {
        for (auto* s : { &thresholdSm, &ratioUSm, &makeupSm, &mixSm, &outputSm })
            s->snap();
        detector.reset (0.0);
        punchDelay.reset (-400.0);
        for (auto& d : audioDelay)
            d.reset (0.0);
        smoother.reset();
        outputStage.reset();
        for (int c = 0; c < maxChannels; ++c)
        {
            analog[size_t (c)].prepare (activeMode, fs);
            analogOld[size_t (c)].prepare (activeMode, fs);
            analogPred[size_t (c)].prepare (activeMode, fs);
        }
        modeFade = 1.0;
        wet = bypassed ? 0.0 : 1.0;
        resetMeters();
    }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        applySettings (false);
    }
    const Settings& getSettings() const noexcept { return settings; }

    void setBypassed (bool b) noexcept { bypassed = b; }

    template <typename Sample>
    void process (Sample* const* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double T = thresholdSm.next();
            const double u = ratioUSm.next();
            const double mkLin = makeupSm.next (1.0e-12);
            const double mix = mixSm.next();
            const double outLin = outputSm.next (1.0e-12);

            // 1. detector: forward sample-peak hold over H+1 samples (linked)
            double in[maxChannels] = { 0.0, 0.0 };
            double a = 0.0;
            for (int c = 0; c < channels; ++c)
            {
                in[c] = static_cast<double> (data[c][i]);
                a = std::max (a, std::abs (in[c]));
            }
            if (! (a <= 1.0e6)) // NaN / Inf / absurd levels
                a = 1.0e6;
            inPeak = std::max (inPeak, a);
            const double p = detector.push (a);
            const double D = p > 1.0e-20 ? 20.0 * std::log10 (p) : -400.0;

            // 2. punch: side-chain delayed by P ms
            const double Dp = punchDelay.push (D, punchSamples);

            // 3.-5. static curve, smoothing, makeup
            const double gr = curve::gainReduction (Dp, T, u);
            const double target = gr > 0.0 ? std::pow (10.0, -gr / 20.0) : 1.0;
            const double g = smoother.process (target);
            minGain = std::min (minGain, g);
            const double gc = g * mkLin;

            // composite gain of the newest sample, for the output-stage prediction
            const double gPred = (1.0 + mix * (gc - 1.0)) * outLin;

            if (modeFade < 1.0)
                modeFade = std::min (1.0, modeFade + fadeStep);
            const double bypassTarget = bypassed ? 0.0 : 1.0;
            if (wet != bypassTarget)
                wet = bypassed ? std::max (0.0, wet - fadeStep) : std::min (1.0, wet + fadeStep);

            double y[maxChannels] = { 0.0, 0.0 }, dry[maxChannels] = { 0.0, 0.0 };
            double predPeak = 0.0;
            for (int c = 0; c < channels; ++c)
            {
                const auto ch = static_cast<size_t> (c);
                const double xd = audioDelay[ch].push (in[c]);
                dry[c] = xd;
                // 6. mix (exact for gc == 1), 7. output gain
                const double y1 = xd + mix * (xd * gc - xd);
                const double y2 = y1 * outLin;
                // 8. analog mode (+ noise)
                double y3 = analog[ch].process (y2);
                if (modeFade < 1.0)
                    y3 = modeFade * y3 + (1.0 - modeFade) * analogOld[ch].process (y2);
                if (noiseScale > 0.0)
                    y3 += noiseScale * noise[ch].next();
                y[c] = y3;
                // prediction of the chain output for the newest input sample
                predPeak = std::max (predPeak, std::abs (analogPred[ch].process (in[c] * gPred)));
            }

            // 9. output peak stage
            const double s = outputStage.process (OutputStage::requiredGain (predPeak));
            for (int c = 0; c < channels; ++c)
            {
                double out = OutputStage::clampToCeiling (y[c] * s);
                if (wet < 1.0)
                    out = wet <= 0.0 ? dry[c] : wet * out + (1.0 - wet) * dry[c];
                data[c][i] = static_cast<Sample> (out);
                outPeak = std::max (outPeak, std::abs (out));
            }
        }
    }

    // Metering since the last resetMeters(): input / output sample peak (linked) and the smallest
    // gain-reduction gain (without makeup).
    double getInputPeak() const noexcept { return inPeak; }
    double getOutputPeak() const noexcept { return outPeak; }
    double getMinGain() const noexcept { return minGain; }
    void resetMeters() noexcept
    {
        inPeak = outPeak = 0.0;
        minGain = 1.0;
    }

    // Values derived from the settings (for tests and the editor).
    double getEffectiveReleaseMs() const noexcept { return effectiveReleaseMs; }
    double getMakeupDb() const noexcept { return makeupDb; }
    int getPunchSamples() const noexcept { return punchSamples; }

private:
    void applySettings (bool snapAll) noexcept
    {
        effectiveReleaseMs = settings.effectiveReleaseMs();
        const double u = curve::ratioToU (settings.ratio);
        makeupDb = curve::makeupDb (settings.thresholdDb, u, effectiveReleaseMs);
        thresholdSm.setTarget (settings.thresholdDb);
        ratioUSm.setTarget (u);
        makeupSm.setTarget (makeupDb == 0.0 ? 1.0 : std::pow (10.0, makeupDb / 20.0));
        mixSm.setTarget (std::clamp (settings.mixPct, 0.0, 100.0) / 100.0);
        outputSm.setTarget (settings.outputDb == 0.0 ? 1.0 : std::pow (10.0, settings.outputDb / 20.0));
        if (settings.attackMs != lastAttack || effectiveReleaseMs != lastRelease || snapAll)
        {
            smoother.setTimes (settings.attackMs, effectiveReleaseMs);
            lastAttack = settings.attackMs;
            lastRelease = effectiveReleaseMs;
        }
        punchSamples = int (std::lround (std::clamp (settings.punch, 0.0, maxPunchMs) * 0.001 * fs));

        const int mode = std::clamp (settings.analog, 0, 4);
        if (mode != activeMode)
        {
            for (int c = 0; c < maxChannels; ++c)
            {
                const auto ch = static_cast<size_t> (c);
                analogOld[ch] = analog[ch];
                analog[ch].prepare (mode, fs);
                analogPred[ch].prepare (mode, fs);
            }
            activeMode = mode;
            modeFade = snapAll ? 1.0 : 0.0;
        }
        noiseScale = analog[0].noiseRms() * std::sqrt (3.0); // uniform [-1, 1) has RMS 1/sqrt(3)
        if (snapAll)
            for (auto* s : { &thresholdSm, &ratioUSm, &makeupSm, &mixSm, &outputSm })
                s->snap();
    }

    double fs = 48000.0;
    int channels = 2, H = 64;
    Settings settings;

    SlidingMax detector;
    VariableDelay punchDelay;
    std::array<DelayLine<double>, maxChannels> audioDelay;
    GainSmoother smoother;
    OutputStage outputStage;
    std::array<AnalogStage, maxChannels> analog, analogOld, analogPred;
    std::array<WhiteNoise, maxChannels> noise;
    Smoother thresholdSm, ratioUSm, makeupSm, mixSm, outputSm;

    int activeMode = 0, punchSamples = 0;
    double modeFade = 1.0, fadeStep = 0.01, noiseScale = 0.0;
    double effectiveReleaseMs = 100.0, makeupDb = 0.0, lastAttack = -1.0, lastRelease = -1.0;
    bool bypassed = false;
    double wet = 1.0;

    double inPeak = 0.0, outPeak = 0.0, minGain = 1.0;
};

} // namespace craft::punch
