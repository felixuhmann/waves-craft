// Craft De-Esser - detector, gain computer and gain smoothing (spec r1 section 5).
// Host-independent: no JUCE dependency. All time constants are in seconds, so behaviour is
// independent of the sample rate.
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::deesser::dsp
{

// One-pole smoothing coefficient c = 1 - exp(-1 / (tau fs)) (spec section 5.3).
inline double onePoleCoeff (double tauSeconds, double fs) noexcept { return 1.0 - std::exp (-1.0 / (tauSeconds * fs)); }

// ---------------------------------------------------------------------------------------------
// Static curve (spec section 5.2): hard knee, 7:1, threshold offset -0.08 dB, at most 25.57 dB (1/19).
struct GainComputer
{
    static constexpr double ratio = 7.0;
    static constexpr double slope = 1.0 - 1.0 / ratio;  // 0.857 dB of reduction per dB above threshold
    static constexpr double thresholdOffsetDb = -0.08;
    static constexpr double minGain = 1.0 / 19.0;      // maximum attenuation, linear
    static double maxReductionDb() noexcept { return -20.0 * std::log10 (minGain); } // 25.575 dB

    // Gain reduction in dB (>= 0) for a detector level in dB.
    static double reductionDb (double levelDb, double thresholdDb) noexcept
    {
        const double gr = (levelDb - (thresholdDb + thresholdOffsetDb)) * slope;
        return std::clamp (gr, 0.0, maxReductionDb());
    }
};

// ---------------------------------------------------------------------------------------------
// Time constants. The detector and the attack follow spec section 5 directly. The release is a
// program-dependent two-stage structure (spec 5, "program-dependent release" and D1); its constants
// were fitted to the spec's envelope vectors 9.4 B, C' and D - see IMPLEMENTATION_NOTES.md.
struct DynamicsTiming
{
    static constexpr double detectorAttack = 11.0e-6;   // s, peak envelope rise
    static constexpr double detectorRelease = 0.38e-3;  // s, peak envelope fall
    static constexpr double gainAttack = 0.47e-3;       // s, gain decrease

    // Release stage 1 (linear gain, one-pole): slow after sustained reduction, faster after bursts.
    static constexpr double releaseSustained = 31.0e-3; // s
    static constexpr double releaseBurst = 13.0e-3;     // s
    // Release stage 2 (follows stage 1 upwards only; gives the delayed / S-shaped onset of the release).
    static constexpr double onsetSustained = 1.5e-3;    // s
    static constexpr double onsetBurst = 10.0e-3;       // s
    // Hold history: charges while reduction is requested (how long it has been held) and is kept,
    // slowly forgetting, while the gain recovers - so the whole release follows the held duration.
    static constexpr double historyCharge = 100.0e-3;   // s
    static constexpr double historyForget = 1.0;        // s
};

// Detector + gain computer + smoothing for one (linked) sidechain signal.
class GainReducer
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        aDet = onePoleCoeff (DynamicsTiming::detectorAttack, fs);
        rDet = std::exp (-1.0 / (DynamicsTiming::detectorRelease * fs));
        cAttack = onePoleCoeff (DynamicsTiming::gainAttack, fs);
        cRelSustained = onePoleCoeff (DynamicsTiming::releaseSustained, fs);
        cRelBurst = onePoleCoeff (DynamicsTiming::releaseBurst, fs);
        cOnsetSustained = onePoleCoeff (DynamicsTiming::onsetSustained, fs);
        cOnsetBurst = onePoleCoeff (DynamicsTiming::onsetBurst, fs);
        cHistoryCharge = onePoleCoeff (DynamicsTiming::historyCharge, fs);
        cHistoryForget = onePoleCoeff (DynamicsTiming::historyForget, fs);
        reset();
    }

    void reset() noexcept
    {
        envelope = 0.0;
        gain1 = gain = 1.0;
        held = 0.0;
        lastTargetDb = 0.0;
    }

    // Processes one sidechain sample; returns the smoothed linear gain (exactly 1 without reduction).
    double process (double d, double thresholdDb) noexcept
    {
        // 1. peak envelope
        const double a = std::abs (d);
        if (a > envelope)
            envelope += (a - envelope) * aDet;
        else
            envelope *= rDet;
        if (envelope < tiny)
            envelope = 0.0; // flush denormals / silence

        // 2. static curve
        const double levelDb = envelope > 0.0 ? 20.0 * std::log10 (envelope) : -400.0;
        const double grDb = GainComputer::reductionDb (levelDb, thresholdDb);
        lastTargetDb = grDb;
        const double target = grDb > 0.0 ? std::pow (10.0, -grDb / 20.0) : 1.0;

        // history of reduction (0 = idle, 1 = reducing for a long time)
        if (grDb > 0.0)
            held += (1.0 - held) * cHistoryCharge;
        else
            held -= held * cHistoryForget;

        // 3. smoothing, stage 1: attack / program-dependent release
        if (target < gain1)
            gain1 += (target - gain1) * cAttack;
        else if (gain1 != target)
        {
            gain1 += (target - gain1) * (cRelBurst + (cRelSustained - cRelBurst) * held);
            if (target - gain1 <= snapEpsilon)
                gain1 = target;
        }

        // stage 2: follows decreases at once, rises with the (program-dependent) onset smoothing
        if (gain1 <= gain)
            gain = gain1;
        else
        {
            gain += (gain1 - gain) * (cOnsetBurst + (cOnsetSustained - cOnsetBurst) * held);
            if (gain1 - gain <= snapEpsilon)
                gain = gain1;
        }
        return gain;
    }

    double getGain() const noexcept { return gain; }
    double getEnvelope() const noexcept { return envelope; }
    double getTargetReductionDb() const noexcept { return lastTargetDb; }

private:
    static constexpr double tiny = 1.0e-30;
    static constexpr double snapEpsilon = 1.0e-10;

    double fs = 48000.0;
    double aDet = 0.0, rDet = 0.0, cAttack = 0.0;
    double cRelSustained = 0.0, cRelBurst = 0.0, cOnsetSustained = 0.0, cOnsetBurst = 0.0;
    double cHistoryCharge = 0.0, cHistoryForget = 0.0;

    double envelope = 0.0, gain1 = 1.0, gain = 1.0, held = 0.0, lastTargetDb = 0.0;
};

} // namespace craft::deesser::dsp
