// Craft Console G - host parameters (spec section 2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/ConsoleEngine.h"

namespace craft::consoleg
{

namespace ids
{
    // dynamics
    inline constexpr const char* compThreshold = "compThreshold";
    inline constexpr const char* compRatio = "compRatio";
    inline constexpr const char* compRelease = "compRelease";
    inline constexpr const char* compFast = "compFast";
    inline constexpr const char* expThreshold = "expThreshold";
    inline constexpr const char* expRange = "expRange";
    inline constexpr const char* expRelease = "expRelease";
    inline constexpr const char* expGate = "expGate";
    inline constexpr const char* expFast = "expFast";
    inline constexpr const char* dynBypass = "dynBypass";
    inline constexpr const char* dynPostEq = "dynPostEq";
    inline constexpr const char* filtersSidechain = "filtersSidechain";
    inline constexpr const char* filtersSplit = "filtersSplit";
    // filters + EQ
    inline constexpr const char* hpFreq = "hpFreq";
    inline constexpr const char* lpFreq = "lpFreq";
    inline constexpr const char* eqBypass = "eqBypass";
    inline constexpr const char* lfGain = "lfGain";
    inline constexpr const char* lfFreq = "lfFreq";
    inline constexpr const char* lmfGain = "lmfGain";
    inline constexpr const char* lmfFreq = "lmfFreq";
    inline constexpr const char* lmfQ = "lmfQ";
    inline constexpr const char* lmfDiv3 = "lmfDiv3";
    inline constexpr const char* hmfGain = "hmfGain";
    inline constexpr const char* hmfFreq = "hmfFreq";
    inline constexpr const char* hmfQ = "hmfQ";
    inline constexpr const char* hmfX3 = "hmfX3";
    inline constexpr const char* hfGain = "hfGain";
    inline constexpr const char* hfFreq = "hfFreq";
    // master
    inline constexpr const char* inputTrim = "inputTrim";
    inline constexpr const char* outputGain = "outputGain";
    inline constexpr const char* analog = "analog";
    inline constexpr const char* polarity = "polarity";
    inline constexpr const char* meterSource = "meterSource";
} // namespace ids

// Continuous parameter <-> taper (the host value is the spec's r).
struct ContinuousParam
{
    const char* id;
    const char* name;
    dsp::TaperId taper;
};
const std::vector<ContinuousParam>& continuousParams();

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Reads all host values into engine settings.
dsp::Settings readSettings (juce::AudioProcessorValueTreeState& state);

} // namespace craft::consoleg
