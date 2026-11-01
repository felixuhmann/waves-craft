#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::vocal
{

juce::String formatValue (const juce::String& id, double v)
{
    if (id == ids::gate)
        return juce::String (param::formatGate (v));
    return juce::String (param::formatDb (v));
}

namespace
{
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name)
{
    // The host-normalised value is the spec's r; mapping and 0.1 dB quantisation live in convertFrom0to1.
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (param::snap<P> (v)); });

    const juce::String paramId (id);
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel ("dB")
                          .withStringFromValueFunction ([paramId] (float v, int) { return formatValue (paramId, v); })
                          .withValueFromStringFunction ([paramId] (const juce::String& text) {
                              if (paramId == ids::gate)
                                  return float (param::parseGate (text.toStdString()));
                              return float (param::snap<P> (text.trim().getDoubleValue()));
                          });

    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (P::defaultValue),
                                                        attributes);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (makeFloat<param::Compression> (ids::compression, "Compression"));
    layout.add (makeFloat<param::Gate> (ids::gate, "Gate"));
    layout.add (makeFloat<param::Gain> (ids::gain, "Gain"));
    auto bypassAttr = juce::AudioParameterBoolAttributes().withStringFromValueFunction (
        [] (bool v, int) { return v ? juce::String ("On") : juce::String ("Off"); });
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::bypass, 1 }, "Bypass", false,
                                                            bypassAttr));
    return layout;
}

} // namespace craft::vocal
