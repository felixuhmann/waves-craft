// Craft FET Limiter - level detector and gain-cell dynamics (spec r1 section 6). Host independent.
//
// The detector measures the sine-peak level of the internal (post Input gain) signal; the static law (spec 5)
// turns it into a target gain reduction T (dB, >= 0). The gain cell follows T through a small network of
// "charge" states that reproduces the measured attack, the program-dependent release and its shapes:
//
//   C  level memory     follows T with tauC (one pole); the attack aims at Teff = T - beta (T - C), so a level
//                       step first settles at (1 - beta) T and then creeps to T (beta > 0), or overshoots and
//                       settles back (beta < 0, the "feedback" overshoot of spec 6.1)
//   F  output state     attacks towards Teff with tauAttack * (21 dB / T)^depthExp (feedback-style: a deeper
//                       step attacks faster, light compression slower; factor limited to 0.25 .. 30), releases
//                       towards max(H, Teff) with tau1
//   H  intermediate     charges towards T with tauHCharge, releases towards max(K, T) with tau2
//   K  program memory   charges towards T with tauKCharge, releases towards T - offset with tau3 (floored at T);
//                       with accel > 0 the release speeds up as K falls below its recent peak M (a slowly
//                       decaying peak hold of K), giving the accelerating, nearly linear recovery of the All ratio
//
//   GR = F  (dB reduction)
//
// A short burst charges F (and partly H) but leaves K nearly empty, so the release is fast and mostly exponential;
// a long hold also charges K and the recovery becomes a slower, three-stage, nearly linear one (spec 6.3 "short
// bursts recover roughly exponentially, long holds closer to linearly"). A positive offset makes the last stage
// aim below the target so the recovery reaches it with a finite slope (the "almost linear in dB" release of the
// All ratio and of revision B). In steady state C = F = H = K = T, so the static law is exact.
// All parameters are physical times, so behaviour does not depend on the sample rate (spec 9).
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::fet_limiter::dsp
{

struct DynParams
{
    double tauAttackMs = 0.7;
    double beta = 0.15;
    double tauCMs = 15.0;
    double tauHChargeMs = 10.0;
    double tauKChargeMs = 60.0;
    double tau1Ms = 300.0;
    double tau2Ms = 600.0;
    double tau3Ms = 600.0;
    double releaseOffsetDb = 0.0;
    double accel = 0.0;
    double depthExp = 0.0;
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
    static constexpr double holdMs = 6.0;
    static constexpr double decayMs = 4.0;

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
    static constexpr double peakMemoryMs = 3000.0;
    static constexpr double referenceDepthDb = 21.0;

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        updateCoefficients();
        reset();
    }

    void reset() noexcept { c = f = h = k = m = 0.0; }

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
        const double teff = target - params.beta * (target - c);

        // K: program memory (M: slowly decaying peak hold of K)
        if (target > k)
            k += (target - k) * cKc;
        else
            k = std::max (target, k + (target - params.releaseOffsetDb - k - params.accel * (m - k)) * c3);
        if (k >= m)
            m = k;
        else
            m += (k - m) * cM;

        // H: intermediate stage
        if (target > h)
            h += (target - h) * cHc;
        else
            h += (std::max (k, target) - h) * c2;

        // F: output stage
        if (target != lastTarget)
        {
            lastTarget = target;
            const double factor = std::clamp (std::pow (referenceDepthDb / std::max (target, 0.01), params.depthExp), 0.25, 30.0);
            cA = onePoleCoeff (params.tauAttackMs * factor, fs);
        }
        if (teff > f)
            f += (teff - f) * cA;
        else
            f += (std::max (h, teff) - f) * c1;

        return f;
    }

    double get() const noexcept { return f; }

private:
    void updateCoefficients() noexcept
    {
        lastTarget = -1.0; // attack coefficient follows the target (see process)
        cC = onePoleCoeff (params.tauCMs, fs);
        cHc = onePoleCoeff (params.tauHChargeMs, fs);
        cKc = onePoleCoeff (params.tauKChargeMs, fs);
        c1 = onePoleCoeff (params.tau1Ms, fs);
        c2 = onePoleCoeff (params.tau2Ms, fs);
        c3 = onePoleCoeff (params.tau3Ms, fs);
        cM = onePoleCoeff (peakMemoryMs, fs);
    }

    DynParams params;
    double fs = 48000.0;
    double cM = 1.0, cA = 1.0, cC = 1.0, cHc = 1.0, cKc = 1.0, c1 = 1.0, c2 = 1.0, c3 = 1.0;
    double c = 0.0, f = 0.0, h = 0.0, k = 0.0, m = 0.0, lastTarget = -1.0;
};

} // namespace craft::fet_limiter::dsp
