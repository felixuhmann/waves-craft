// Craft Band Limiter - settings, parameter mappings and profiles (spec r1 sections 3, 9). Host independent.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "../../../mastering-limiter/source/dsp/Requantiser.h"

namespace craft::band_limiter::dsp
{

using craft::mastering_limiter::dsp::Dither;
using craft::mastering_limiter::dsp::Requantiser;
using craft::mastering_limiter::dsp::Shaping;

inline constexpr int numBands = 5;
inline constexpr int numCrossovers = 4;
inline constexpr int numProfiles = 9;

enum class Variant
{
    LinearMulti = 0,
    LinearProfile = 1,
    LowLatencyMulti = 2,
    LowLatencyProfile = 3
};

inline constexpr bool isLinearPhase (Variant v) noexcept { return v == Variant::LinearMulti || v == Variant::LinearProfile; }
inline constexpr bool isProfileVariant (Variant v) noexcept { return v == Variant::LinearProfile || v == Variant::LowLatencyProfile; }

enum class Behaviour
{
    Adaptive = 0,
    Aggressive = 1,
    Warm = 2,
    Scaled = 3,
    Manual = 4
};

inline constexpr std::array<double, numBands> defaultReleaseMs { 598.66, 503.04, 402.20, 298.46, 200.52 };
inline constexpr std::array<double, numCrossovers> defaultCrossoverHz { 80.0, 320.0, 1278.0, 5113.0 };
inline constexpr std::array<double, numCrossovers> crossoverMinHz { 40.0, 150.0, 1022.0, 4000.0 };
inline constexpr std::array<double, numCrossovers> crossoverMaxHz { 350.0, 3000.0, 4757.0, 16000.0 };
inline constexpr int wordLengths[5] = { 24, 22, 20, 18, 16 };

struct Settings
{
    double thresholdDb = 0.0, ceilingDb = 0.0;
    std::array<double, numBands> gainDb { 0.0, 0.0, 0.0, 0.0, 0.0 };
    std::array<double, numBands> priority { 0.0, 0.0, 0.0, 0.0, 0.0 };
    std::array<double, numBands> releaseMs = defaultReleaseMs;
    std::array<bool, numBands> solo { false, false, false, false, false };
    std::array<double, numCrossovers> crossoverHz = defaultCrossoverHz;
    double separation = 1.0; // 0..1
    Behaviour behaviour = Behaviour::Adaptive;
    int wordLength = 24;
    Dither dither = Dither::Type1;
    Shaping shaping = Shaping::Normal;
};

namespace param
{
    inline double levelFromNorm (double r) { return -30.0 + 30.0 * std::clamp (r, 0.0, 1.0); }
    inline double levelToNorm (double db) { return std::clamp ((db + 30.0) / 30.0, 0.0, 1.0); }
    inline double bandDbFromNorm (double r) { return -12.0 + 24.0 * std::clamp (r, 0.0, 1.0); }
    inline double bandDbToNorm (double db) { return std::clamp ((db + 12.0) / 24.0, 0.0, 1.0); }

    inline constexpr double releaseSpan = 250000.0;
    inline double releaseFromNorm (double r) { return 0.01 * std::pow (releaseSpan, std::clamp (r, 0.0, 1.0)); }
    inline double releaseToNorm (double ms) { return std::clamp (std::log (std::max (ms, 0.01) / 0.01) / std::log (releaseSpan), 0.0, 1.0); }

    inline constexpr double crossoverSpan = 1334.8;
    inline double crossoverRawFromNorm (double r) { return 16.0 * std::pow (crossoverSpan, std::clamp (r, 0.0, 1.0)); }
    inline double crossoverToNorm (double hz) { return std::clamp (std::log (std::max (hz, 16.0) / 16.0) / std::log (crossoverSpan), 0.0, 1.0); }
    inline double clampCrossover (int k, double hz) { return std::clamp (hz, crossoverMinHz[size_t (k)], crossoverMaxHz[size_t (k)]); } // D3
    inline double crossoverFromNorm (int k, double r) { return clampCrossover (k, crossoverRawFromNorm (r)); }

    inline double multiplierFromNorm (double r) { return 0.1 * std::pow (100.0, std::clamp (r, 0.0, 1.0)); }
    inline double multiplierToNorm (double m) { return std::clamp (std::log (std::max (m, 0.1) / 0.1) / std::log (100.0), 0.0, 1.0); }

    // Spec 9: p = floor(8 r + 0.5) + 1
    inline int profileFromNorm (double r) { return int (std::floor (8.0 * std::clamp (r, 0.0, 1.0) + 0.5)) + 1; }
} // namespace param

// One complete set of the Multi controls (spec 9). Values were chosen against tables/profiles.csv, the
// spec gives the measured behaviour, not the control values (see IMPLEMENTATION_NOTES).
struct Profile
{
    const char* name;
    std::array<double, numBands> priority;
    double releaseScale;
    double separation;
    Behaviour behaviour;
};

inline constexpr std::array<Profile, numProfiles> profiles { {
    { "1 Standard",      { 0, 0, 0, 0, 0 },        1.00, 1.0, Behaviour::Adaptive },
    { "2 Relaxed",       { 0, 0, 0, 0, 0 },        1.55, 1.0, Behaviour::Adaptive },
    { "3 Merged",        { 0, 0, 0, 0, 0 },        2.70, 0.5, Behaviour::Adaptive },
    { "4 Broad",         { 0, 0, 0, 0, 0 },        1.30, 0.0, Behaviour::Adaptive },
    { "5 Broad Fixed",   { 0, 0, 0, 0, 0 },        0.75, 0.0, Behaviour::Manual },
    { "6 Warm Balance",  { 0, 3, 0, 0, 0 },        1.45, 1.0, Behaviour::Adaptive },
    { "7 Long",          { 0, 0, 0, 0, 0 },        1.50, 1.0, Behaviour::Adaptive },
    { "8 Bright",        { 0, -6, 0, 0, 0 },       1.40, 1.0, Behaviour::Adaptive },
    { "9 Low Weight",    { 0, 12, 0, -12, 0 },     1.45, 1.0, Behaviour::Adaptive },
} };

// Simple variants: expands a profile (1..9) and the release multiplier into Settings; other fields are kept.
inline void applyProfile (Settings& s, int profileNumber, double releaseMultiplier) noexcept
{
    const auto& p = profiles[size_t (std::clamp (profileNumber, 1, numProfiles) - 1)];
    s.priority = p.priority;
    s.separation = p.separation;
    s.behaviour = p.behaviour;
    s.crossoverHz = defaultCrossoverHz;
    for (int b = 0; b < numBands; ++b)
        s.releaseMs[size_t (b)] = defaultReleaseMs[size_t (b)] * p.releaseScale * releaseMultiplier;
}

} // namespace craft::band_limiter::dsp
