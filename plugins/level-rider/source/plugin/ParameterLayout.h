// Craft Level Rider - host parameters (spec r1 section 2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::levelrider
{

namespace ids
{
    inline constexpr const char* target    = "target";
    inline constexpr const char* vocalSens = "vocalSens";
    inline constexpr const char* musicSens = "musicSens"; // r = 0: Off
    inline constexpr const char* rangeMax  = "rangeMax";
    inline constexpr const char* rangeMin  = "rangeMin";
    inline constexpr const char* idle      = "idle";
    inline constexpr const char* rider     = "rider";     // the ride gain g (section 9)
    inline constexpr const char* output    = "output";
    inline constexpr const char* attack    = "attack";    // Slow | Fast
    inline constexpr const char* spill     = "spill";     // Live products only
    inline constexpr const char* bypass    = "bypass";

    // non-parameter state (saved with the plug-in state, section 9)
    inline constexpr const char* automationMode = "automationMode";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (bool live);

// Display text of a parameter value in its section 2 format ("-21.0 dB", "Off").
juce::String formatValue (const juce::String& paramId, double value, bool withUnit = false);

} // namespace craft::levelrider
