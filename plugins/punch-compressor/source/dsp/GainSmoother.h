// Craft Punch Compressor - gain smoothing: attack and program-dependent release (spec r1 sections 7.1 / 7.2 / 7.4).
//
// Two stages, both on the linear gain-reduction gain g (1 = no reduction):
//
//  * fast stage g1: attack = one-pole on linear gain with time constant Attack (section 7.1, normative);
//    release = one-pole on log gain with a short time constant tf(Release) - it lets the gain recover
//    between the peaks of fast material (section 6: equal-RMS noise vs sine, low-frequency ripple).
//  * slow stage g2: charges down towards g1 (one-pole on log gain, time constant tc(Release)) and releases with
//    the measured release law: a one-pole on z = g^gamma towards (K t)^gamma, clamped at the target t, time constant
//    a * Release. gamma(Release) and a(Release) are fitted to the section 7.4 release table (K = 2), linear in
//    log10(Release) between the fitted anchors.
//
// The applied gain is min(g1, g2): after sustained compression both stages sit at the target and the
// recovery follows the slow law (section 7.4); short peaks only charge g2 partially and recover quickly.
// The informative law of section 7.2 (one-pole in dB, 0.7 Release) misses some release-table points by more
// than the tolerance; see IMPLEMENTATION_NOTES.md ("Release law").
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace craft::punch
{

struct ReleaseLaw
{
    static constexpr double K = 2.0;
    // fast stage / charge time constants: tf = min(f0 (Rel/100)^pf, kf Rel), tc = c0 (Rel/100)^pc  [ms]
    static constexpr double fastF0 = 7.008, fastPf = 0.748, fastKf = 0.224, chargeC0 = 79.85, chargePc = 0.0;
    // anchors (ms) and fitted gamma / a
    static constexpr std::array<double, 8> relMs { 3.0, 30.0, 100.0, 250.0, 300.0, 500.0, 1000.0, 3000.0 };
    static constexpr std::array<double, 8> gamma { 0.787, 1.009, 0.817, 0.346, 0.452, 0.432, 0.573, 1.113 };
    static constexpr std::array<double, 8> a { 4.429, 4.140, 3.117, 1.837, 2.007, 1.963, 2.372, 4.513 };

    static double interp (const std::array<double, 8>& ys, double relMsValue) noexcept
    {
        const double l = std::log10 (std::max (relMsValue, 1.0e-3));
        if (relMsValue <= relMs.front()) return ys.front();
        if (relMsValue >= relMs.back()) return ys.back();
        size_t k = 0;
        while (k + 2 < relMs.size() && relMs[k + 1] <= relMsValue)
            ++k;
        const double l0 = std::log10 (relMs[k]), l1 = std::log10 (relMs[k + 1]);
        return ys[k] + (l - l0) / (l1 - l0) * (ys[k + 1] - ys[k]);
    }

    // fast-stage release and slow-stage charge time constants (ms)
    static double fastReleaseMs (double rel) noexcept { return std::min (fastF0 * std::pow (rel / 100.0, fastPf), fastKf * rel); }
    static double chargeMs (double rel) noexcept { return chargeC0 * std::pow (rel / 100.0, chargePc); }
};

class GainSmoother
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        setTimes (attackMs, releaseMs);
        reset();
    }

    void setTimes (double newAttackMs, double newReleaseMs) noexcept
    {
        attackMs = newAttackMs;
        releaseMs = newReleaseMs;
        auto coeff = [this] (double ms) { return std::exp (-1.0 / (std::max (ms, 1.0e-3) * 0.001 * fs)); };
        ca = coeff (attackMs);
        cf = coeff (ReleaseLaw::fastReleaseMs (releaseMs));
        cc = coeff (ReleaseLaw::chargeMs (releaseMs));
        gam = ReleaseLaw::interp (ReleaseLaw::gamma, releaseMs);
        cs = coeff (ReleaseLaw::interp (ReleaseLaw::a, releaseMs) * releaseMs);
        kGam = std::pow (ReleaseLaw::K, gam);
    }

    void reset() noexcept { g1 = g2 = 1.0; }

    // target: linear gain-reduction gain (<= 1). Returns the applied gain.
    double process (double t) noexcept
    {
        // fast stage
        if (t < g1)
            g1 = t + ca * (g1 - t);
        else if (g1 < t)
        {
            g1 = t * std::exp (cf * std::log (g1 / t));
            if (t - g1 < 1.0e-12 * t)
                g1 = t;
        }
        // slow stage
        if (g1 < g2)
            g2 = g1 * std::exp (cc * std::log (g2 / g1));
        else if (g2 < t)
        {
            const double zt = kGam * std::pow (t, gam);
            const double z = zt + cs * (std::pow (g2, gam) - zt);
            const double g = std::pow (z, 1.0 / gam);
            g2 = g < t ? g : t;
        }
        return g1 < g2 ? g1 : g2;
    }

    double getGain() const noexcept { return std::min (g1, g2); }

private:
    double fs = 48000.0, attackMs = 7.0, releaseMs = 100.0;
    double ca = 0.0, cf = 0.0, cc = 0.0, cs = 0.0, gam = 1.0, kGam = 2.0;
    double g1 = 1.0, g2 = 1.0;
};

} // namespace craft::punch
