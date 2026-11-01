#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::limiter
{

juce::String formatValue (const juce::String& id, double v)
{
    if (id == ids::release)
        return juce::String (param::formatReleaseMs (v));
    return juce::String (param::formatDb (v));
}

namespace
{
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name, const juce::String& unit)
{
    // The host-normalised value is the spec's r; the mapping and the quantisation to the displayed resolution are
    // applied in convertFrom0to1, so the host and the GUI only ever see step values.
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (param::snap<P> (v)); });

    const juce::String paramId (id);
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (unit)
                          .withStringFromValueFunction ([paramId] (float v, int) { return formatValue (paramId, v); })
                          .withValueFromStringFunction ([] (const juce::String& text) {
                              return float (param::snap<P> (text.trim().getDoubleValue()));
                          });

    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (P::defaultValue),
                                                        attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id, const juce::String& name)
{
    auto attributes = juce::AudioParameterBoolAttributes().withStringFromValueFunction (
        [] (bool v, int) { return v ? juce::String ("On") : juce::String ("Off"); });
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, false, attributes);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (makeFloat<param::Threshold> (ids::threshold, "Threshold", "dB"));
    layout.add (makeFloat<param::Ceiling> (ids::ceiling, "Out Ceiling", "dB"));
    layout.add (makeFloat<param::Release> (ids::release, "Release", "ms"));
    layout.add (makeBool (ids::autoRelease, "Auto Release"));
    layout.add (makeBool (ids::bypass, "Bypass"));
    return layout;
}

} // namespace craft::limiter
