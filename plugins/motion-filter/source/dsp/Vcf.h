// Craft Motion Filter - the voltage-controlled-filter stage of one channel (spec r1 section 5).
#pragma once

#include <array>
#include <cmath>

#include "Blocks.h"

namespace craft::mf
{

// Per-sample control values for the filter stage (already modulated, clamped and mapped).
struct VcfControl
{
    double g = 0.5;              // tan(pi fc / fs) for the SVF types
    double k = 1.0;              // 1/Q (small-signal)
    double combDelaySamples = 0; // comb delay
    double combGain = 0.5;       // comb feedback gain
    double ampGain = 1.0;        // amp type linear gain
};

// All six filter types run continuously; their outputs are blended with one-pole-smoothed weights so a
// type or slope change is a click-free ~20 ms crossfade (spec §10).
class Vcf
{
public:
    void prepare (double fs)
    {
        comb.prepare (fs);
        weightSmoother = std::exp (-1.0 / (0.004 * fs));
        reset();
    }

    void reset() noexcept
    {
        s1.reset();
        s2.reset();
        comb.reset();
        for (size_t i = 0; i < weights.size(); ++i)
            weights[i] = (int (i) == int (type)) ? 1.0 : 0.0;
        slopeWeight = slope24 ? 1.0 : 0.0;
    }

    void setType (FilterType t) noexcept { type = t; }
    void setSlope24 (bool s) noexcept { slope24 = s; }
    void setResonanceLimiting (double kappa) noexcept { s1.kappa = s2.kappa = kappa; }
    double combMaxDelay() const noexcept { return comb.maxDelay(); }
    double typeWeight (FilterType t) const noexcept { return weights[size_t (t)]; }

    // Response of one SVF section for an SVF type (LP, HP, BP, BR).
    static double svfResponse (int typeIndex, const SvfSection::Out& o, double x) noexcept
    {
        switch (typeIndex)
        {
            case 0: return o.lp;
            case 1: return o.hp;
            case 2: return o.bp;
            case 3: return x - o.bp; // notch = LP + HP
            default: return 0.0;
        }
    }

    double process (double x, const VcfControl& c) noexcept
    {
        // advance the crossfade weights
        const double b = 1.0 - weightSmoother;
        for (size_t i = 0; i < weights.size(); ++i)
            weights[i] += ((int (i) == int (type) ? 1.0 : 0.0) - weights[i]) * b;
        slopeWeight += ((slope24 ? 1.0 : 0.0) - slopeWeight) * b;

        // SVF types: section 1, then section 2 fed with section 1's response (same fc and Q)
        const double svfWeight = weights[0] + weights[1] + weights[2] + weights[3];
        const auto o1 = s1.process (x, c.g, c.k);
        double r1 = 0.0;
        for (int t = 0; t < 4; ++t)
            r1 += weights[size_t (t)] * svfResponse (t, o1, x);
        r1 = svfWeight > 1.0e-9 ? r1 / svfWeight : o1.lp;
        const auto o2 = s2.process (r1, c.g, c.k);
        double r2 = 0.0;
        for (int t = 0; t < 4; ++t)
            r2 += weights[size_t (t)] * svfResponse (t, o2, r1);
        r2 = svfWeight > 1.0e-9 ? r2 / svfWeight : o2.lp;
        const double svfOut = svfWeight * ((1.0 - slopeWeight) * r1 + slopeWeight * r2);

        const double combOut = comb.process (x, c.combDelaySamples, c.combGain);
        const double ampOut = x * c.ampGain;

        return svfOut + weights[4] * combOut + weights[5] * ampOut;
    }

private:
    FilterType type = FilterType::LowPass;
    bool slope24 = false;
    SvfSection s1, s2;
    CombFilter comb;
    std::array<double, numFilterTypes> weights { { 1, 0, 0, 0, 0, 0 } };
    double slopeWeight = 0.0, weightSmoother = 0.0;
};

// Analog-prototype magnitude of the filter (for tests and the response display). f and fc in Hz.
inline double prototypeMagnitude (FilterType type, bool slope24, double f, double fc, double q) noexcept
{
    const double w = f / fc;
    const double dRe = 1.0 - w * w, dIm = w / q;
    const double den = std::sqrt (dRe * dRe + dIm * dIm);
    double num = 1.0;
    switch (type)
    {
        case FilterType::LowPass:    num = 1.0; break;
        case FilterType::HighPass:   num = w * w; break;
        case FilterType::BandPass:   num = w / q; break;
        case FilterType::BandReject: num = std::abs (1.0 - w * w); break;
        default:                     return 1.0;
    }
    const double h = num / std::max (den, 1.0e-12);
    return slope24 ? h * h : h;
}

} // namespace craft::mf
