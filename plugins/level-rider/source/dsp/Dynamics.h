// Craft Level Rider - static law (spec r2 section 5), level detector and gain ballistics (section 6),
// activity gate (section 7.1, 7.2, 7.4) and idle movement (section 7.3).
// Host-independent: no JUCE dependency. All state is updated once per 1 ms detector block.
#pragma once

#include <algorithm>
#include <cmath>

#include "Parameters.h"

namespace craft::levelrider::dsp
{

inline double powerToDb (double p) noexcept { return 10.0 * std::log10 (std::max (p, 1.0e-20)); }
inline double dbToPower (double db) noexcept { return std::pow (10.0, db / 10.0); }

// One-pole coefficient for a time constant tau (s) updated every dt seconds.
inline double onePoleCoeff (double tau, double dt) noexcept { return tau > 0.0 ? 1.0 - std::exp (-dt / tau) : 1.0; }

// ---------------------------------------------------------------------------------------------
// Section 5 (normative): steady-state ride gain for detector level L (dB), target T (dB) and range.
struct StaticLaw
{
    static constexpr double a = 0.5162, b = 0.7252;

    static double gainDb (double levelDb, double targetDb, double minDb, double maxDb) noexcept
    {
        const double u = levelDb - targetDb;
        const double w = 0.5 * (maxDb - minDb);
        const double c = 0.5 * (maxDb + minDb);
        const double g = a * std::pow (10.0, -w / 20.0) + b * std::pow (10.0, -(u + c) / 20.0);
        return clampTo (c + 20.0 * std::log10 (g), minDb, maxDb);
    }
};

// ---------------------------------------------------------------------------------------------
// Section 6 constants of one Attack mode. The spec's structure is non-binding and its constants a starting point
// (Slow 71 ms / 50 ms / 0.94 s / 0.45 s / 52 ms, Fast 2.1 ms / 450 ms / 1.30 s / 0.79 s / 15 ms). This detector
// reads the analytic power and adds a "near" release (below); all constants are fitted to the normative tables
// T2 / T3 / T4 / T7 (see IMPLEMENTATION_NOTES.md).
struct Ballistics
{
    double attack, hold, release, up, down;
    // Program-dependent release: while the block power is within nearDb below the state (a moderate level change)
    // the state follows with nearRelease; further below (a pause or a large drop) it releases with `release`.
    double nearDb, nearRelease;
    // Section 7.2 relative threshold (r2) for this mode.
    double relativeDropDb = 24.0;     // Idle when the short-term level falls this far below Lref
    double referenceDecay = 1.5;      // Lref decay (dB/s) while not exceeded, also while Idle
    double reentryDb = 20.0;          // ride again once the short-term level is back within this of Lref
    double seedOffsetDb = 0.0;        // after a relative Idle the detector restarts from Lref + this
    int referenceSource = 0;          // Lref = running max of: 0 the 1 ms block level, 1 the 10 ms level, 2 the 0.1 s level
    // Blocks more than farDb below the state release with farRelease (the wait after a large drop grows with its size).
    double farDb = 0.0, farRelease = 1.0;
};

// Complete set of fitted constants (both Attack modes).
struct Tuning
{
    Ballistics slow { 0.059575, 0.097442, 1.6584, 0.44703, 0.048725, 8.2557, 0.38748, 24.31, 1.3677, 17.601, 5.1977, 2 };
    Ballistics fast { 0.0012102, 0.5566, 1.667, 0.77546, 0.0098301, 8.6176, 0.28828, 23.436, 1.431, 21.212, 2.0463, 0 };
};

// Level detector on the weighted mean power of 1 ms blocks: attack when the block power exceeds the state,
// otherwise hold, then release (power one-pole). L = 10 log10(state).
class LevelDetector
{
public:
    void reset (double initialPower = 0.0) noexcept
    {
        state = initialPower;
        holdLeft = 0.0;
    }

    void update (double blockPower, const Ballistics& b, double dt) noexcept
    {
        if (blockPower > state)
        {
            state += (blockPower - state) * onePoleCoeff (b.attack, dt);
            holdLeft = b.hold;
        }
        else if (holdLeft > 0.0)
            holdLeft -= dt;
        else
        {
            const double below = powerToDb (state) - powerToDb (blockPower);
            const double tau = below <= b.nearDb ? b.nearRelease : (b.farDb > 0.0 && below > b.farDb ? b.farRelease : b.release);
            state += (blockPower - state) * onePoleCoeff (tau, dt);
        }
        if (state < 1.0e-24)
            state = 0.0;
    }

    double levelDb() const noexcept { return powerToDb (state); }
    double power() const noexcept { return state; }

private:
    double state = 0.0, holdLeft = 0.0;
};

// ---------------------------------------------------------------------------------------------
// Section 7.3 idle movement speed (dB/s) as a function of the current fader position. Fitted to table T4:
// about 6.5 dB/s around 0 dB, slower (3.5 .. 5 dB/s) at low fader positions; the same in both directions.
inline double idleSpeedDbPerSecond (double gDb) noexcept
{
    const double g = std::min (gDb, 5.87);
    return std::max (3.0, 6.35 + 0.115 * g - 0.0098 * g * g);
}

// ---------------------------------------------------------------------------------------------
// Section 7 activity gate. Inputs: the weighted block power. Decides Ride / Idle and reports, when riding
// resumes, whether the Idle was caused by the relative threshold (7.2) or by the absolute threshold / Spill.
class ActivityGate
{
public:
    static constexpr double absoluteSmoothing = 0.100;  // s, one-pole on power (7.1)
    static constexpr double hysteresisDb = 4.0;         // theta_off = theta_on - 4 dB (7.1)
    static constexpr double fastSmoothing = 0.010;      // s, short level used for the relative test (7.2)
    static constexpr double spillHysteresisDb = 3.0;    // Live: off below Spill - 3 dB (assumption, 7.4)
    static constexpr double floorDb = -200.0;

    enum class Resume { none, afterRelative, afterAbsolute };

    void reset() noexcept
    {
        smoothPower = fastPower = 0.0;
        referenceDb = floorDb;
        absoluteOn = spillOn = false;
        relativeIdle = relativeCaused = false;
        riding = false;
        resume = Resume::none;
    }

    // thetaOnDb = -62 - Vocal Sensitivity. spillDb only used when spillEnabled.
    bool update (double blockPower, double thetaOnDb, bool spillEnabled, double spillDb, const Ballistics& t,
                 double dt) noexcept
    {
        smoothPower += (blockPower - smoothPower) * onePoleCoeff (absoluteSmoothing, dt);
        fastPower += (blockPower - fastPower) * onePoleCoeff (fastSmoothing, dt);
        if (smoothPower < 1.0e-24) smoothPower = 0.0;
        if (fastPower < 1.0e-24) fastPower = 0.0;

        const double smoothDb = powerToDb (smoothPower);
        const double fastDb = powerToDb (fastPower);

        // 7.1 absolute threshold with 4 dB hysteresis
        if (smoothDb > thetaOnDb)
            absoluteOn = true;
        else if (smoothDb < thetaOnDb - hysteresisDb)
            absoluteOn = false;

        // 7.2 relative threshold: Lref = running maximum of the block level, decaying while not exceeded (also
        // while Idle). Idle when the short-term level falls more than relativeDropDb below Lref; ride again once
        // it is back within reentryDb (r2).
        const double source = t.referenceSource == 2 ? smoothDb : t.referenceSource == 1 ? fastDb : powerToDb (blockPower);
        referenceDb = std::max (std::max (referenceDb - t.referenceDecay * dt, source), floorDb);
        if (! relativeIdle && fastDb < referenceDb - t.relativeDropDb)
            relativeIdle = true;
        else if (relativeIdle && fastDb >= referenceDb - t.reentryDb)
            relativeIdle = false;

        // 7.4 spill gate (Live products)
        if (! spillEnabled)
            spillOn = true;
        else if (smoothDb > spillDb)
            spillOn = true;
        else if (smoothDb < spillDb - spillHysteresisDb)
            spillOn = false;

        const bool wasRiding = riding;
        riding = absoluteOn && ! relativeIdle && spillOn;
        if (! riding && relativeIdle)
            relativeCaused = true;          // this Idle period involved the relative threshold
        resume = Resume::none;
        if (riding && ! wasRiding)
        {
            resume = relativeCaused ? Resume::afterRelative : Resume::afterAbsolute;
            relativeCaused = false;
        }
        if (! riding && ! relativeIdle && ! absoluteOn)
            relativeCaused = false;         // silence after the drop: the absolute threshold rules (T4)
        return riding;
    }

    bool isRiding() const noexcept { return riding; }
    Resume resumed() const noexcept { return resume; }
    double smoothLevelDb() const noexcept { return powerToDb (smoothPower); }
    double fastLevelDb() const noexcept { return powerToDb (fastPower); }
    double fastPowerValue() const noexcept { return fastPower; }
    double referenceLevelDb() const noexcept { return referenceDb; }

private:
    double smoothPower = 0.0, fastPower = 0.0, referenceDb = floorDb;
    bool absoluteOn = false, spillOn = false, relativeIdle = false, relativeCaused = false, riding = false;
    Resume resume = Resume::none;
};

} // namespace craft::levelrider::dsp
