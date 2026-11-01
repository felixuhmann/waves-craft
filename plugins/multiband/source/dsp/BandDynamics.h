// Craft Multiband - per-band level-dependent gain with attack / release / behaviour / auto release
// (spec r2 sections 4, 5 and 6). Host-independent: no JUCE dependency.
//
// All time-dependent behaviour runs on the normalised gain change
//     nu = (g - 1) / (G_R - 1),   g = linear band gain without the Gain offset,  G_R = 10^(Range/20)
// (nu = 0: unity, nu = 1: full Range). The spec's envelope tables are Range-independent in this variable
// (attack and release fractions of the *linear* gain change agree for Range -24 / -12 / +12), so one law serves
// cut and boost.
//
// Per sample:
//   detector p: instantaneous amplitude (analytic-signal magnitude, linked over channels), squared, with instant
//               rise and an exponential fall (peakReleaseMs / peakReleaseOptoMs) - reads a steady sine as its peak
//   target   nu_t = (G_R^f - 1) / (G_R - 1),  f = curve(10 log10 p - Threshold)
//   fast     a: rises toward nu_t with tau = attackScale x Attack; falls toward nu_t with
//               tau = tauF * shape,  tauF = fastMinMs + fastSpanMs Release / (Release + fastHalfMs)
//               (Auto: x 1 - (1 - autoFast) Release / (Release + autoHalfMs), i.e. hardly any change at short
//               Release; Opto: x optoFastScale)
//               Electro shape = 1 + electroSlowGain a^electroSlowPow       (slow from a large change)
//               Opto    shape = 1 + optoSlowGain (1 - a)^optoSlowPow       (photocell: slow near unity)
//   onset    o: onsetDepth (LP(nu_t, onsetMs) - LP(LP(nu_t, onsetMs), onsetDecayMs)): the short initial fast
//               component of the attack (gone after a few ms whatever the Attack)
//   slow     s: while a > s it charges toward a with tau = chargeMs / chargeOptoMs (~0.1 s: the part of the gain
//               change that has lasted); while a < s it releases toward a with tau = hold + Release / k(s)
//               Electro: k(s) = e0 + e1 (1 - s)^e2   (slow while the change is large, faster near the target)
//               Opto:    k(s) = o0 + o1 s^o2         (fast while the change is large, slower near the target)
//   output   nu = min(max(a, s) + o, max(a, s, nu_t))
// So the release depends on how long the preceding overload lasted in every mode (spec r2 5.3): a short overload
// leaves mostly the fast component, a sustained one has charged s, which releases at the Release / Behavior law.
// Auto shortens only the fast component (autoFast), so after a sustained overload Auto equals Manual.
// All states are clamped and reset if they ever become non-finite (spec r2 section 6 robustness).
// The constants (DynamicsLaw) were fitted to the spec r2 section 5 / 6 tables (see IMPLEMENTATION_NOTES.md).
#pragma once

#include <algorithm>
#include <cmath>

#include "GainCurve.h"

namespace craft::multiband
{

struct DynamicsLaw
{
    double peakReleaseMs = 1.1408, peakReleaseOptoMs = 0.136037;
    double fastMinMs = 0.0107804, fastSpanMs = 11.9903, fastHalfMs = 86.8705;
    double autoFast = 0.417326, autoHalfMs = 104.995, optoFastScale = 0.50374;
    double electroSlowGain = 2.75052, electroSlowPow = 4.44609;
    double optoSlowGain = 35.5209, optoSlowPow = 2.10309;
    double chargeMs = 130.303, chargeOptoMs = 109.588;
    double holdMs = 69.3679, holdOptoMs = 27.2038;
    double e0 = 0.695809, e1 = 8.17755, e2 = 2.85608;
    double o0 = 3.7747, o1 = 1637.83, o2 = 12;
    double attackScale = 0.896465;
    double onsetMs = 0.680861, onsetDecayMs = 1.06319, onsetDepth = 0.348229;

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

    void reset() noexcept { p = a = s = o1 = o2 = nu = 0.0; }

    // envelopeSquared: squared instantaneous amplitude (linked: max over channels). Returns nu in [0, 1].
    double process (double envelopeSquared, double thresholdDb, double rangeDb, double attackMs, double releaseMs,
                    bool opto, bool autoRelease, const GainCurve& curve) noexcept
    {
        // peak detector on the squared envelope
        const double e2 = std::isfinite (envelopeSquared) ? std::max (envelopeSquared, 1.0e-20) : 1.0e-20;
        p = e2 > p ? e2 : p + (e2 - p) * (opto ? cPeakOpto : cPeak);
        double f = curve.eval (10.0 * std::log10 (p) - thresholdDb);
        if (! std::isfinite (f))
            f = 0.0;

        if (rangeDb != curRange)
        {
            curRange = rangeDb;
            lnGr = rangeDb * (std::log (10.0) / 20.0);
            grMinus1 = std::expm1 (lnGr);
        }
        double target = std::abs (rangeDb) < 1.0e-6 ? f : std::expm1 (lnGr * f) / grMinus1;
        target = std::isfinite (target) ? std::clamp (target, 0.0, 1.0) : 0.0;

        if (attackMs != curAttackMs)
        {
            curAttackMs = attackMs;
            cAttack = coeff (attackMs * law.attackScale);
        }
        if (releaseMs != curReleaseMs || autoRelease != curAuto || opto != curOpto)
        {
            curReleaseMs = releaseMs;
            curAuto = autoRelease;
            curOpto = opto;
            const double tf = law.fastMinMs + law.fastSpanMs * releaseMs / (releaseMs + law.fastHalfMs);
            const double autoScale = 1.0 - (1.0 - law.autoFast) * releaseMs / (releaseMs + law.autoHalfMs);
            tauFall = tf * (autoRelease ? autoScale : 1.0) * (opto ? law.optoFastScale : 1.0);
        }

        // fast component
        if (target > a)
            a += (target - a) * cAttack;
        else
        {
            const double ac = std::clamp (a, 0.0, 1.0);
            const double shape = opto ? 1.0 + law.optoSlowGain * std::pow (1.0 - ac, law.optoSlowPow)
                                      : 1.0 + law.electroSlowGain * std::pow (ac, law.electroSlowPow);
            a += (target - a) * coeff (tauFall * shape);
        }

        // onset component
        o1 += (target - o1) * cOnset;
        o2 += (o1 - o2) * cOnsetDecay;
        const double onset = law.onsetDepth * std::max (0.0, o1 - o2);

        // slow (charged) component
        if (a > s)
            s += (a - s) * (opto ? cChargeOpto : cCharge);
        else
        {
            const double sc = std::clamp (s, 0.0, 1.0);
            const double k = opto ? law.o0 + law.o1 * std::pow (sc, law.o2)
                                  : law.e0 + law.e1 * std::pow (1.0 - sc, law.e2);
            s += (a - s) * coeff ((opto ? law.holdOptoMs : law.holdMs) + releaseMs / std::max (k, 1.0e-3));
        }

        const double base = std::max (a, s);
        nu = std::min (base + onset, std::max (base, target));
        if (! std::isfinite (nu) || ! std::isfinite (a) || ! std::isfinite (s) || ! std::isfinite (p))
        {
            reset();
            return 0.0;
        }
        nu = std::clamp (nu, 0.0, 1.0);
        return nu;
    }

    double getNu() const noexcept { return nu; }

private:
    double coeff (double tauMs) const noexcept { return -std::expm1 (-dtMs / std::max (tauMs, 1.0e-6)); }

    void updateConstants() noexcept
    {
        cPeak = coeff (law.peakReleaseMs);
        cPeakOpto = coeff (law.peakReleaseOptoMs);
        cCharge = coeff (law.chargeMs);
        cChargeOpto = coeff (law.chargeOptoMs);
        cOnset = coeff (law.onsetMs);
        cOnsetDecay = coeff (law.onsetDecayMs);
        curAttackMs = -1.0;
        curReleaseMs = -1.0;
    }

    DynamicsLaw law = DynamicsLaw::standard();
    double dtMs = 1000.0 / 48000.0;
    double p = 0.0, a = 0.0, s = 0.0, o1 = 0.0, o2 = 0.0, nu = 0.0;
    double cPeak = 0.0, cPeakOpto = 0.0, cCharge = 0.0, cChargeOpto = 0.0, cOnset = 0.0, cOnsetDecay = 0.0;
    double curAttackMs = -1.0, cAttack = 1.0, curReleaseMs = -1.0, tauFall = 1.0;
    bool curAuto = false, curOpto = false;
    double curRange = 1.0e9, lnGr = 0.0, grMinus1 = 1.0;
};

} // namespace craft::multiband
