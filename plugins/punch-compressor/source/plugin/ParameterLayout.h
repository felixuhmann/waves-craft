// Craft Punch Compressor - host parameters (spec r1 section 2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::punch
{

namespace ids
{
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* ratio     = "ratio";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* attack    = "attack";
    inline constexpr const char* punch     = "punch";
    inline constexpr const char* release   = "release";
    inline constexpr const char* sync      = "sync";
    inline constexpr const char* note      = "note";
    inline constexpr const char* analog    = "analog";
    inline constexpr const char* output    = "output";
    inline constexpr const char* limiter   = "limiter";
    inline constexpr const char* meter     = "meter";
    inline constexpr const char* tempo     = "tempo";
    inline constexpr const char* bypass    = "bypass";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Display text of a parameter value (section 2 formats, without unit).
juce::String formatValue (const juce::String& paramId, double value);

} // namespace craft::punch
