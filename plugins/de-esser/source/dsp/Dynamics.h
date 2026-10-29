// Craft De-Esser - detector, gain computer and gain smoothing (spec r2 section 5).
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
// Time constants of spec r2 section 5 (all in seconds).
struct DynamicsTiming
{
    static constexpr double detectorAttack = 11.0e-6;  // s, peak envelope rise
    static constexpr double detectorRelease = 0.38e-3; // s, peak envelope fall
    static constexpr double gainAttack = 0.47e-3;      // s, tau_a: fast part, deficit increasing
    static constexpr double fastRelease = 11.6e-3;     // s, tau_f: fast part, deficit decreasing
    static constexpr double slowCharge = 7.2e-3;       // s, tau_c: slow part charging toward the fast part
    static constexpr double slowRelease = 32.4e-3;     // s, tau_s: slow part releasing toward the target
};

// Detector + gain computer + two-component release (spec r2 section 5) for one (linked) sidechain signal.
class GainReducer
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        aDet = onePoleCoeff (DynamicsTiming::detectorAttack, fs);
        rDet = std::exp (-1.0 / (DynamicsTiming::detectorRelease * fs));
        cAttack = onePoleCoeff (DynamicsTiming::gainAttack, fs);
        cFast = onePoleCoeff (DynamicsTiming::fastRelease, fs);
        cCharge = onePoleCoeff (DynamicsTiming::slowCharge, fs);
        cSlow = onePoleCoeff (DynamicsTiming::slowRelease, fs);
        reset();
    }

    void reset() noexcept
    {
        envelope = 0.0;
        fast = slow = 0.0;
        gain = 1.0;
        lastTargetDb = 0.0;
    }

    // Processes one sidechain sample; returns the applied linear gain (exactly 1 without reduction).
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

        // 2. static curve -> target deficit dt = 1 - 10^(-GR/20)
        const double levelDb = envelope > 0.0 ? 20.0 * std::log10 (envelope) : -400.0;
        const double grDb = GainComputer::reductionDb (levelDb, thresholdDb);
        lastTargetDb = grDb;
        const double target = grDb > 0.0 ? 1.0 - std::pow (10.0, -grDb / 20.0) : 0.0;

        // 3. fast part: attack when dt > df, release when dt < df
        if (target > fast)
            fast += (target - fast) * cAttack;
        else if (target < fast)
            fast += (target - fast) * cFast;
        if (std::abs (fast - target) <= snapEpsilon)
            fast = target;

        //    slow part: charges toward df while df > ds, otherwise releases toward dt while dt < ds
        if (fast > slow)
            slow += (fast - slow) * cCharge;
        else if (target < slow)
        {
            slow += (target - slow) * cSlow;
            if (slow - target <= snapEpsilon)
                slow = target;
        }

        gain = 1.0 - std::max (fast, slow);
        return gain;
    }

    double getGain() const noexcept { return gain; }
    double getEnvelope() const noexcept { return envelope; }
    double getTargetReductionDb() const noexcept { return lastTargetDb; }

private:
    static constexpr double tiny = 1.0e-30;
    static constexpr double snapEpsilon = 1.0e-10;

    double fs = 48000.0;
    double aDet = 0.0, rDet = 0.0, cAttack = 0.0, cFast = 0.0, cCharge = 0.0, cSlow = 0.0;
    double envelope = 0.0, fast = 0.0, slow = 0.0, gain = 1.0, lastTargetDb = 0.0;
};

} // namespace craft::deesser::dsp
