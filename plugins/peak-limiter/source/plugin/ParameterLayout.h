// Craft Peak Limiter - host parameters (spec r1 section 2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::limiter
{

namespace ids
{
    inline constexpr const char* threshold   = "threshold";
    inline constexpr const char* ceiling     = "ceiling";
    inline constexpr const char* release     = "release";
    inline constexpr const char* autoRelease = "autoRelease";
    inline constexpr const char* bypass      = "bypass";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Display text of a parameter value (spec section 2 formats, without unit).
juce::String formatValue (const juce::String& paramId, double value);

} // namespace craft::limiter
