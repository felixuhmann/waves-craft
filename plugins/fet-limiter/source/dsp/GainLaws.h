// Craft FET Limiter - gain staging (spec 4) and static compression law (spec 5). Host independent.
#pragma once

#include <cmath>
#include <limits>

#include "Tables.h"

namespace craft::fet_limiter::dsp
{

inline constexpr double levelDisplayMin = -60.0; // display -60 == "-inf" (host value 0)

inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) noexcept { return g > 0.0 ? 20.0 * std::log10 (g) : -std::numeric_limits<double>::infinity(); }

// Small-signal gain of the Input control (linear). Display -inf (r = 0) is silence. Between the -inf row and the
// first finite row the amplitude is interpolated linearly (assumption G1), elsewhere dB are interpolated linearly
// between rows (spec 4: "Linear interpolation in display dB between rows").
inline double inputGain (Revision rev, double display) noexcept
{
    if (display <= levelDisplayMin)
        return 0.0;
    const Curve& c = specTables().gainIn[revIndex (rev)];
    const size_t n = c.x.size();
    if (display >= c.x[n - 1])
        return dbToGain (c.y[n - 1]);
    size_t i = 0;
    while (i + 2 < n && display >= c.x[i + 1])
        ++i;
    const double t = std::clamp ((display - c.x[i]) / (c.x[i + 1] - c.x[i]), 0.0, 1.0);
    if (! std::isfinite (c.y[i])) // segment starting at -inf: linear in amplitude
        return t * dbToGain (c.y[i + 1]);
    return dbToGain (c.y[i] + t * (c.y[i + 1] - c.y[i]));
}

inline double inputGainDb (Revision rev, double display) noexcept { return gainToDb (inputGain (rev, display)); }

// Output control: gain relative to Output display 0 (linear). Display -inf (r = 0) is silence.
inline double outputGain (Revision rev, double display) noexcept
{
    if (display <= levelDisplayMin)
        return 0.0;
    return dbToGain (specTables().gainOut[revIndex (rev)] (display));
}

inline double outputGainDb (Revision rev, double display) noexcept { return gainToDb (outputGain (rev, display)); }

// Small-signal makeup of the All ratio (spec 4): +1.2 dB rev A (contained in its table), +0.6 dB rev B.
inline double allMakeupDb (Revision rev) noexcept
{
    if (rev == Revision::A)
        return specTables().staticGr[0][4].y.front();
    return 0.6;
}

// Static law as a positive gain-cell reduction R(x) >= 0 (dB) at internal sine-peak level x (dB). The steady-state
// gain change is  -R(x) (+ allMakeupDb for the All ratio), i.e. the table value GR(x).
class StaticLaw
{
public:
    void set (Revision rev, Ratio ratio)
    {
        const Curve& c = specTables().staticGr[revIndex (rev)][int (ratio)];
        const double top = (ratio == Ratio::All && rev == Revision::A) ? c.y.front() : 0.0;
        curve.x = c.x;
        curve.y.resize (c.y.size());
        for (size_t i = 0; i < c.y.size(); ++i)
            curve.y[i] = std::max (0.0, top - c.y[i]);
    }

    double reduction (double xDb) const noexcept { return curve (xDb); }

private:
    Curve curve;
};

// Table value GR(x) (dB, <= 0 except All makeup) as the plug-in realises it in steady state.
inline double staticGainChangeDb (Revision rev, Ratio ratio, double xDb)
{
    StaticLaw law;
    law.set (rev, ratio);
    return -law.reduction (xDb) + (ratio == Ratio::All ? allMakeupDb (rev) : 0.0);
}

} // namespace craft::fet_limiter::dsp
