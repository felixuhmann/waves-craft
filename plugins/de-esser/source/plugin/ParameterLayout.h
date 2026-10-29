// Craft De-Esser - host parameters (spec r1 section 3).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::deesser
{

namespace ids
{
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* frequency = "frequency";
    inline constexpr const char* sidechain = "sidechain"; // Band-Pass | High-Pass
    inline constexpr const char* audio     = "audio";     // Split | Wideband
    inline constexpr const char* monitor   = "monitor";   // Output | SideChain
    inline constexpr const char* bypass    = "bypass";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Value formats of spec section 3 ("-43.6 dB", integer Hz).
juce::String formatValue (const juce::String& paramId, double value);

} // namespace craft::deesser
