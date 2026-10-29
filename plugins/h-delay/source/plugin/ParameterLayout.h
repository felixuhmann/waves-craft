// Craft Hybrid Delay - host parameters (spec r1 section 2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::hdelay
{

namespace ids
{
    inline constexpr const char* delayMs       = "delayMs";
    inline constexpr const char* delayNote     = "delayNote";
    inline constexpr const char* delayRMs      = "delayRMs";
    inline constexpr const char* delayRNote    = "delayRNote";
    inline constexpr const char* dual          = "dual";
    inline constexpr const char* sync          = "sync";
    inline constexpr const char* bpm           = "bpm";
    inline constexpr const char* feedback      = "feedback";
    inline constexpr const char* pingPong      = "pingPong";
    inline constexpr const char* pingPongStart = "pingPongStart";
    inline constexpr const char* phaseL        = "phaseL";
    inline constexpr const char* phaseR        = "phaseR";
    inline constexpr const char* mix           = "mix";
    inline constexpr const char* output        = "output";
    inline constexpr const char* analog        = "analog";
    inline constexpr const char* lofi          = "lofi";
    inline constexpr const char* modRate       = "modRate";
    inline constexpr const char* modDepth      = "modDepth";
    inline constexpr const char* hiPass        = "hiPass";
    inline constexpr const char* loPass        = "loPass";
    inline constexpr const char* bypass        = "bypass";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace craft::hdelay
