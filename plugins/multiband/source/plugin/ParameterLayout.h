// Craft Multiband - host parameters (spec r2 section 2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Parameters.h"

namespace craft::multiband
{

namespace ids
{
    inline constexpr const char* outputGain  = "outputGain";
    inline constexpr const char* xoverLow    = "xoverLow";
    inline constexpr const char* xoverMid    = "xoverMid";
    inline constexpr const char* xoverHigh   = "xoverHigh";
    inline constexpr const char* xoverQ      = "xoverQ";
    inline constexpr const char* knee        = "knee";
    inline constexpr const char* autoRelease = "autoRelease";
    inline constexpr const char* opto        = "opto";
    inline constexpr const char* bypass      = "bypass";

    inline const char* crossover (int k)
    {
        static const char* const names[] = { xoverLow, xoverMid, xoverHigh };
        return names[k];
    }

    // Band parameters: "b1Gain" ... "b4Bypass" (band index 0..3).
    enum class BandParam { gain, range, threshold, attack, release, solo, bypass };
    juce::String band (int b, BandParam p);
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Display text of a parameter value (spec section 2 formats, without unit).
juce::String formatValue (const juce::String& paramId, double value);

} // namespace craft::multiband
