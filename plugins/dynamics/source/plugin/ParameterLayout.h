// Craft Dynamics - host parameters (spec r1 section 2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/DynamicsEngine.h"

namespace craft::dynamics
{

namespace ids
{
    // compressor / expander
    inline constexpr const char* compThreshold = "compThreshold";
    inline constexpr const char* ratio         = "ratio";
    inline constexpr const char* compAttack    = "compAttack";
    inline constexpr const char* compRelease   = "compRelease";
    inline constexpr const char* pdr           = "pdr";
    inline constexpr const char* makeup        = "makeup";
    inline constexpr const char* levelRef      = "levelRef";    // choice: LowRef, PeakRef
    inline constexpr const char* compBypass    = "compBypass";
    inline constexpr const char* compEq        = "compEq";      // choice: Wideband, Sidechain, Split
    // gate / expander
    inline constexpr const char* gateMode      = "gateMode";    // choice: Gate, Expander
    inline constexpr const char* gateOpen      = "gateOpen";
    inline constexpr const char* gateClose     = "gateClose";
    inline constexpr const char* gateAttack    = "gateAttack";
    inline constexpr const char* gateRelease   = "gateRelease";
    inline constexpr const char* hold          = "hold";
    inline constexpr const char* floor         = "floor";
    inline constexpr const char* gateBypass    = "gateBypass";
    inline constexpr const char* gateEq        = "gateEq";
    // filter module
    inline constexpr const char* filterType    = "filterType";  // choice: Low-pass, Band-pass, High-pass, Band-reject
    inline constexpr const char* filterFreq    = "filterFreq";
    inline constexpr const char* filterQ       = "filterQ";
    // global
    inline constexpr const char* monitor       = "monitor";     // choice: Audio, Sidechain, Passive
    inline constexpr const char* keyMode       = "keyMode";     // choice: Stereo, L>R, R>L
    inline constexpr const char* lookahead     = "lookahead";
    inline constexpr const char* inputGainL    = "inputGainL";
    inline constexpr const char* inputGainR    = "inputGainR";
    inline constexpr const char* outputGain    = "outputGain";
    inline constexpr const char* linkThreshold = "linkThreshold";
    inline constexpr const char* linkAttack    = "linkAttack";
    inline constexpr const char* linkRelease   = "linkRelease";
    inline constexpr const char* bypass        = "bypass";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (dsp::Variant variant);

// Display quantity of a float parameter (for the editor's value boxes).
dsp::Quantity quantityFor (const juce::String& paramId);

} // namespace craft::dynamics
