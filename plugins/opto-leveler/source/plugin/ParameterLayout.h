// Craft Opto Leveler - host parameters (spec r1 section 2).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::opto_leveler
{

namespace ids
{
    inline constexpr const char* gain = "gain";                   // 0 ... 100 display units, taper T0
    inline constexpr const char* peakReduction = "peakReduction"; // 0 ... 100 display units, taper T0
    inline constexpr const char* mode = "mode";                   // Compress / Limit
    inline constexpr const char* hiFreq = "hiFreq";               // 0 ... 100 (100 = flat)
    inline constexpr const char* analog = "analog";               // Off / 50 Hz / 60 Hz
    inline constexpr const char* mix = "mix";                     // 0 ... 100 %
    inline constexpr const char* trim = "trim";                   // -18 ... +18 dB
    inline constexpr const char* autoMakeup = "autoMakeup";       // Off / On (UI coupling, spec 9)
    inline constexpr const char* vu = "vuDisplay";                // In / GR / Out (metering only)
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace craft::opto_leveler
