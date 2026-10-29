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

// Structure (spec 5 "peak-sensitive" detector, 6 "same linked detector", one gain element):
//   rectifier:  e1 = max(p, e1 * exp(-1/(tau1 fs))), p = max_c |sidechain_c| (instant attack, 0.3 ms decay);
//   level:      e2 one-pole on e1 in the linear domain: attack = the (auto / fast) attack constant, release short;
//               L = 20 log10 e2 - cal + delta  (cal: the mean reading of a steady 1 kHz sine, so L = sine peak dBFS)
//   gain:       g_i = curve(L) (curve = comp_static row incl. make-up); the output gain follows g_i instantly
//               downwards and recovers with the release constant in dB (exponential in dB, slower for the last few dB
//               of reduction - "program dependent", spec 5).
//   expander:   ex = one-pole (0.3 ms / 15 ms) of e1 -> level -> depth state D (attack = opening, release = closing).
// Peak sensitivity (noise vs sine) and the timing dependence of the static gain (fast attack / long release reduce a
// steady sine more) follow from the detector ripple held by the release stage; `delta` adds the residual measured
// offsets of spec 9.2 (fast attack, release 4 s) as a level shift.
struct DetectorConstants
{
    static constexpr double stage1TauMs = 0.04;
    static constexpr double attackScale = 2.6;            // x comp_model_fit attack constants (auto attack)
    static constexpr double fastAttackMs = 1.8;
    // level-stage release: a fixed fraction of the attack constant (keeps the detector's peak sensitivity the same
    // for all auto-attack settings), fixed for fast attack
    static constexpr double levelReleaseRatio = 0.035, levelReleaseFastMs = 5.0;
    static double levelReleaseMs (double T, double ratio, bool fast) noexcept
    {
        return fast ? levelReleaseFastMs : std::max (3.0, attackMs (T, ratio, false) * levelReleaseRatio);
    }
    static constexpr double attackBoost = 4.0, attackBoostSpanDb = 20.0; // attack speeds up close to the target
    static constexpr double releaseScale04 = 0.95;        // x comp_model_fit release constants (release 0.4 s)
    // release slows for small reduction: factor 1 + K exp(g / releaseTailDb), K = tail * (release / 0.4 s)^exponent
    static constexpr double releaseTail = 3.0, releaseTailFast = 6.0, releaseTailDb = 4.0;
    static constexpr double releaseTailExp = 0.5, releaseTailExpFast = 1.0;
    static double tailK (bool fast, double releaseS) noexcept
    {
        return fast ? releaseTailFast * std::pow (releaseS / 0.4, releaseTailExpFast)
                    : releaseTail * std::pow (releaseS / 0.4, releaseTailExp);
    }
    static constexpr double release12Extra = 1.3;
    static constexpr double fastDeltaDb = 3.3;            // residual static offsets (spec 9.2), level shift
    static constexpr double releaseDeltaDbPerDecade = 2.1;

    static constexpr double expSmoothAttackMs = 0.3, expSmoothReleaseMs = 15.0;
    static constexpr double expOpenMs = 3.0, expOpenFastMs = 0.3;
    // Closing (release): the depth state D moves towards its full depth (a * range for the expander, floor + offset
    // for the gate) exponentially, tau = factor * release, and stops at the level-dependent target. Small level
    // steps therefore settle quickly while a full closing takes ~release (fitted to the exp_static staircases: the
    // first step after silence and the closing step of the gate's down staircase).
    static constexpr double closeFactorExp = 1.05, closeFactorGate = 1.1;
    static constexpr double silenceLevel = 1.0e-6;        // -120 dBFS: below this the expander state holds

    // Release-control scaling relative to 0.4 s (spec 5 timing table, 63 % points: 87 / 126 / 345 ms for
    // 0.1 / 0.4 / 1.2 s; log-log extrapolation beyond) and the fast-attack release factor (66/126, 30/87 ms).
    static double releaseScale (double releaseS) noexcept
    {
        static const std::vector<double> x { std::log (0.1), std::log (0.4), std::log (1.2) };
        static const std::vector<double> y { std::log (87.0 / 126.0), 0.0, std::log (345.0 / 126.0 * release12Extra) };
        return std::exp (interpLinearExtrap (x, y, std::log (std::max (0.01, releaseS))));
    }
    static double fastReleaseFactor (double releaseS) noexcept
    {
        static const std::vector<double> x { std::log (0.1), std::log (0.4) };
        static const std::vector<double> y { std::log (30.0 / 87.0), std::log (66.0 / 126.0) };
        return std::exp (interpLinear (x, y, std::log (std::max (0.01, releaseS))));
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

    static double attackMs (double T, double ratio, bool fast) noexcept
    {
        return fast ? fastAttackMs : CompressorTiming::get().autoAttackMs (T, ratio) * attackScale;
    }
    static double gainReleaseMs (double T, double ratio, bool fast, double releaseS) noexcept
    {
        double r = CompressorTiming::get().releaseMs04 (T, ratio) * releaseScale04 * releaseScale (releaseS);
        return fast ? r * fastReleaseFactor (releaseS) : r;
    }
    static double deltaDb (bool fast, double releaseS) noexcept
    {
        return (fast ? fastDeltaDb : 0.0) + releaseDeltaDbPerDecade * std::log10 (std::max (0.01, releaseS) / 0.4);
    }

    // level stage (linear domain)
    static double levelStep (double e2, double e1, double aA, double aR, double boostAmount = attackBoost) noexcept
    {
        if (e1 > e2)
        {
            const double gapDb = gainToDb (e1) - gainToDb (std::max (e2, 1.0e-12));
            const double boost = 1.0 + boostAmount * std::max (0.0, 1.0 - gapDb / attackBoostSpanDb);
            return e2 + std::min (1.0, aA * boost) * (e1 - e2);
        }
        return e2 + aR * (e1 - e2);
    }
    // gain release stage (dB domain): g <= 0 reduction excluding make-up
    static double gainStep (double g, double gi, double aG, double tail) noexcept
    {
        if (gi <= g)
            return gi;
        return g + aG / (1.0 + tail * std::exp (g / releaseTailDb)) * (gi - g);
    }
};

// Mean reading (dB re. sine peak) of the level stage for a steady 1 kHz sine.
inline double simulateLevelReadingDb (double attackMs, double releaseMs, double boost = DetectorConstants::attackBoost,
                                      double fs = 48000.0)
{
    const double d1 = std::exp (-1000.0 / (DetectorConstants::stage1TauMs * fs));
    const double aA = DetectorConstants::alpha (attackMs, fs), aR = DetectorConstants::alpha (releaseMs, fs);
    double e1 = 0.0, e2 = 0.0, acc = 0.0;
    const int settle = int (fs * std::max (0.5, 10.0 * attackMs / 1000.0)), measure = int (fs * 0.1);
    for (int i = 0; i < settle + measure; ++i)
    {
        e1 = std::max (std::abs (std::sin (2.0 * pi * 1000.0 * i / fs)), e1 * d1);
        e2 = DetectorConstants::levelStep (e2, e1, aA, aR, boost);
        if (i >= settle)
            acc += gainToDb (e2);
    }
    return acc / measure;
}

// Reading offsets on the comp_static grid (threshold x ratio) for auto attack, and for fast attack.
class DetectorCalibration
{
public:
    static const DetectorCalibration& get()
    {
        static const DetectorCalibration c;
        return c;
    }

    double compOffsetDb (double T, double ratio, bool fast) const noexcept
    {
        if (fast)
            return fastOffset;
        const auto& ts = CompressorTable::get().thresholdGrid();
        const double tc = std::min (ts.back(), std::max (ts.front(), T));
        const size_t ti = segmentIndex (ts, tc);
        const double tt = (tc - ts[ti]) / (ts[ti + 1] - ts[ti]);
        const double u = std::max (us.front(), CompressorTable::uOf (ratio));
        const size_t ui = segmentIndex (us, u);
        const double tu = std::min (1.0, std::max (0.0, (u - us[ui]) / (us[ui + 1] - us[ui])));
        auto v = [&] (size_t a, size_t b) { return grid[a][b]; };
        const double g0 = v (ti, ui) + tu * (v (ti, ui + 1) - v (ti, ui));
        const double g1 = v (ti + 1, ui) + tu * (v (ti + 1, ui + 1) - v (ti + 1, ui));
        return g0 + tt * (g1 - g0);
    }
    double expanderOffsetDb() const noexcept { return expOffset; }

private:
    DetectorCalibration()
    {
        const auto& tab = CompressorTable::get();
        for (double R : tab.ratioGrid())
            us.push_back (CompressorTable::uOf (R));
        for (double T : tab.thresholdGrid())
        {
            std::vector<double> row;
            for (double R : tab.ratioGrid())
                row.push_back (simulateLevelReadingDb (DetectorConstants::attackMs (T, R, false), DetectorConstants::levelReleaseMs (T, R, false)));
            grid.push_back (row);
        }
        fastOffset = simulateLevelReadingDb (DetectorConstants::fastAttackMs, DetectorConstants::levelReleaseFastMs, 0.0);
        // the expander smoother has the same structure (plain one-pole, no attack boost difference matters little)
        expOffset = simulateLevelReadingDb (DetectorConstants::expSmoothAttackMs, DetectorConstants::expSmoothReleaseMs);
    }

    std::vector<double> us;
    std::vector<std::vector<double>> grid;
    double fastOffset = 0.0, expOffset = 0.0;
};

class DynamicsSection
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        d1 = std::exp (-1000.0 / (DetectorConstants::stage1TauMs * fs));
        xA = DetectorConstants::alpha (DetectorConstants::expSmoothAttackMs, fs);
        xR = DetectorConstants::alpha (DetectorConstants::expSmoothReleaseMs, fs);
        (void) DetectorCalibration::get();
        reset();
        setSettings (settings, true);
    }

    void reset() noexcept
    {
        e1 = e2 = ex = gs = 0.0;
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
            calibDb = DetectorCalibration::get().compOffsetDb (s.compThreshold, s.compRatio, s.compFast)
                      - DetectorConstants::deltaDb (s.compFast, s.compRelease);
            aA = DetectorConstants::alpha (DetectorConstants::attackMs (s.compThreshold, s.compRatio, s.compFast), fs);
            aR = DetectorConstants::alpha (DetectorConstants::levelReleaseMs (s.compThreshold, s.compRatio, s.compFast), fs);
            tail = DetectorConstants::tailK (s.compFast, s.compRelease);
            aG = DetectorConstants::alpha (
                DetectorConstants::gainReleaseMs (s.compThreshold, s.compRatio, s.compFast, s.compRelease), fs);
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
        const double rel = std::max (0.001, s.expRelease);
        closeAlpha = DetectorConstants::alpha (
            1000.0 * rel * (s.gate ? DetectorConstants::closeFactorGate : DetectorConstants::closeFactorExp), fs);
    }

    // One sample: linked detector input peak (max over channels of |sidechain|) -> total dynamics gain in dB.
    double process (double peak) noexcept
    {
        e1 = std::max (peak, e1 * d1);
        ex = DetectorConstants::levelStep (ex, e1, xA, xR);

        compDb = 0.0;
        if (compActive)
        {
            e2 = DetectorConstants::levelStep (e2, e1, aA, aR, settings.compFast ? 0.0 : DetectorConstants::attackBoost);
            const double gi = CompressorTable::evaluate (curve, gainToDb (e2) - calibDb) - makeupDb;
            gs = DetectorConstants::gainStep (gs, gi, aG, tail);
            compDb = gs + makeupDb;
        }
        else
        {
            e2 = e1;
            gs = 0.0;
        }

        expDb = 0.0;
        if (e1 > DetectorConstants::silenceLevel) // digital silence: the expander state holds
        {
            const double level = gainToDb (ex) - DetectorCalibration::get().expanderOffsetDb();
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
        if (! std::isfinite (e1) || ! std::isfinite (e2) || ! std::isfinite (ex) || ! std::isfinite (gs) || ! std::isfinite (depth) || e1 > 1.0e6)
            reset();
    }

private:
    double fs = 48000.0;
    DynamicsSettings settings;
    CompressorTable::Curve curve {};
    bool compActive = false, expActive = false;
    double makeupDb = 0.0, calibDb = 0.0;
    double d1 = 0.0, aA = 0.0, aR = 0.0, aG = 0.0, tail = 1.0, xA = 0.0, xR = 0.0;
    double e1 = 0.0, e2 = 0.0, ex = 0.0, gs = 0.0;
    double expOnset = -48.0, expSlope = 1.2, gateOpenLevel = -45.0, gateCloseLevel = -50.0;
    double offset = 6.0, floorDb = 0.0, depthCap = 0.0, openAlpha = 0.0, closeAlpha = 0.0;
    double depth = 0.0;
    bool gateOpen = true;
    double compDb = 0.0, expDb = 0.0;
};

} // namespace craft::consoleg::dsp
