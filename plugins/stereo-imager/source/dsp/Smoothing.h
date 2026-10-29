// Craft Audio stereo imager - parameter smoothing (spec r1 section 6).
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::dsp
{

// First-order exponential smoother; snaps to the exact target once within `epsilon`
// so that settled values are bit-identical to their targets.
class OnePoleSmoother
{
public:
    void setTimeConstant (double seconds, double sampleRate) noexcept
    {
        coeff = std::exp (-1.0 / (seconds * sampleRate));
    }
    void setEpsilon (double e) noexcept { epsilon = e; }
    void setTarget (double t) noexcept { target = t; }
    void snapToTarget() noexcept { current = target; }
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

// Second-order (two cascaded identical one-poles) smoother: S-shaped step response.
// 10-90 % rise time = 3.3577 tau.
class SecondOrderSmoother
{
public:
    static constexpr double riseTimeToTau = 1.0 / 3.3577;

    void setRiseTime (double seconds10to90, double sampleRate) noexcept
    {
        coeff = std::exp (-1.0 / (seconds10to90 * riseTimeToTau * sampleRate));
    }
    void setEpsilon (double e) noexcept { epsilon = e; }
    void setTarget (double t) noexcept { target = t; }
    void snapToTarget() noexcept { s1 = s2 = target; }
    double getTarget() const noexcept { return target; }
    double getCurrent() const noexcept { return s2; }
    bool isSettled() const noexcept { return s1 == target && s2 == target; }

    double next() noexcept
    {
        if (! isSettled())
        {
            s1 = target + coeff * (s1 - target);
            s2 = s1 + coeff * (s2 - s1);
            if (std::abs (s1 - target) <= epsilon && std::abs (s2 - target) <= epsilon)
                s1 = s2 = target;
        }
        return s2;
    }

private:
    double coeff = 0.0, s1 = 0.0, s2 = 0.0, target = 0.0, epsilon = 1.0e-7;
};

// Linear ramp between 0 and 1 (crossfades).
class LinearRamp
{
public:
    void setRampTime (double seconds, double sampleRate) noexcept
    {
        step = 1.0 / std::max (1.0, seconds * sampleRate);
    }
    void setTarget (double t) noexcept { target = t; }
    void snapToTarget() noexcept { current = target; }
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

} // namespace craft::dsp
