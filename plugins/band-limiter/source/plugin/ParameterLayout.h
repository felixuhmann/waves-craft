// Craft Band Limiter - host parameters (spec r1 section 3).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Parameters.h"

namespace craft::band_limiter
{

namespace ids
{
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* ceiling = "ceiling";
    inline constexpr const char* separation = "separation";
    inline constexpr const char* behaviour = "releaseBehaviour";
    inline constexpr const char* multiplier = "releaseMultiplier";
    inline constexpr const char* profile = "profile";
    inline constexpr const char* quantize = "quantize";
    inline constexpr const char* dither = "dither";
    inline constexpr const char* shaping = "shaping";
    inline constexpr const char* bypass = "bypass";

    inline juce::String gain (int band) { return "gain" + juce::String (band + 1); }
    inline juce::String priority (int band) { return "priority" + juce::String (band + 1); }
    inline juce::String release (int band) { return "release" + juce::String (band + 1); }
    inline juce::String solo (int band) { return "solo" + juce::String (band + 1); }
    inline juce::String crossover (int k) { return "crossover" + juce::String (k + 1); }
} // namespace ids

inline const char* const crossoverNames[dsp::numCrossovers] = { "Low", "LowMid", "HighMid", "High" };

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (dsp::Variant variant);

} // namespace craft::band_limiter
