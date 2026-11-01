// Craft Flanger - host parameters (spec r1 section 2).
#pragma once

#include <atomic>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/FlangerEngine.h"

namespace craft::flanger
{

namespace ids
{
    inline constexpr const char* delay          = "delay";
    inline constexpr const char* depth          = "depth";
    inline constexpr const char* rate           = "rate";
    inline constexpr const char* mix            = "mix";
    inline constexpr const char* feedback       = "feedback";
    inline constexpr const char* gain           = "gain";
    inline constexpr const char* stereo         = "stereo";
    inline constexpr const char* filterFreq     = "filterFreq";
    inline constexpr const char* filterType     = "filterType";     // choice: Low-pass, High-pass
    inline constexpr const char* filterOn       = "filterOn";
    inline constexpr const char* waveform       = "waveform";       // choice: Sine, Triangle
    inline constexpr const char* wetInvert      = "wetInvert";
    inline constexpr const char* feedbackInvert = "feedbackInvert";
    inline constexpr const char* tape           = "tape";
    inline constexpr const char* stop           = "stop";
    inline constexpr const char* link           = "link";
    inline constexpr const char* tempoSync      = "tempoSync";
    inline constexpr const char* bypass         = "bypass";
} // namespace ids

// `tempoSyncShown` lets the Rate text follow the Tempo Sync switch ("Hz" / "x/Beat", spec 9).
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (dsp::Variant variant,
                                                                          std::shared_ptr<std::atomic<bool>> tempoSyncShown);

// Unit shown after a value in the editor.
juce::String unitFor (const juce::String& paramId, bool tempoSync);

} // namespace craft::flanger
