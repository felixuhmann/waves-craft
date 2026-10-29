// Craft Audio doubler - host parameters (spec r1 section 3).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::doubler
{

namespace ids
{
    inline constexpr const char* master      = "master";
    inline constexpr const char* fbHighPass  = "fbHighPass";
    inline constexpr const char* modReset    = "modReset";
    inline constexpr const char* range       = "range";
    inline constexpr const char* align       = "align";
    inline constexpr const char* directOn    = "directOn";
    inline constexpr const char* directGain  = "directGain";
    inline constexpr const char* directPan   = "directPan";
    inline constexpr const char* eqLowOn     = "eqLowOn";
    inline constexpr const char* eqLowGain   = "eqLowGain";
    inline constexpr const char* eqLowFreq   = "eqLowFreq";
    inline constexpr const char* eqHighOn    = "eqHighOn";
    inline constexpr const char* eqHighGain  = "eqHighGain";
    inline constexpr const char* eqHighFreq  = "eqHighFreq";
    inline constexpr const char* routingA    = "routingA";
    inline constexpr const char* routingB    = "routingB";
    inline constexpr const char* bypass      = "bypass";

    // per voice: "v<n><Suffix>", n = 1..4
    inline constexpr const char* on       = "On";
    inline constexpr const char* gain     = "Gain";
    inline constexpr const char* pan      = "Pan";
    inline constexpr const char* delay    = "Delay";
    inline constexpr const char* feedback = "Feedback";
    inline constexpr const char* octave   = "Octave";
    inline constexpr const char* detune   = "Detune";
    inline constexpr const char* depth    = "Depth";
    inline constexpr const char* rate     = "Rate";

    inline juce::String voice (int index0, const char* suffix) { return "v" + juce::String (index0 + 1) + suffix; }
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (int numVoices);

// Display format of a parameter value (number only; units are shown by the GUI / host label).
juce::String formatValue (const juce::String& paramId, double value);

} // namespace craft::doubler
