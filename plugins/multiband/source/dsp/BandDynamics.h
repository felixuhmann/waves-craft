// Craft Multiband - per-band level-dependent gain with attack / release / behaviour / auto release
// (spec r1 sections 4, 5 and 6). Host-independent: no JUCE dependency.
//
// All time-dependent behaviour runs on the normalised gain change
//     nu = (g - 1) / (G_R - 1),   g = linear band gain without the Gain offset,  G_R = 10^(Range/20)
// (nu = 0: unity, nu = 1: full Range). The spec's envelope tables are Range-independent in this variable
// (attack and release fractions of the *linear* gain change agree for Range -24 / -12 / +12), so one law serves
// cut and boost.
//
// Per sample:
//   detector p: instantaneous amplitude (analytic-signal magnitude, linked over channels) with instant attack and
//               an exponential fall (peakReleaseMs) - a peak detector that reads a steady sine as its peak
//   target   nu_t = (G_R^f - 1) / (G_R - 1),  f = curve(20 log10 p - Threshold)
//   fast     a: rises toward nu_t with tau = Attack (one-pole), falls toward nu_t with tau = fastReleaseMs
//   onset    o: depth * (LP(nu_t, onsetMs) - LP(LP(nu_t, onsetMs), onsetDecayMs)), the short initial fast
//               component of the attack (decays within a few ms whatever the Attack)
//   charge   s: while a > s it charges toward a with tau = chargeMs (it holds the part of the gain change that
//               has lasted); while a < s it releases toward a with tau = hold + Release_eff / k(s)
//               Electro: k(s) = e0 + e1 (1 - s)^e2   (slow while the change is large, faster near the target)
//               Opto:    k(s) = o0 + o1 s^o2         (fast while the change is large, slower near the target)
//   memory   m: charges toward 1 while s charges (tau = memoryMs), decays during release (memoryDecay x the
//               release time constant): how long the preceding overload lasted.
//               Auto: Release_eff = Release (autoMin + (1 - autoMin) m), Manual: Release_eff = Release
//   output   nu = min(max(a, s) + o, max(a, s, nu_t))
// The constants (DynamicsLaw) were fitted to the section 5 / 6 tables (see IMPLEMENTATION_NOTES.md).
#pragma once

#include <algorithm>
#include <cmath>

#include "GainCurve.h"

namespace craft::multiband
{

struct DynamicsLaw
{
    double peakReleaseMs = 1.55369;
    double fastReleaseMs = 1.96836;
    double chargeMs = 46.8268;
    double holdMs = 59.8614;
    double holdOptoMs = 35.5607;
    double e0 = 0.480972, e1 = 2.63701, e2 = 1.40008;
    double o0 = 1.10027, o1 = 203.845, o2 = 7.56418;
    double autoMin = 0.05;
    double memoryMs = 365.094;
    double memoryDecay = 20.0;
    double onsetMs = 0.883292;
    double onsetDecayMs = 0.928495;
    double onsetDepth = 0.485577;

    static const DynamicsLaw& standard() noexcept
    {
        static const DynamicsLaw law {};
        return law;
    }
};

// Normalised gain change -> linear gain.
inline double gainFromNu (double nu, double rangeDb) noexcept
{
    const double gr = std::pow (10.0, rangeDb / 20.0);
    return 1.0 + nu * (gr - 1.0);
}

// Static curve value f -> normalised gain change.
inline double nuFromF (double f, double rangeDb) noexcept
{
    if (std::abs (rangeDb) < 1.0e-6)
        return f;
    const double gr = std::pow (10.0, rangeDb / 20.0);
    return (std::pow (gr, f) - 1.0) / (gr - 1.0);
}

class BandDynamics
{
public:
    void prepare (double sampleRate) noexcept
    {
        dtMs = 1000.0 / sampleRate;
        updateConstants();
        reset();
    }

    void setLaw (const DynamicsLaw& l) noexcept
    {
        law = l;
        updateConstants();
    }

    void reset() noexcept
    {
        p = a = s = m = o1 = o2 = nu = 0.0;
    }

    // envelopeSquared: squared instantaneous amplitude (linked: max over channels). Returns nu.
    double process (double envelopeSquared, double thresholdDb, double rangeDb, double attackMs, double releaseMs,
                    bool opto, bool autoRelease, const GainCurve& curve) noexcept
    {
        // peak detector on the squared envelope
        const double e2 = std::max (envelopeSquared, 1.0e-20);
        p = e2 > p ? e2 : p + (e2 - p) * cPeak;
        const double f = curve.eval (10.0 * std::log10 (p) - thresholdDb);

        if (rangeDb != curRange)
        {
            curRange = rangeDb;
            lnGr = rangeDb * (std::log (10.0) / 20.0);
            grMinus1 = std::expm1 (lnGr);
        }
        const double target = std::abs (rangeDb) < 1.0e-6 ? f : std::expm1 (lnGr * f) / grMinus1;

        if (attackMs != curAttackMs)
        {
            curAttackMs = attackMs;
            cAttack = coeff (attackMs);
        }

        // fast state
        a += (target - a) * (target > a ? cAttack : cFast);

        // onset component
        o1 += (target - o1) * cOnset;
        o2 += (o1 - o2) * cOnsetDecay;
        const double onset = law.onsetDepth * std::max (0.0, o1 - o2);

        // charged state + overload memory
        if (a > s)
        {
            s += (a - s) * cCharge;
            m += (1.0 - m) * cMemory;
        }
        else
        {
            const double sc = std::max (s, 0.0);
            const double k = opto ? law.o0 + law.o1 * std::pow (sc, law.o2)
                                  : law.e0 + law.e1 * std::pow (std::max (1.0 - sc, 0.0), law.e2);
            const double rel = autoRelease ? releaseMs * (law.autoMin + (1.0 - law.autoMin) * m) : releaseMs;
            const double tau = (opto ? law.holdOptoMs : law.holdMs) + rel / std::max (k, 1.0e-3);
            s += (a - s) * coeff (tau);
            m -= m * coeff (tau * law.memoryDecay);
        }

        const double base = std::max (a, s);
        nu = std::min (base + onset, std::max (base, target));
        return nu;
    }

    double getNu() const noexcept { return nu; }

private:
    double coeff (double tauMs) const noexcept { return -std::expm1 (-dtMs / std::max (tauMs, 1.0e-6)); }

    void updateConstants() noexcept
    {
        cPeak = coeff (law.peakReleaseMs);
        cFast = coeff (law.fastReleaseMs);
        cCharge = coeff (law.chargeMs);
        cMemory = coeff (law.memoryMs);
        cOnset = coeff (law.onsetMs);
        cOnsetDecay = coeff (law.onsetDecayMs);
        curAttackMs = -1.0;
    }

    DynamicsLaw law = DynamicsLaw::standard();
    double dtMs = 1000.0 / 48000.0;
    double p = 0.0, a = 0.0, s = 0.0, m = 0.0, o1 = 0.0, o2 = 0.0, nu = 0.0;
    double cPeak = 0.0, cFast = 0.0, cCharge = 0.0, cMemory = 0.0, cOnset = 0.0, cOnsetDecay = 0.0;
    double curAttackMs = -1.0, cAttack = 1.0;
    double curRange = 1.0e9, lnGr = 0.0, grMinus1 = 1.0;
};

} // namespace craft::multiband
