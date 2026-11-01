// Craft Flanger - LFO waveforms and stereo phase offset (spec r1 section 5).
#pragma once

#include <cmath>

namespace craft::flanger::dsp
{

inline constexpr double twoPi = 6.283185307179586476925286766559;

// Wrap a phase in cycles to [0, 1).
inline double wrapPhase (double p) noexcept
{
    p -= std::floor (p);
    return p >= 1.0 ? 0.0 : p;
}

// Sine LFO, phase in cycles: L = sin(2 pi p).
inline double lfoSine (double phaseCycles) noexcept
{
    return std::sin (twoPi * phaseCycles);
}

// Triangle LFO with the sine's phase convention (5.1): 0 at p = 0, +1 at 1/4, 0 at 1/2, -1 at 3/4.
inline double lfoTriangle (double phaseCycles) noexcept
{
    const double p = wrapPhase (phaseCycles);
    if (p < 0.25) return 4.0 * p;
    if (p < 0.75) return 2.0 - 4.0 * p;
    return 4.0 * p - 4.0;
}

// Right-channel phase offset (5.3) in cycles: floor(Stereo * 1024 / 360) / 1024, Stereo in integer degrees.
inline double stereoOffsetCycles (double stereoDeg) noexcept
{
    const long deg = std::lround (stereoDeg < 0.0 ? 0.0 : stereoDeg);
    return double ((deg * 1024L) / 360L) / 1024.0;
}

} // namespace craft::flanger::dsp
