// Craft De-Esser - detector, gain computer and gain smoothing (spec r3 section 5, normative pseudocode).
// Host-independent: no JUCE dependency. All time constants are in seconds, so behaviour is
// independent of the sample rate.
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::deesser::dsp
{

// One-pole coefficient c = 1 - exp(-1 / (tau fs)).
inline double onePoleCoeff (double tauSeconds, double fs) noexcept { return 1.0 - std::exp (-1.0 / (tauSeconds * fs)); }

// ---------------------------------------------------------------------------------------------
// Static curve (r3 section 5 step 2): hard knee, slope 0.85648 dB/dB (~7:1), threshold offset 0.0793 dB,
// maximum reduction 25.570 dB (linear ~1/19).
struct GainComputer
{
    static constexpr double slope = 0.85648;
    static constexpr double thresholdOffsetDb = 0.0793; // GR starts at Threshold - 0.0793 dB
    static constexpr double maxReductionDb = 25.570;

    // Gain reduction in dB (>= 0) for a detector level in dB.
    static double reductionDb (double levelDb, double thresholdDb) noexcept
    {
        const double gr = (levelDb - (thresholdDb - thresholdOffsetDb)) * slope;
        return std::min (std::max (gr, 0.0), maxReductionDb);
    }
};

// ---------------------------------------------------------------------------------------------
// Time constants of r3 section 5 (seconds).
struct DynamicsTiming
{
    static constexpr double detectorAttack = 4.594e-6;  // a_det
    static constexpr double detectorRelease = 0.4942e-3; // r_det (multiplicative decay)
    static constexpr double gainAttack = 0.4934e-3;     // c_a
    static constexpr double fastRelease = 9.913e-3;     // c_f
    static constexpr double slowCharge = 3.789e-3;      // c_c
    static constexpr double slowRelease = 31.97e-3;     // c_s
};

// Detector + gain computer + two-part release for one (linked) sidechain signal, implemented literally from the
// r3 pseudocode: 1 detector -> 2 static curve -> 3a fast part f -> 3b slow part s -> 4 g = 1 - max(f, s).
class GainReducer
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        aDet = onePoleCoeff (DynamicsTiming::detectorAttack, fs);
        rDet = std::exp (-1.0 / (DynamicsTiming::detectorRelease * fs));
        cA = onePoleCoeff (DynamicsTiming::gainAttack, fs);
        cF = onePoleCoeff (DynamicsTiming::fastRelease, fs);
        cC = onePoleCoeff (DynamicsTiming::slowCharge, fs);
        cS = onePoleCoeff (DynamicsTiming::slowRelease, fs);
        reset();
    }

    void reset() noexcept
    {
        e = f = s = 0.0;
        gain = 1.0;
        lastTargetDb = 0.0;
    }

    // Processes one sidechain sample; returns the applied linear gain (exactly 1 without reduction).
    double process (double d, double thresholdDb) noexcept
    {
        // 1. peak detector
        const double v = std::abs (d);
        if (v > e)
            e = e + (v - e) * aDet;
        else
            e = e * rDet;
        if (e < tiny)
            e = 0.0; // denormal / silence flush (does not change any audible result)

        // 2. static curve
        const double gr = e > 0.0 ? GainComputer::reductionDb (20.0 * std::log10 (e), thresholdDb) : 0.0;
        lastTargetDb = gr;
        const double t = gr > 0.0 ? 1.0 - std::pow (10.0, -gr / 20.0) : 0.0;

        // 3a. fast part: attack or fast release toward t
        if (t > f)
            f = f + (t - f) * cA;
        else
            f = f + (t - f) * cF;

        // 3b. slow part: charges toward f while the target exceeds it, otherwise releases toward t
        if (t > s)
            s = s + (f - s) * cC;
        else
            s = s + (t - s) * cS;

        // Exact identity once released (implementation detail outside the pseudocode, |change| <= 1e-12):
        // the one-poles only approach t asymptotically, so they are landed on t when practically there.
        if (std::abs (f - t) <= snapEpsilon)
            f = t;
        if (t <= s && s - t <= snapEpsilon)
            s = t;

        // 4. applied gain
        gain = 1.0 - std::max (f, s);
        return gain;
    }

    double getGain() const noexcept { return gain; }
    double getEnvelope() const noexcept { return e; }
    double getTargetReductionDb() const noexcept { return lastTargetDb; }

private:
    static constexpr double tiny = 1.0e-30;
    static constexpr double snapEpsilon = 1.0e-12;

    double fs = 48000.0;
    double aDet = 0.0, rDet = 0.0, cA = 0.0, cF = 0.0, cC = 0.0, cS = 0.0;
    double e = 0.0, f = 0.0, s = 0.0, gain = 1.0, lastTargetDb = 0.0;
};

} // namespace craft::deesser::dsp
