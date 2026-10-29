// Craft Audio stereo imager - host parameters (spec r1 section 3).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Matrix.h"

namespace craft
{

namespace ids
{
    inline constexpr const char* gain      = "gain";
    inline constexpr const char* width     = "width";
    inline constexpr const char* rotation  = "rotation";
    inline constexpr const char* asymmetry = "asymmetry";
    inline constexpr const char* shuffle   = "shuffle";
    inline constexpr const char* frequency = "frequency";
    inline constexpr const char* bassTrim  = "bassTrim";
    inline constexpr const char* swap      = "swap";
    inline constexpr const char* polarity1 = "polarity1";
    inline constexpr const char* polarity2 = "polarity2";
    inline constexpr const char* inputMode = "inputMode";
    inline constexpr const char* bypass    = "bypass";

    // non-automatable state stored with the plug-in state
    inline constexpr const char* meterMode = "meterMode";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (dsp::Variant variant);

// Value formats of spec section 12.
juce::String formatValue (const juce::String& paramId, double value);

} // namespace craft
