// Craft Mastering Limiter - host parameters (spec r1 section 2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::mastering_limiter
{

namespace ids
{
    inline constexpr const char* threshold  = "threshold";
    inline constexpr const char* ceiling    = "ceiling";
    inline constexpr const char* release    = "release";
    inline constexpr const char* adaptive   = "adaptiveRelease";
    inline constexpr const char* wordLength = "wordLength"; // choice: 24 / 22 / 20 / 18 / 16 Bits
    inline constexpr const char* dither     = "dither";     // choice: None / Type 1 / Type 2
    inline constexpr const char* shaping    = "shaping";    // choice: None / Moderate / Normal / Ultra
    inline constexpr const char* bypass     = "bypass";
} // namespace ids

// Editor-only state stored in the plug-in state (not automatable): Threshold / Ceiling marker link.
inline constexpr const char* linkMarkersProperty = "linkMarkers";

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace craft::mastering_limiter
