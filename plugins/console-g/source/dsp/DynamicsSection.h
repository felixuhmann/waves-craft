// Craft Console G - run-time dynamics: linked detector, compressor gain computer and expander / gate state.
#pragma once

#include <array>
#include <cmath>

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

// Detector (spec 5 "peak-sensitive", 6 "same linked detector"):
//   stage 1 (shared): e1 = max(|x|, e1 * exp(-1/(tau1 fs))) of the linked peak max_c |x_c| (instant attack);
//   compressor:       e2 += alpha (e1 - e2), alpha from the attack / release constants;
//   expander:         ex += alpha (e1 - ex), short fixed constants.
// Levels are calibrated so that a steady 1 kHz sine of peak level L dBFS reads L at the DEFAULT compressor timing
// (auto attack, release 0.4 s) - the condition of tables/comp_static.csv. Other timings read higher or lower, as
// the reference does (spec 5: fast attack and long release give more reduction on the same sine).
struct DetectorConstants
{
    static constexpr double stage1TauMs = 2.5;
    static constexpr double fastAttackMs = 1.9;          // comp_model_fit fast rows
    static constexpr double expSmoothAttackMs = 0.3, expSmoothReleaseMs = 15.0;
    static constexpr double expOpenMs = 3.0, expOpenFastMs = 0.3;
    static constexpr double expCloseFactor = 1.07;        // closing time constant / release setting
    static constexpr double gateCloseFactor = 1.2;
    static constexpr double silenceLevel = 1.0e-6;        // -120 dBFS: below this the expander state holds

    // Release scaling vs the 0.4 s constant (auto attack) and the fast-attack release factor.
    static double releaseScale (double releaseS) noexcept
    {
        static const std::vector<double> x { std::log (0.1), std::log (0.4), std::log (1.2), std::log (4.0) };
        static const std::vector<double> y { std::log (0.692), 0.0, std::log (2.65), std::log (3.6) };
        return std::exp (interpLinearExtrap (x, y, std::log (std::max (0.01, releaseS))));
    }
    static double fastReleaseFactor (double releaseS) noexcept
    {
        static const std::vector<double> x { std::log (0.1), std::log (0.4) };
        static const std::vector<double> y { std::log (0.335), std::log (0.532) };
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
};

// Mean detector reading (dB re. sine peak) of a steady 1 kHz sine, simulated (rate-independent constants).
inline double simulateDetectorDb (double attackMs, double releaseMs, double fs = 48000.0, double freq = 1000.0)
{
    const double d1 = std::exp (-1000.0 / (DetectorConstants::stage1TauMs * fs));
    const double aA = DetectorConstants::alpha (attackMs, fs), aR = DetectorConstants::alpha (releaseMs, fs);
    double e1 = 0.0, e2 = 0.0, acc = 0.0;
    const int settle = int (fs * std::max (1.5, 12.0 * std::max (attackMs, releaseMs) / 1000.0));
    const int measure = int (fs * 0.2);
    for (int i = 0; i < settle + measure; ++i)
    {
        const double x = std::abs (std::sin (2.0 * pi * freq * i / fs));
        e1 = std::max (x, e1 * d1);
        e2 += (e1 > e2 ? aA : aR) * (e1 - e2);
        if (i >= settle)
            acc += gainToDb (e2);
    }
    return acc / measure;
}

// Calibration offsets of the compressor detector on the comp_static grid (threshold x ratio), default timing.
class DetectorCalibration
{
public:
    static const DetectorCalibration& get()
    {
        static const DetectorCalibration c;
        return c;
    }

    double compOffsetDb (double T, double ratio) const noexcept
    {
        const auto& tab = CompressorTable::get();
        const auto& ts = tab.thresholdGrid();
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
        const auto& timing = CompressorTiming::get();
        for (double R : tab.ratioGrid())
            us.push_back (CompressorTable::uOf (R));
        for (double T : tab.thresholdGrid())
        {
            std::vector<double> row;
            for (double R : tab.ratioGrid())
                row.push_back (simulateDetectorDb (timing.autoAttackMs (T, R), timing.releaseMs04 (T, R)));
            grid.push_back (row);
        }
        expOffset = simulateDetectorDb (DetectorConstants::expSmoothAttackMs, DetectorConstants::expSmoothReleaseMs);
    }

    std::vector<double> us;
    std::vector<std::vector<double>> grid;
    double expOffset = 0.0;
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
        e1 = e2 = ex = 0.0;
        depth = 0.0;
        gateOpen = true;
        lastGainDb = 0.0;
    }

    void setSettings (const DynamicsSettings& s, bool force = false) noexcept
    {
        const bool compChanged = force || s.compThreshold != settings.compThreshold || s.compRatio != settings.compRatio
                                 || s.compFast != settings.compFast || s.compRelease != settings.compRelease;
        settings = s;
        if (compChanged)
        {
            const auto& timing = CompressorTiming::get();
            curve = CompressorTable::get().curve (s.compThreshold, s.compRatio);
            compActive = CompressorTable::uOf (s.compRatio) > 0.0;
            makeupDb = curve[0];
            calibDb = DetectorCalibration::get().compOffsetDb (s.compThreshold, s.compRatio);
            const double rel04 = timing.releaseMs04 (s.compThreshold, s.compRatio);
            double attackMs = timing.autoAttackMs (s.compThreshold, s.compRatio);
            double releaseMs = rel04 * DetectorConstants::releaseScale (s.compRelease);
            if (s.compFast)
            {
                attackMs = DetectorConstants::fastAttackMs;
                releaseMs *= DetectorConstants::fastReleaseFactor (s.compRelease);
            }
            aA = DetectorConstants::alpha (attackMs, fs);
            aR = DetectorConstants::alpha (releaseMs, fs);
        }
        const auto& em = ExpanderModel::get();
        expActive = s.expRange > 0.0;
        expOnset = em.onsetDb (s.expThreshold);
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
            depthCap = floorDb + offset + 3.0;
        }
        openAlpha = DetectorConstants::alpha (s.expFast ? DetectorConstants::expOpenFastMs : DetectorConstants::expOpenMs, fs);
        closeAlpha = DetectorConstants::alpha (
            1000.0 * s.expRelease * (s.gate ? DetectorConstants::gateCloseFactor : DetectorConstants::expCloseFactor), fs);
    }

    // One sample: linked detector input peak (max over channels of |sidechain|) -> total dynamics gain in dB.
    double process (double peak) noexcept
    {
        e1 = std::max (peak, e1 * d1);
        e2 += (e1 > e2 ? aA : aR) * (e1 - e2);
        ex += (e1 > ex ? xA : xR) * (e1 - ex);

        compDb = 0.0;
        if (compActive)
            compDb = CompressorTable::evaluate (curve, gainToDb (e2) - calibDb);

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
            depth += (target > depth ? closeAlpha : openAlpha) * (target - depth);
        }
        if (expActive)
        {
            const double x = std::max (0.0, depth - offset);
            if (settings.gate)
                expDb = -std::min (x, floorDb);
            else if (floorDb > 0.0 && x > 0.0)
                expDb = -softMin (x, floorDb);
        }
        if (! std::isfinite (compDb) || ! std::isfinite (expDb))
        {
            reset();
            compDb = expDb = 0.0;
        }
        lastGainDb = compDb + expDb;
        return lastGainDb;
    }

    double makeup() const noexcept { return compActive ? makeupDb : 0.0; }
    double compressorGainDb() const noexcept { return compDb; }
    double expanderGainDb() const noexcept { return expDb; }

    void sanitise() noexcept
    {
        if (! std::isfinite (e1) || ! std::isfinite (e2) || ! std::isfinite (ex) || ! std::isfinite (depth) || e1 > 1.0e6)
            reset();
    }

private:
    // smooth minimum (p-norm), so the expansion slope bends into the floor (spec 6 staircase shape)
    static double softMin (double x, double f) noexcept
    {
        constexpr double p = 6.0;
        return x * f / std::pow (std::pow (x, p) + std::pow (f, p), 1.0 / p);
    }

    double fs = 48000.0;
    DynamicsSettings settings;
    CompressorTable::Curve curve {};
    bool compActive = false, expActive = false;
    double makeupDb = 0.0, calibDb = 0.0;
    double d1 = 0.0, aA = 0.0, aR = 0.0, xA = 0.0, xR = 0.0;
    double e1 = 0.0, e2 = 0.0, ex = 0.0;
    double expOnset = -48.0, expSlope = 1.2, gateOpenLevel = -45.0, gateCloseLevel = -50.0;
    double offset = 6.0, floorDb = 0.0, depthCap = 0.0, openAlpha = 0.0, closeAlpha = 0.0;
    double depth = 0.0;
    bool gateOpen = true;
    double compDb = 0.0, expDb = 0.0, lastGainDb = 0.0;
};

} // namespace craft::consoleg::dsp
