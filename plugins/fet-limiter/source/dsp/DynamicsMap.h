// Craft FET Limiter - gain-cell parameters per Revision / Ratio / Attack / Release (spec 6). Host independent.
// PLACEHOLDER - replaced by the fitted map.
#pragma once

#include "Dynamics.h"
#include "Tables.h"

namespace craft::fet_limiter::dsp
{

inline DynParams dynamicsFor (Revision, Ratio, double attack, double release)
{
    DynParams p;
    p.tauAttackMs = 1.55 * std::pow (0.17 / 1.55, (attack - 1.0) / 6.0);
    p.tau1Ms = p.tau2Ms = p.tau3Ms = 800.0 * std::pow (0.1, (release - 1.0) / 6.0);
    return p;
}

} // namespace craft::fet_limiter::dsp
