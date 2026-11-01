// Craft Colour Delay - parameter smoothing (spec r1 section 8).
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::cdl
{

// First-order exponential smoother. Snaps to the exact target once within `epsilon`, so settled values are
// bit-identical to their targets (keeps the [exact] parts of the signal path exact once a control has settled).
class OnePoleSmoother
{
public:
    void setTimeConstant (double seconds, double sampleRate) noexcept
    {
        coeff = seconds > 0.0 ? std::exp (-1.0 / (seconds * sampleRate)) : 0.0;
    }
    void setEpsilon (double e) noexcept { epsilon = e; }
    void setTarget (double t) noexcept { target = t; }
    void snapToTarget() noexcept { current = target; }
    void reset (double v) noexcept { current = target = v; }
    double getTarget() const noexcept { return target; }
    double getCurrent() const noexcept { return current; }
    bool isSettled() const noexcept { return current == target; }

    double next() noexcept
    {
        if (current != target)
        {
            current = target + coeff * (current - target);
            if (std::abs (current - target) <= epsilon)
                current = target;
        }
        return current;
    }

private:
    double coeff = 0.0, current = 0.0, target = 0.0, epsilon = 1.0e-9;
};

// Linear ramp (used for crossfades between delay taps).
class LinearRamp
{
public:
    void setRampTime (double seconds, double sampleRate) noexcept { step = 1.0 / std::max (1.0, seconds * sampleRate); }
    void setTarget (double t) noexcept { target = t; }
    void reset (double v) noexcept { current = target = v; }
    double getCurrent() const noexcept { return current; }
    double getTarget() const noexcept { return target; }
    bool isSettled() const noexcept { return current == target; }

    double next() noexcept
    {
        if (current < target)
            current = std::min (target, current + step);
        else if (current > target)
            current = std::max (target, current - step);
        return current;
    }

private:
    double step = 0.001, current = 0.0, target = 0.0;
};

inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) noexcept { return g > 0.0 ? 20.0 * std::log10 (g) : -400.0; }

// Safety soft clip, transparent up to +24 dBFS (|x| <= 16) and bounded by +30 dBFS (|x| < 32). Only signals far
// beyond any normal level reach it (e.g. Feedback 100 % with sustained full-scale input, which otherwise grows
// without bound); everything the spec measures stays exactly linear.
inline double safetyClip (double x) noexcept
{
    constexpr double knee = 16.0;
    const double a = std::abs (x);
    if (a <= knee)
        return x;
    const double y = knee + knee * std::tanh ((a - knee) / knee);
    return x < 0.0 ? -y : y;
}

} // namespace craft::cdl
