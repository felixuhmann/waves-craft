// Craft Enhancer - host parameters (spec r1 section 1).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::enhancer
{

namespace ids
{
    inline const juce::String input = "input", output = "output", direct = "direct", punch = "punch";
    inline juce::String bandLetter (int b) { return juce::String::charToString (juce::juce_wchar ('A' + b)); }
    inline juce::String bandGain (int b) { return "gain" + bandLetter (b); }
    inline juce::String width (int b) { return "width" + bandLetter (b); }
    inline juce::String solo (int b) { return "solo" + bandLetter (b); }
    inline juce::String bypass (int b) { return "bypass" + bandLetter (b); }
    inline juce::String crossover (int k) { return "xover" + juce::String (k + 1); }
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace craft::enhancer
