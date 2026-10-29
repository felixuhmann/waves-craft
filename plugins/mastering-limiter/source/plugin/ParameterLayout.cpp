#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::mastering_limiter
{

namespace
{
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name, bool isMs)
{
    // The host-normalised value r is the spec 2 mapping; convertFrom0to1 applies the mapping and the display
    // resolution (0.1 dB / 0.01 ms), so every value reported back is a step value.
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (P::snap (v)); });

    auto attributes = juce::AudioParameterFloatAttributes()
                          .withStringFromValueFunction ([isMs] (float v, int) {
                              return juce::String (isMs ? dsp::formatMs (double (v)) : dsp::formatDb (double (v)));
                          })
                          .withValueFromStringFunction ([] (const juce::String& text) {
                              return float (P::snap (dsp::parseNumber (text.toStdString(), P::defaultValue)));
                          });

    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (P::defaultValue),
                                                        attributes);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    namespace pm = dsp::param;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Units are part of the value text, so the labels stay empty (no "-12.0 dB dB" in hosts).
    layout.add (makeFloat<pm::Level> (ids::threshold, "Threshold", false));
    layout.add (makeFloat<pm::Level> (ids::ceiling, "Out Ceiling", false));
    layout.add (makeFloat<pm::Release> (ids::release, "Release", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ids::adaptive, 1 }, "Adaptive Release", true,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return v ? "On" : "Off"; })));
    // AudioParameterChoice maps index k of n to r = k / (n - 1): exactly the spec 2 step positions.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ids::wordLength, 1 }, "Word Length",
        juce::StringArray { "24 Bits", "22 Bits", "20 Bits", "18 Bits", "16 Bits" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::dither, 1 }, "Dither",
                                                              juce::StringArray { "None", "Type 1", "Type 2" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ids::shaping, 1 }, "Noise Shaping", juce::StringArray { "None", "Moderate", "Normal", "Ultra" }, 2));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ids::bypass, 1 }, "Bypass", false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return v ? "On" : "Off"; })));
    return layout;
}

} // namespace craft::mastering_limiter
