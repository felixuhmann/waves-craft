// Craft FET Limiter - level detector and gain-cell dynamics (spec r2 section 6). Host independent.
//
// The detector measures the sine-peak level of the internal (post Input gain) signal; the static law (spec 5)
// turns it into a target gain reduction T (dB, >= 0). The gain cell has one output state F and three memories:
//
//   C, C2  level memories  one-pole followers of T (tauC slow, tauC2 fast). The attack aims at
//                          Teff = T - beta (T - C) - beta2 (T - C2): beta > 0 is the slow deepening of the reduction
//                          with hold time (r2 13.4), beta2 < 0 the overshoot of a short burst (spec 6.1)
//   M      program memory  0 .. 1, charges with tauMCharge while the cell is compressing, decays with tauMDecay
//   F      output          attacks towards Teff with tauAttack * (21 dB / T)^depthExp; recovers at a rate in dB/ms
//
//     rate = ((1 - M) / msPerDbFast * E^2 / (E^2 + kneeDb^2) + M / msPerDbSlow * E / (E + kneeSlowDb))
//            / (1 + (F / depthDb)^depthPower),   E = F - T
//
// The last factor makes deep reduction recover slowly: that is the hold of r2 13.2 (the time to recover the first dB
// follows from the initial rate) and the acceleration towards the end. The knees give the tails of the ordinary
// ratios (All: both ~ 0, it reaches the target with a finite slope). M is the program dependence of spec 6.3.
// In steady state F = T, so the static law is exact. All parameters are physical times (spec 9).
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::fet_limiter::dsp
{

struct DynParams
{
    double tauAttackMs = 0.7;
    double beta = 0.0;
    double tauCMs = 2000.0;
    double tauMChargeMs = 30.0;
    double tauMDecayMs = 500.0;
    double msPerDbFast = 100.0;
    double msPerDbSlow = 200.0;
    double kneeDb = 5.0;
    double depthDb = 30.0;
    double depthPower = 2.0;
    double depthExp = 0.0;
    double beta2 = 0.0;
    double tauC2Ms = 10.0;
    double kneeSlowDb = 5.0;
    double overshootCapDb = 200.0; // an overshoot (beta2 < 0) does not lift Teff above this
};

// Coefficient of a one-pole smoother with time constant tau (ms) at sample rate fs.
inline double onePoleCoeff (double tauMs, double fs) noexcept
{
    if (tauMs <= 0.0)
        return 1.0;
    return 1.0 - std::exp (-1000.0 / (tauMs * fs));
}

// Peak detector with instantaneous attack, a hold of holdMs after the last peak and an exponential decay.
// A steady sine (any frequency above 1 / (2 holdMs)) gives a constant output equal to its peak.
class PeakDetector
{
public:
    static constexpr double holdMs = 2.0;
    static constexpr double decayMs = 1.0;

    void prepare (double fs) noexcept
    {
        holdSamples = std::max (1, int (std::lround (holdMs * 0.001 * fs)));
        decay = std::exp (-1000.0 / (decayMs * fs));
        reset();
    }

    void reset() noexcept
    {
        env = 0.0;
        counter = 0;
    }

    double process (double a) noexcept
    {
        if (a >= env)
        {
            env = a;
            counter = holdSamples;
        }
        else
        {
            if (a >= env * 0.995) // a peak of practically the same height refreshes the hold
                counter = holdSamples;
            if (counter > 0)
                --counter;
            else
                env = std::max (a, env * decay);
        }
        return env;
    }

    double get() const noexcept { return env; }

private:
    double env = 0.0, decay = 0.0;
    int holdSamples = 1, counter = 0;
};

class GainCell
{
public:
    static constexpr double referenceDepthDb = 21.0;

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        updateCoefficients();
        reset();
    }

    void reset() noexcept { c = c2State = f = m = 0.0; }

    void setParams (const DynParams& p) noexcept
    {
        params = p;
        updateCoefficients();
    }

    const DynParams& getParams() const noexcept { return params; }

    // One sample: target reduction (dB, >= 0) in, gain reduction (dB, >= 0) out.
    double process (double target) noexcept
    {
        c += (target - c) * cC;
        c2State += (target - c2State) * cC2;
        const double settled = target - params.beta * (target - c);
        const double over = settled - params.beta2 * (target - c2State);
        const double teff = over > settled ? std::max (settled, std::min (over, params.overshootCapDb)) : over;

        if (target >= 1.0 && target >= f - 0.5)
            m += (1.0 - m) * cMc;
        else
            m -= m * cMd;

        if (teff > f)
        {
            if (target != lastTarget)
            {
                lastTarget = target;
                const double factor = std::clamp (std::pow (referenceDepthDb / std::max (target, 0.01), params.depthExp), 0.25, 30.0);
                cA = onePoleCoeff (params.tauAttackMs * factor, fs);
            }
            f += (teff - f) * cA;
        }
        else
        {
            const double e = f - teff;
            const double rate = ((1.0 - m) * rFast * e * e / (e * e + params.kneeDb * params.kneeDb) + m * rSlow * e / (e + params.kneeSlowDb))
                                / (1.0 + std::pow (f / params.depthDb, params.depthPower));
            f = std::max (teff, f - rate * msPerSample);
        }
        return f;
    }

    double get() const noexcept { return f; }

private:
    void updateCoefficients() noexcept
    {
        lastTarget = -1.0; // the attack coefficient follows the target (see process)
        cC = onePoleCoeff (params.tauCMs, fs);
        cC2 = onePoleCoeff (params.tauC2Ms, fs);
        cMc = onePoleCoeff (params.tauMChargeMs, fs);
        cMd = onePoleCoeff (params.tauMDecayMs, fs);
        rFast = 1.0 / params.msPerDbFast;
        rSlow = 1.0 / params.msPerDbSlow;
        msPerSample = 1000.0 / fs;
    }

    DynParams params;
    double fs = 48000.0, msPerSample = 1000.0 / 48000.0;
    double cA = 1.0, cC = 1.0, cC2 = 1.0, cMc = 1.0, cMd = 1.0, rFast = 0.01, rSlow = 0.01;
    double c = 0.0, c2State = 0.0, f = 0.0, m = 0.0, lastTarget = -1.0;
};

} // namespace craft::fet_limiter::dsp
