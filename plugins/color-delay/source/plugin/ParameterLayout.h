// Craft Colour Delay - host parameters (spec r1 section 2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft
{

namespace ids
{
    inline constexpr const char* input        = "input";
    inline constexpr const char* output       = "output";
    inline constexpr const char* mode         = "mode";
    inline constexpr const char* delayMsL     = "delayMsL";
    inline constexpr const char* delayMsR     = "delayMsR";
    inline constexpr const char* noteL        = "noteL";
    inline constexpr const char* noteR        = "noteR";
    inline constexpr const char* feedbackL    = "feedbackL";
    inline constexpr const char* feedbackR    = "feedbackR";
    inline constexpr const char* delayLink    = "delayLink";
    inline constexpr const char* dryWet       = "dryWet";
    inline constexpr const char* hp           = "hp";
    inline constexpr const char* lp           = "lp";
    inline constexpr const char* filterLink   = "filterLink";
    inline constexpr const char* verbOn       = "verbOn";
    inline constexpr const char* distOn       = "distOn";
    inline constexpr const char* doublerOn    = "doublerOn";
    inline constexpr const char* phaserOn     = "phaserOn";
    inline constexpr const char* verbSize     = "verbSize";
    inline constexpr const char* verbLevel    = "verbLevel";
    inline constexpr const char* distLevel    = "distLevel";
    inline constexpr const char* distSend     = "distSend";
    inline constexpr const char* distWidth    = "distWidth";
    inline constexpr const char* doublerLevel = "doublerLevel";
    inline constexpr const char* doublerDetune = "doublerDetune";
    inline constexpr const char* phaserLevel  = "phaserLevel";
    inline constexpr const char* phaserRate   = "phaserRate";
    inline constexpr const char* monitor      = "monitor";
    inline constexpr const char* bypass       = "bypass";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Display text of a parameter value (physical units), used by the host and the editor.
juce::String formatValue (const juce::String& paramId, double value);

} // namespace craft
