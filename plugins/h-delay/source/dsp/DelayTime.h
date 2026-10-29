// Craft Hybrid Delay - delay time resolution (spec r1 section 3).
#pragma once

#include <cmath>

#include "Parameters.h"

namespace craft::hdelay
{

inline constexpr double maxDelayMs = 3500.0;

// Duration of a note value at `tempo` BPM, in ms (no fallback).
inline double noteDurationMs (int noteIndex, double tempo) noexcept
{
    return param::noteBeats (noteIndex) * 60000.0 / tempo;
}

// Note value actually used: if the note is longer than 3500 ms, the longest note in the list whose
// duration is <= 3500 ms (spec 3, review decision 11.4). Returns the index.
inline int effectiveNote (int noteIndex, double tempo) noexcept
{
    if (noteDurationMs (noteIndex, tempo) <= maxDelayMs + 1.0e-9)
        return noteIndex;
    int best = 0;
    double bestMs = -1.0;
    for (int i = 0; i < param::numNotes; ++i)
    {
        const double ms = noteDurationMs (i, tempo);
        if (ms <= maxDelayMs + 1.0e-9 && ms > bestMs)
        {
            best = i;
            bestMs = ms;
        }
    }
    return best;
}

// Delay time in ms for the given mode.
inline double resolveDelayMs (param::Sync sync, double ms, int noteIndex, double manualBpm, double hostBpm) noexcept
{
    if (sync == param::Sync::Ms)
        return std::clamp (ms, 1.0, maxDelayMs);
    const double tempo = std::clamp (sync == param::Sync::Host ? hostBpm : manualBpm, 1.0, 1000.0);
    return noteDurationMs (effectiveNote (noteIndex, tempo), tempo);
}

// Integer delay in samples: D = t_ms * fs / 1000 rounded to nearest (review decision 11.2).
inline double delaySamples (double ms, double sampleRate) noexcept
{
    return std::max (1.0, std::round (ms * sampleRate / 1000.0));
}

// Tap tempo helpers (spec 2 "Tap").
// Nearest note value (in log-duration) to a tapped interval at the given tempo.
inline int nearestNote (double intervalMs, double tempo) noexcept
{
    int best = 0;
    double bestErr = 1.0e30;
    for (int i = 0; i < param::numNotes; ++i)
    {
        const double ms = noteDurationMs (i, tempo);
        if (ms > maxDelayMs + 1.0e-9)
            continue;
        const double err = std::abs (std::log (intervalMs / ms));
        if (err < bestErr)
        {
            bestErr = err;
            best = i;
        }
    }
    return best;
}

} // namespace craft::hdelay
