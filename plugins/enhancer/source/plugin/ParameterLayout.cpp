#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::enhancer
{

namespace
{
using StringFn = std::string (*) (double);

template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name, double defaultValue,
                                                      StringFn toText, double offValue, const juce::String& unit = {})
{
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (P::snap (v)); });

    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (unit)
                          .withStringFromValueFunction ([toText] (float v, int) { return juce::String (toText (v)); })
                          .withValueFromStringFunction ([offValue] (const juce::String& text) {
                              return float (P::snap (dsp::parseNumber (text.toStdString(), offValue, offValue)));
                          });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (defaultValue), attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id, const juce::String& name)
{
    auto attributes = juce::AudioParameterBoolAttributes().withStringFromValueFunction (
        [] (bool v, int) { return juce::String (v ? "On" : "Off"); });
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, false, attributes);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    namespace pm = dsp::param;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (makeFloat<pm::Gain> (ids::input, "Input", 0.0, dsp::formatDb, 0.0));
    layout.add (makeFloat<pm::Direct> (ids::direct, "Direct", 0.0, dsp::formatDirect, pm::Direct::offValue));
    for (int b = 0; b < dsp::numBands; ++b)
        layout.add (makeFloat<pm::BandGain> (ids::bandGain (b), "Band " + ids::bandLetter (b) + " Gain", 0.0, dsp::formatBandGain, pm::BandGain::offValue));
    for (int k = 0; k < dsp::numCrossovers; ++k)
        layout.add (makeFloat<pm::Crossover> (ids::crossover (k), "Crossover " + juce::String (k + 1), pm::crossoverDefaults[k], dsp::formatHz, pm::crossoverDefaults[k]));
    for (int b = 0; b < dsp::numBands; ++b)
        layout.add (makeFloat<pm::Width> (ids::width (b), "Band " + ids::bandLetter (b) + " Width", 1.0, dsp::formatWidth, 1.0));
    layout.add (makeFloat<pm::Punch> (ids::punch, "Punch", 3.0, dsp::formatPunch, 3.0));
    for (int b = 0; b < dsp::numBands; ++b)
    {
        layout.add (makeBool (ids::solo (b), "Band " + ids::bandLetter (b) + " Solo"));
        layout.add (makeBool (ids::bypass (b), "Band " + ids::bandLetter (b) + " Bypass"));
    }
    layout.add (makeFloat<pm::Gain> (ids::output, "Output", 0.0, dsp::formatDb, 0.0));
    return layout;
}

} // namespace craft::enhancer
