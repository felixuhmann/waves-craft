// Craft Punch Compressor - gain smoothing: attack and program-dependent release (spec r2 sections 7.1, 7.2, 7.4, 7.5).
//
// Three stages on the linear gain-reduction gain (1 = no reduction); the applied gain is min(gA, gM):
//
//  * gA (instant stage): attack = one-pole on linear gain, time constant Attack (7.1, normative); release = one-pole
//    of about a millisecond at every Release setting (7.5: short overloads recover in milliseconds).
//  * A gate opens once reduction has lasted holdMs(Release) (8 ... 30 ms); only then do the other stages charge.
//  * gM (medium stage): with the gate open it charges quickly towards gA; otherwise it relaxes towards gS with a
//    Release-limited time constant of about 15 ms.
//  * gS (slow stage): with the gate open it charges towards gM (about 100 ms, independent of Release); otherwise
//    it recovers autonomously with the measured law: a one-pole on z = g^gamma towards (K t)^gamma, clamped at the
//    target t, time constant a * Release. gamma(Release) and a(Release) are fitted to the 7.4 release rows.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace craft::punch
{

struct ReleaseLaw
{
    static constexpr double K = 30.0, gamma = 1.1;
    // fitted to tables 7.5 / 6 (tools/fit/opt.py)
    static constexpr double fastMs = 1.11, mChargeMs = 3.575, mMaxMs = 12.49, mPerRel = 0.1351, sChargeMs = 142.4, sQuickMs = 22.0, sSplit = 0.40, gateDecay = 3.03, holdScale = 1.231, sLeakMs = 300.0, endSpeedup = 3.0, endGain = 0.72, slowFastF0 = 11.5, slowFastPf = 0.75, slowFastKf = 0.224, progUpMs = 40.0, progDownMs = 5.0;
    // anchors (ms) and fitted a (release time constant / Release), table 7.5 rows of 1000 ms bursts: with K this large
    // the recovery is nearly a straight ramp of g^gamma that ends after about 2 Release
    static constexpr std::array<double, 7> relMs { 3.0, 10.0, 30.0, 100.0, 300.0, 1000.0, 3000.0 };
    static constexpr std::array<double, 7> a { 150.0, 124.7, 110.2, 101.4, 93.3, 100.7, 99.7 };

    static double interp (const std::array<double, 7>& ys, double relMsValue) noexcept
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

    // time reduction must last before the slower stages charge (ms), linear in log10 Release
    static constexpr std::array<double, 7> holdAtAnchor { 34.0, 30.0, 22.0, 23.0, 18.0, 10.0, 7.8 };
    static double holdMs (double rel) noexcept { return holdScale * interp (holdAtAnchor, rel); }
    // fast-stage release under sustained reduction (fitted to the section 6 vectors); short overloads use fastMs
    static double sustainedFastMs (double rel) noexcept { return std::min (slowFastF0 * std::pow (rel / 100.0, slowFastPf), slowFastKf * rel); }
    static double mediumMs (double rel) noexcept { return std::min (mMaxMs + 4.0 * std::min (1.0, rel / 3000.0), std::max (std::min (2.5, 0.25 * rel), mPerRel * rel)); }
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
        fastBase = std::min (ReleaseLaw::fastMs, 0.2 * releaseMs);
        fastSpan = std::max (0.0, ReleaseLaw::sustainedFastMs (releaseMs) - fastBase);
        cpu = coeff (ReleaseLaw::progUpMs);
        cpd = coeff (ReleaseLaw::progDownMs);
        cmc = coeff (ReleaseLaw::mChargeMs);
        cm = coeff (ReleaseLaw::mediumMs (releaseMs));
        csc = coeff (ReleaseLaw::sChargeMs);
        csq = coeff (ReleaseLaw::sQuickMs);
        csl = coeff (ReleaseLaw::sLeakMs);
        holdLimit = ReleaseLaw::holdMs (releaseMs);
        msPerSample = 1000.0 / fs;
        gam = ReleaseLaw::gamma;
        cs = coeff (ReleaseLaw::interp (ReleaseLaw::a, releaseMs) * releaseMs);
        csEnd = coeff (ReleaseLaw::interp (ReleaseLaw::a, releaseMs) * releaseMs / ReleaseLaw::endSpeedup);
        kGam = std::pow (ReleaseLaw::K, gam);
    }

    void reset() noexcept
    {
        gA = gM = gS = 1.0;
        held = 0.0;
        prog = 0.0;
    }

    // t: target linear gain-reduction gain (<= 1). Returns the applied gain.
    double process (double t) noexcept
    {
        // programme state: 0 ... 1, how sustained the reduction has been
        prog = t < 0.995 ? 1.0 + cpu * (prog - 1.0) : cpd * prog;

        // the gate counts time spent reducing; time spent recovering (t above gA) counts against it
        const bool recovering = t > 1.02 * gA || t >= 0.995; // 0.17 dB: sample-peak jitter of a steady tone is not recovery
        held = recovering ? std::max (0.0, held - ReleaseLaw::gateDecay * msPerSample)
                          : std::min (held + msPerSample * std::min (1.5, (1.0 - t) / 0.775), holdLimit + 1.0); // deeper opens sooner

        if (t < gA)
            gA = t + ca * (gA - t);
        else if (gA < t)
        {
            const double p3 = prog * prog * prog;
            gA = t + std::exp (-msPerSample / (fastBase + fastSpan * p3)) * (gA - t);
            if (t - gA < 1.0e-12)
                gA = t;
        }

        const bool open = held >= holdLimit;

        if (open && gA < gM)
            gM = gA + cmc * (gM - gA);
        else if (gM < gS)
        {
            gM = gS + cm * (gM - gS);
            if (gS - gM < 1.0e-12)
                gM = gS;
        }

        if (open && gM < gS)
            gS = gM + (1.0 - gS < ReleaseLaw::sSplit * (1.0 - gM) ? csq : csc) * (gS - gM); // quick part, then slow
        else if (t < gA && gA < gS) // gate closed: repeated peaks still charge the slow stage a little
        {
            gS = gA + csl * (gS - gA);
            gM = std::min (gM, gS);
        }
        else if (gS < t)
        {
            const double zt = kGam * std::pow (t, gam);
            const double z = zt + (gS > ReleaseLaw::endGain ? csEnd : cs) * (std::pow (gS, gam) - zt); // the last dB go faster
            const double g = std::pow (z, 1.0 / gam);
            gS = g < t ? g : t;
        }
        return gA < gM ? gA : gM;
    }

    double getGain() const noexcept { return std::min (gA, gM); }

    // The gain expected `samples` samples from now if the target t stays as it is. The output stage needs it for the
    // sample entering the look-ahead: its peak is already in the target but the attack has not acted on it yet.
    double predictGain (double t, int samples) const noexcept
    {
        const double ms = samples * msPerSample;
        double a = gA;
        if (t < gA)
            a = t + std::exp (-ms / attackMs) * (gA - t);
        else if (gA < t)
            a = t + std::exp (-ms / (fastBase + fastSpan * prog * prog * prog)) * (gA - t);
        return a < gM ? a : gM;
    }

private:
    double fs = 48000.0, attackMs = 7.0, releaseMs = 100.0;
    double ca = 0.0, cpu = 0.0, cpd = 0.0, fastSpan = 0.0, fastBase = 0.9, csEnd = 0.0, prog = 0.0, cmc = 0.0, cm = 0.0, csc = 0.0, csq = 0.0, csl = 0.0, cs = 0.0, gam = 1.0, kGam = 2.0;
    double holdLimit = 15.0, msPerSample = 1.0 / 48.0, held = 0.0;
    double gA = 1.0, gM = 1.0, gS = 1.0;
};

} // namespace craft::punch
