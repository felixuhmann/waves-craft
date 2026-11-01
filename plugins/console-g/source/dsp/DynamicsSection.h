// Craft Console G - run-time dynamics: linked detector, compressor gain computer and expander / gate state.
#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "Dynamics.h"

namespace craft::consoleg::dsp
{

struct DynamicsSettings
{
    double compThreshold = 0.0; // display dB
    double compRatio = 1.0;     // 1 = off, may be +inf
    bool compFast = false;
    double compRelease = 0.4;   // s
    double expThreshold = -30.0;
    double expRange = 0.0;      // 0 = off
    bool gate = false;
    bool expFast = false;
    double expRelease = 0.2;    // s
};

// Structure (spec r2 sections 5, 5.1, 6; one linked detector, one gain element):
//   rectifier:  e1 = max(p, e1 * exp(-1/(tau1 fs))), p = max_c |sidechain_c|;
//   level:      e2 one-pole on e1 (linear domain, short constants). Auto: symmetric, reads the mean of e1. Fast: quick
//               attack, slower release, reads near the peak - that reading difference is the extra static reduction
//               of fast attack (spec 5), and its ripple is the fast-attack gain modulation (H3);
//               L = 20 log10 e2 - cal + delta (cal: auto reading of a 1 kHz sine, so L = sine peak dBFS);
//   gain:       g_i = curve(L) - make-up (curve = comp_static row), then a smoother IN dB: two parallel one-poles with
//               the same attack and different releases, output = w g1 + (1-w) g2. Being linear in dB, the relative
//               times do not depend on threshold, ratio or depth (spec 5.1).
//   expander:   ex = one-pole (attack, short release) of e1 -> level -> depth state D.
struct DetectorConstants
{
    static constexpr double stage1TauMs = 0.06;
    static constexpr double levelAutoMs = 1.0;
    static constexpr double levelFastAttackMs = 0.8, levelFastReleaseMs = 5.0;
    static constexpr double attackMs = 42.0, fastAttackMs = 1.7;
    // Gain-modulation distortion (spec 5, r2 10): a small part of the detector ripple reaches the gain directly.
    static constexpr double rippleAuto = 0.018, rippleFast = 0.33;
    static constexpr double calTrimDb = 0.1;
    static constexpr double fastDeltaDb = 0.8;            // residual level shift of fast attack (spec 9.2)
    static constexpr double releaseDeltaDbPerDecade = 2.1; // level shift vs release (spec 9.2, release 4 s row)

    static constexpr double expSmoothAttackMs = 2.5, expSmoothReleaseMs = 3.0;
    static constexpr double expOpenMs = 1.9, expOpenFastMs = 0.2;
    static constexpr double silenceLevel = 1.0e-6;        // -120 dBFS: below this the expander state holds

    // Closing of expander / gate: tau vs release (spec r2 section 6 tables: 0.2 s -> 0.205, 1 s -> 0.77, 4 s -> 2.8).
    static double closeTauSeconds (double releaseS) noexcept
    {
        static const std::vector<double> x { std::log (0.2), std::log (1.0), std::log (4.0) };
        static const std::vector<double> y { std::log (0.205), std::log (0.77), std::log (2.8) };
        return std::exp (interpLinearExtrap (x, y, std::log (std::max (0.01, releaseS))));
    }

    struct ReleaseLaw { double w, tau1Ms, tau2Ms; };

    // Two-exponential fit of the spec 5.1 release rows (t50/t63/t90), interpolated in ln(release).
    static ReleaseLaw releaseLaw (bool fast, double releaseS) noexcept
    {
        static const std::vector<double> x { std::log (0.1), std::log (0.4), std::log (1.2), std::log (4.0) };
        static const std::vector<double> w { 1.0, 0.643, 0.346, 0.1 };
        static const std::vector<double> t1 { std::log (84.5), std::log (95.2), std::log (88.7), std::log (20.0) };
        static const std::vector<double> t2 { std::log (215.1), std::log (215.1), std::log (461.2), std::log (524.0) };
        const double lr = std::log (std::max (0.01, releaseS));
        if (! fast)
            return { interpLinear (x, w, lr), std::exp (interpLinear (x, t1, lr)), std::exp (interpLinear (x, t2, lr)) };
        if (releaseS >= 0.4) // above the measured fast rows: the slow part scales like the auto law
            return { 0.84, 44.0, 741.9 * std::exp (interpLinear (x, t2, lr)) / 215.1 };
        static const std::vector<double> fx { std::log (0.1), std::log (0.4) };
        static const std::vector<double> fw { 1.0, 0.84 };
        static const std::vector<double> ft1 { std::log (22.0), std::log (44.0) };
        return { interpLinear (fx, fw, lr), std::exp (interpLinear (fx, ft1, lr)), 741.9 };
    }

    static double interpLinearExtrap (const std::vector<double>& xs, const std::vector<double>& ys, double x) noexcept
    {
        if (x <= xs.front() || x >= xs.back())
        {
            const size_t i = x <= xs.front() ? 0 : xs.size() - 2;
            return ys[i] + (x - xs[i]) * (ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]);
        }
        return interpLinear (xs, ys, x);
    }

    static double alpha (double tauMs, double fs) noexcept { return 1.0 - std::exp (-1000.0 / (std::max (tauMs, 1.0e-3) * fs)); }

    static double deltaDb (bool fast, double releaseS) noexcept
    {
        return (fast ? fastDeltaDb : 0.0) + releaseDeltaDbPerDecade * std::log10 (std::max (0.01, releaseS) / 0.4);
    }

    static double levelStep (double e2, double e1, double aA, double aR) noexcept { return e2 + (e1 > e2 ? aA : aR) * (e1 - e2); }
};

// Mean reading (dB re. sine peak) of a level stage for a steady 1 kHz sine.
inline double simulateLevelReadingDb (double attackMs, double releaseMs, double fs = 48000.0)
{
    const double d1 = std::exp (-1000.0 / (DetectorConstants::stage1TauMs * fs));
    const double aA = DetectorConstants::alpha (attackMs, fs), aR = DetectorConstants::alpha (releaseMs, fs);
    double e1 = 0.0, e2 = 0.0, acc = 0.0;
    const int settle = int (fs * 0.2), measure = int (fs * 0.1);
    for (int i = 0; i < settle + measure; ++i)
    {
        e1 = std::max (std::abs (std::sin (2.0 * pi * 1000.0 * i / fs)), e1 * d1);
        e2 = DetectorConstants::levelStep (e2, e1, aA, aR);
        if (i >= settle)
            acc += gainToDb (e2);
    }
    return acc / measure;
}

// Level calibration (a function of the rate only through small sampling effects, so computed once per rate).
struct DetectorCalibration
{
    double comp = 0.0, expander = 0.0;
    explicit DetectorCalibration (double fs = 48000.0)
    {
        comp = simulateLevelReadingDb (DetectorConstants::levelAutoMs, DetectorConstants::levelAutoMs, fs) + DetectorConstants::calTrimDb;
        expander = simulateLevelReadingDb (DetectorConstants::expSmoothAttackMs, DetectorConstants::expSmoothReleaseMs, fs);
    }
};

class DynamicsSection
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        d1 = std::exp (-1000.0 / (DetectorConstants::stage1TauMs * fs));
        rippleAlpha = DetectorConstants::alpha (2.0, fs);
        xA = DetectorConstants::alpha (DetectorConstants::expSmoothAttackMs, fs);
        xR = DetectorConstants::alpha (DetectorConstants::expSmoothReleaseMs, fs);
        calibration = DetectorCalibration (fs);
        reset();
        setSettings (settings, true);
    }

    void reset() noexcept
    {
        e1 = e2 = ex = gs1 = gs2 = giSlow = 0.0;
        depth = 0.0;
        gateOpen = true;
    }

    void setSettings (const DynamicsSettings& s, bool force = false) noexcept
    {
        const bool compChanged = force || s.compThreshold != settings.compThreshold || s.compRatio != settings.compRatio
                                 || s.compFast != settings.compFast || s.compRelease != settings.compRelease;
        const bool expChanged = force || compChanged || s.expThreshold != settings.expThreshold
                                || s.expRange != settings.expRange || s.gate != settings.gate || s.expFast != settings.expFast
                                || s.expRelease != settings.expRelease;
        settings = s;
        if (compChanged)
        {
            curve = CompressorTable::get().curve (s.compThreshold, s.compRatio);
            compActive = CompressorTable::uOf (s.compRatio) > 0.0;
            makeupDb = curve[0];
            calibDb = calibration.comp - DetectorConstants::deltaDb (s.compFast, s.compRelease);
            aA = DetectorConstants::alpha (s.compFast ? DetectorConstants::levelFastAttackMs : DetectorConstants::levelAutoMs, fs);
            aR = DetectorConstants::alpha (s.compFast ? DetectorConstants::levelFastReleaseMs : DetectorConstants::levelAutoMs, fs);
            const auto law = DetectorConstants::releaseLaw (s.compFast, s.compRelease);
            gW = law.w;
            ripple = s.compFast ? DetectorConstants::rippleFast : DetectorConstants::rippleAuto;
            gA = DetectorConstants::alpha (s.compFast ? DetectorConstants::fastAttackMs : DetectorConstants::attackMs, fs);
            gR1 = DetectorConstants::alpha (law.tau1Ms, fs);
            gR2 = DetectorConstants::alpha (law.tau2Ms, fs);
        }
        if (! expChanged)
            return;
        const auto& em = ExpanderModel::get();
        expActive = s.expRange > 0.0;
        expOnset = s.gate ? em.onsetDb (s.expThreshold) : em.onsetDb (s.expThreshold, s.expRange);
        expSlope = em.slope (s.expThreshold); // dB of attenuation per dB below the onset (~1.2)
        gateOpenLevel = em.gateOpenDb (s.expThreshold);
        gateCloseLevel = em.gateCloseDb (s.expThreshold);
        if (s.gate)
        {
            offset = em.gateOffset();
            floorDb = em.gateFloorDb (s.expRange);
            depthCap = floorDb + offset;
        }
        else
        {
            offset = em.expanderOffset();
            floorDb = std::max (0.0, em.expanderDepthMax (s.expRange) - offset);
            depthCap = em.expanderDepthMax (s.expRange);
        }
        openAlpha = DetectorConstants::alpha (s.expFast ? DetectorConstants::expOpenFastMs : DetectorConstants::expOpenMs, fs);
        closeAlpha = DetectorConstants::alpha (1000.0 * DetectorConstants::closeTauSeconds (s.expRelease), fs);
    }

    // One sample: linked detector input peak (max over channels of |sidechain|) -> total dynamics gain in dB.
    double process (double peak) noexcept
    {
        e1 = std::max (peak, e1 * d1);
        ex = DetectorConstants::levelStep (ex, e1, xA, xR);

        compDb = 0.0;
        if (compActive)
        {
            e2 = DetectorConstants::levelStep (e2, e1, aA, aR);
            const double gi = CompressorTable::evaluate (curve, gainToDb (e2) - calibDb) - makeupDb;
            gs1 += (gi < gs1 ? gA : gR1) * (gi - gs1);
            gs2 += (gi < gs2 ? gA : gR2) * (gi - gs2);
            const double g = gW * gs1 + (1.0 - gW) * gs2;
            giSlow += rippleAlpha * (gi - giSlow);
            compDb = g + ripple * (0.2 - 0.1 * g) * (gi - giSlow) + makeupDb; // AC part only; grows with the reduction
        }
        else
        {
            e2 = e1;
            gs1 = gs2 = 0.0;
        }

        expDb = 0.0;
        if (e1 > DetectorConstants::silenceLevel) // digital silence: the expander state holds
        {
            const double level = gainToDb (ex) - calibration.expander;
            double target;
            if (settings.gate)
            {
                if (gateOpen && level < gateCloseLevel)
                    gateOpen = false;
                else if (! gateOpen && level > gateOpenLevel)
                    gateOpen = true;
                target = gateOpen ? 0.0 : depthCap;
            }
            else
                target = std::min (depthCap, std::max (0.0, expSlope * (expOnset - level) + offset));
            if (target > depth)
                depth = std::min (target, depth + closeAlpha * (depthCap - depth));
            else
                depth += openAlpha * (target - depth);
        }
        if (expActive)
        {
            const double x = std::max (0.0, depth - offset);
            if (settings.gate)
                expDb = -std::min (x, floorDb);
            else
                expDb = -std::min (x, floorDb);
        }
        if (! std::isfinite (compDb) || ! std::isfinite (expDb))
        {
            reset();
            compDb = expDb = 0.0;
        }
        return compDb + expDb;
    }

    double makeup() const noexcept { return compActive ? makeupDb : 0.0; }
    double compressorGainDb() const noexcept { return compDb; }
    double expanderGainDb() const noexcept { return expDb; }

    void sanitise() noexcept
    {
        if (! std::isfinite (e1) || ! std::isfinite (e2) || ! std::isfinite (ex) || ! std::isfinite (gs1) || ! std::isfinite (gs2) || ! std::isfinite (depth) || e1 > 1.0e6)
            reset();
    }

private:
    double fs = 48000.0;
    DynamicsSettings settings;
    CompressorTable::Curve curve {};
    bool compActive = false, expActive = false;
    double makeupDb = 0.0, calibDb = 0.0;
    double d1 = 0.0, aA = 0.0, aR = 0.0, gA = 0.0, gR1 = 0.0, gR2 = 0.0, gW = 1.0, ripple = 0.0, rippleAlpha = 0.0, giSlow = 0.0, xA = 0.0, xR = 0.0;
    double e1 = 0.0, e2 = 0.0, ex = 0.0, gs1 = 0.0, gs2 = 0.0;
    DetectorCalibration calibration;
    double expOnset = -48.0, expSlope = 1.2, gateOpenLevel = -45.0, gateCloseLevel = -50.0;
    double offset = 6.0, floorDb = 0.0, depthCap = 0.0, openAlpha = 0.0, closeAlpha = 0.0;
    double depth = 0.0;
    bool gateOpen = true;
    double compDb = 0.0, expDb = 0.0;
};

} // namespace craft::consoleg::dsp
