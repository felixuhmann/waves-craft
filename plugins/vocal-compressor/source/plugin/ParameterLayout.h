// Craft Vocal Compressor - host parameters (spec r1 section 3).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::vocal
{

namespace ids
{
    inline constexpr const char* compression = "compression";
    inline constexpr const char* gate        = "gate";
    inline constexpr const char* gain        = "gain";
    inline constexpr const char* bypass      = "bypass";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Display text of a parameter value (0.1 dB; the gate shows "-Inf" when off).
juce::String formatValue (const juce::String& paramId, double value);

} // namespace craft::vocal
