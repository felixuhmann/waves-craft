// Craft Console E - host parameters (spec r1 section 2). Every continuous control is stored as its
// host-normalised position r; the displayed value follows the section 2 taper.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::consolee
{

namespace ids
{
    // I/O
    inline constexpr const char* inputTrim = "inputTrim";
    inline constexpr const char* outputFader = "outputFader";
    inline constexpr const char* polarity = "polarity";
    inline constexpr const char* analogue = "analogue";
    inline constexpr const char* meterOutput = "meterOutput"; // display only: input / output meter
    // filters
    inline constexpr const char* hp = "hpFreq";
    inline constexpr const char* lp = "lpFreq";
    inline constexpr const char* split = "split";
    // EQ
    inline constexpr const char* lfGain = "lfGain";
    inline constexpr const char* lfFreq = "lfFreq";
    inline constexpr const char* lfBell = "lfBell";
    inline constexpr const char* lmfGain = "lmfGain";
    inline constexpr const char* lmfFreq = "lmfFreq";
    inline constexpr const char* lmfQ = "lmfQ";
    inline constexpr const char* hmfGain = "hmfGain";
    inline constexpr const char* hmfFreq = "hmfFreq";
    inline constexpr const char* hmfQ = "hmfQ";
    inline constexpr const char* hfGain = "hfGain";
    inline constexpr const char* hfFreq = "hfFreq";
    inline constexpr const char* hfBell = "hfBell";
    inline constexpr const char* eqBypass = "eqBypass";
    inline constexpr const char* eqToSidechain = "eqToSidechain";
    // dynamics
    inline constexpr const char* compRatio = "compRatio";
    inline constexpr const char* compThreshold = "compThreshold";
    inline constexpr const char* compRelease = "compRelease";
    inline constexpr const char* compFast = "compFastAttack";
    inline constexpr const char* expRange = "expRange";
    inline constexpr const char* expThreshold = "expThreshold";
    inline constexpr const char* expRelease = "expRelease";
    inline constexpr const char* expFast = "expFastAttack";
    inline constexpr const char* gate = "gateMode";
    inline constexpr const char* dynBypass = "dynamicsBypass";
    inline constexpr const char* channelOut = "channelOut";
    // host bypass
    inline constexpr const char* bypass = "bypass";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Display text of a continuous control at normalised position r (section 2 resolutions), without unit.
juce::String formatValue (const juce::String& paramId, double r);
// Unit shown next to the value ("dB", "Hz", "kHz", "s", "" ...) for position r.
juce::String unitFor (const juce::String& paramId, double r);
// Parses typed text back to a normalised position.
double parseValue (const juce::String& paramId, const juce::String& text, double currentR);

} // namespace craft::consolee
