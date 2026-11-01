// Craft Flanger - Rate <-> Depth link (spec r1 section 5.5 / review decision 11.2).
// Control coupling only: while Link is on, the product Rate x Depth (constant peak pitch deviation) is held
// constant; the DSP always uses the current Rate and Depth values.
#pragma once

#include "Parameters.h"

namespace craft::flanger::dsp
{

class RateDepthLink
{
public:
    // Capture the product to hold (called when Link is switched on or a state is loaded).
    void capture (double rateHz, double depthPct) noexcept { product = rateHz * depthPct; }
    double getProduct() const noexcept { return product; }

    // Rate changed by the user or the host: the Depth that keeps the product, clamped to 0 ... 100 %.
    // Returns a negative value when no coupling applies (product 0: nothing to hold).
    double depthForRate (double rateHz) const noexcept
    {
        if (product <= 0.0)
            return -1.0;
        if (rateHz <= 0.0)
            return param::Depth::maxValue;
        return clampTo (product / rateHz, param::Depth::minValue, param::Depth::maxValue);
    }

    // Depth changed: the Rate that keeps the product, clamped to the Rate range (negative = no coupling).
    double rateForDepth (double depthPct) const noexcept
    {
        if (product <= 0.0)
            return -1.0;
        if (depthPct <= 0.0)
            return param::Rate::maxValue;
        return clampTo (product / depthPct, param::Rate::minValue, param::Rate::maxValue);
    }

private:
    double product = 0.0;
};

} // namespace craft::flanger::dsp
