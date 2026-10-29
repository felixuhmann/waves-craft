// Craft Level Rider - static law (spec r1 section 5), level detector and gain ballistics (section 6),
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
// Section 6 constants (Slow / Fast). The spec's structure is non-binding and its constants are a starting point
// (Slow 71 ms / 50 ms / 0.94 s / 0.45 s / 52 ms, Fast 2.1 ms / 450 ms / 1.30 s / 0.79 s / 15 ms). Because this
// detector reads the analytic (ripple-free) power instead of the plain block mean square, the constants were
// re-fitted to the normative tables T2 / T3 (least squares, see IMPLEMENTATION_NOTES.md).
struct Ballistics
{
    double attack, hold, release, up, down;

    static Ballistics slow() noexcept { return { 0.0512, 0.0780, 1.004, 0.492, 0.0430 }; }
    static Ballistics fast() noexcept { return { 0.00137, 0.482, 1.502, 0.751, 0.0143 }; }
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
            state += (blockPower - state) * onePoleCoeff (b.release, dt);
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
// Section 7 activity gate. Inputs: the weighted block power. Decides Ride / Idle.
class ActivityGate
{
public:
    static constexpr double absoluteSmoothing = 0.100;  // s, one-pole on power (7.1)
    static constexpr double hysteresisDb = 4.0;         // theta_off = theta_on - 4 dB (7.1)
    static constexpr double fastSmoothing = 0.010;      // s, short level used for the relative test (7.2)
    static constexpr double relativeDropDb = 24.0;      // non-vocal when more than this below the reference (7.2)
    static constexpr double referenceDecayDbPerSecond = 1.5; // running-maximum decay (7.2)
    static constexpr double spillHysteresisDb = 3.0;    // Live: off below Spill - 3 dB (assumption, 7.4)
    static constexpr double floorDb = -200.0;

    void reset() noexcept
    {
        smoothPower = fastPower = 0.0;
        referenceDb = floorDb;
        absoluteOn = spillOn = false;
        riding = false;
    }

    // thetaOnDb = -62 - Vocal Sensitivity. spillDb only used when spillEnabled.
    bool update (double blockPower, double thetaOnDb, bool spillEnabled, double spillDb, double dt) noexcept
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

        // 7.2 relative threshold: running maximum of the 1 ms block level, decaying while not exceeded; the
        // short-term (10 ms) level must not fall more than 24 dB below it
        referenceDb = std::max (referenceDb - referenceDecayDbPerSecond * dt, powerToDb (blockPower));
        referenceDb = std::max (referenceDb, floorDb);
        const bool relativeOk = fastDb >= referenceDb - relativeDropDb;

        // 7.4 spill gate (Live products)
        if (! spillEnabled)
            spillOn = true;
        else if (smoothDb > spillDb)
            spillOn = true;
        else if (smoothDb < spillDb - spillHysteresisDb)
            spillOn = false;

        riding = absoluteOn && relativeOk && spillOn;
        return riding;
    }

    bool isRiding() const noexcept { return riding; }
    double smoothLevelDb() const noexcept { return powerToDb (smoothPower); }
    double fastLevelDb() const noexcept { return powerToDb (fastPower); }
    double referenceLevelDb() const noexcept { return referenceDb; }

private:
    double smoothPower = 0.0, fastPower = 0.0, referenceDb = floorDb;
    bool absoluteOn = false, spillOn = false, riding = false;
};

} // namespace craft::levelrider::dsp
