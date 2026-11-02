// Craft FET Limiter - host parameters (spec r1 section 2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::fet_limiter
{

namespace ids
{
    inline constexpr const char* input      = "input";
    inline constexpr const char* output     = "output";
    inline constexpr const char* attack     = "attack";
    inline constexpr const char* release    = "release";
    inline constexpr const char* ratio      = "ratio";      // index 0..4 = 20 / 12 / 8 / 4 / All
    inline constexpr const char* analog     = "analog";     // index 0..2 = 50 Hz / 60 Hz / Off
    inline constexpr const char* revision   = "revision";   // index 0 = B, 1 = A
    inline constexpr const char* meter      = "meter";      // index 0..2 = GR / In / Out (display only)
    inline constexpr const char* compOff    = "compOff";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* trim       = "trim";
    inline constexpr const char* autoMakeup = "autoMakeup";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace craft::fet_limiter
