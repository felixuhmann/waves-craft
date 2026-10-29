// Craft FET Limiter - Auto Makeup editor linkage (spec r1 section 8, D6). Host independent.
//
// While the user moves Input (editor gesture), Output follows so that the steady-state output level of a
// -18 dBFS sine stays constant, using the static law of spec 5:
//     level(i, o) = L + Gin(i) + GR(L + Gin(i)) + Gout(o),   L = -18 dBFS
// Output stays within its range (it "stops following" at 0 / -inf); Output moves never affect Input; host
// automation of Input does not call this.
#pragma once

#include <cmath>

#include "GainLaws.h"
#include "Parameters.h"

namespace craft::fet_limiter::dsp
{

inline constexpr double autoMakeupReferenceDbfs = -18.0;

// Steady-state gain (dB) from the input to the output for a sine of peak level levelDb, Output display 0.
inline double steadyStateGainDb (Revision rev, Ratio ratio, bool compOff, double inputDisplay, double levelDb)
{
    const double gin = inputGainDb (rev, inputDisplay);
    if (! std::isfinite (gin))
        return gin;
    const double gr = compOff ? 0.0 : staticGainChangeDb (rev, ratio, levelDb + gin);
    return gin + gr;
}

// New Output display after Input moved from inputStart to inputNow, with Output at outputStart when the gesture
// began. Returns outputStart when no compensation is needed or possible.
inline double autoMakeupOutput (Revision rev, Ratio ratio, bool compOff, double inputStart, double inputNow,
                                double outputStart)
{
    const double g0 = steadyStateGainDb (rev, ratio, compOff, inputStart, autoMakeupReferenceDbfs);
    const double g1 = steadyStateGainDb (rev, ratio, compOff, inputNow, autoMakeupReferenceDbfs);
    if (! std::isfinite (g0) || ! std::isfinite (g1) || outputStart <= param::Level::minValue)
        return outputStart;
    const Curve& out = specTables().gainOut[revIndex (rev)];
    const double required = out (outputStart) + (g0 - g1);
    if (std::abs (required - out (outputStart)) < 0.005)
        return outputStart;
    // the output law is flat at its top (rev A: -3 .. 0 display = 0 dB); keep the start value when it already
    // gives the required gain
    double o = out.inverse (required);
    if (required >= out.y.back())
        o = std::max (outputStart, out.inverse (out.y.back()));
    return param::Level::snap (std::clamp (o, param::Level::minValue + 0.1, param::Level::maxValue));
}

} // namespace craft::fet_limiter::dsp
