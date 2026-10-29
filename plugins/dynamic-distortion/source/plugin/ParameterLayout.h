// Craft Dynamic Distortion - host parameters (spec r1 section 3). The host-normalised value is the spec's r.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::ddp
{

namespace ids
{
    inline constexpr const char* input = "input";
    inline constexpr const char* direct = "direct";
    inline constexpr const char* drive = "drive";
    inline constexpr const char* distLevel = "distLevel";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* bass = "bass";
    inline constexpr const char* mid = "mid";
    inline constexpr const char* midFreq = "midFreq";
    inline constexpr const char* treble = "treble";
    inline constexpr const char* lowPass = "lowPass";
    inline constexpr const char* output = "output";
    inline constexpr const char* bypass = "bypass";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Value text in the spec's display resolution (shared by host and editor).
namespace text
{
    juce::String decibels (double v);   // "+1.5 dB" [0.1]
    juce::String percent (double v);    // "62.0 %" [0.1]
    juce::String attackMs (double v);   // "1.0 ms" [0.1]
    juce::String releaseMs (double v);  // "31.80 ms" [0.01]
    juce::String toneValue (double v);  // "-15.0" [0.5]
    juce::String hertz (double v);      // "1252 Hz" [1]
} // namespace text

} // namespace craft::ddp
