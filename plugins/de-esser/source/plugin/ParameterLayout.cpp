#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::deesser
{

juce::String formatValue (const juce::String& id, double v)
{
    auto fmt = [] (double x, int decimals) {
        const double q = std::pow (10.0, decimals);
        x = std::round (x * q) / q;
        if (x == 0.0)
            x = 0.0; // avoid "-0.0"
        return juce::String (x, decimals);
    };
    if (id == ids::threshold) return fmt (v, 1);
    if (id == ids::frequency) return fmt (v, 0);
    return fmt (v, 2);
}

namespace
{
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name, const juce::String& unit)
{
    // The host-normalised value is the spec's r; the section 3 mapping and quantisation are applied by
    // convertFrom0to1, so every value reported back is a step value.
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (dsp::param::snap<P> (v)); });

    const juce::String paramId (id);
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (unit)
                          .withStringFromValueFunction ([paramId] (float v, int) { return formatValue (paramId, v); })
                          .withValueFromStringFunction ([] (const juce::String& text) {
                              auto t = text.trim().toLowerCase().removeCharacters (" dbhz");
                              double mult = 1.0;
                              if (t.endsWithChar ('k'))
                              {
                                  mult = 1000.0;
                                  t = t.dropLastCharacters (1);
                              }
                              return float (dsp::param::snap<P> (t.getDoubleValue() * mult));
                          });

    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (P::defaultValue),
                                                        attributes);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const char* id, const juce::String& name, const juce::StringArray& states,
                                                        int defaultIndex)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, states, defaultIndex);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    namespace pm = dsp::param;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (makeFloat<pm::Threshold> (ids::threshold, "Threshold", "dB"));
    layout.add (makeFloat<pm::Frequency> (ids::frequency, "Freq", "Hz"));
    // two-state switches: normalised r < 0.5 selects the first entry (spec section 3)
    layout.add (makeChoice (ids::sidechain, "SideChain", { "Band-Pass", "High-Pass" }, 1));
    layout.add (makeChoice (ids::audio, "Audio", { "Split", "Wideband" }, 0));
    layout.add (makeChoice (ids::monitor, "Monitor", { "Output", "SideChain" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::bypass, 1 }, "Bypass", false));
    return layout;
}

} // namespace craft::deesser
