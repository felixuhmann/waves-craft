#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft
{

juce::String formatValue (const juce::String& id, double v)
{
    // avoid "-0.0"
    auto fmt = [] (double x, int decimals) {
        const double q = std::pow (10.0, decimals);
        x = std::round (x * q) / q;
        if (x == 0.0)
            x = 0.0;
        return juce::String (x, decimals);
    };
    if (id == ids::gain)      return fmt (v, 1);
    if (id == ids::width)     return fmt (v, 2);
    if (id == ids::shuffle)   return fmt (v, 1);
    if (id == ids::frequency) return fmt (v, 0);
    if (id == ids::asymmetry) return fmt (v, 0);
    if (id == ids::rotation)  return fmt (v, 1);
    if (id == ids::bassTrim)  return fmt (v, 1);
    return fmt (v, 2);
}

namespace
{
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name, const juce::String& unit)
{
    // The normalised value r is what the host sees; the mapping of spec section 3 and the
    // quantisation are applied by convertFrom0to1, so every value reported back is a step value.
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
                              auto t = text.trim().toLowerCase();
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

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id, const juce::String& name, const juce::String& offText,
                                                    const juce::String& onText)
{
    auto attributes = juce::AudioParameterBoolAttributes().withStringFromValueFunction (
        [offText, onText] (bool v, int) { return v ? onText : offText; });
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, false, attributes);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (dsp::Variant variant)
{
    namespace pm = dsp::param;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    if (variant != dsp::Variant::MsMatrix)
    {
        layout.add (makeFloat<pm::Gain> (ids::gain, "Gain", "dB"));
        layout.add (makeFloat<pm::Width> (ids::width, "Width", ""));
        layout.add (makeFloat<pm::Rotation> (ids::rotation, "Rotation", "deg"));
        layout.add (makeFloat<pm::Asymmetry> (ids::asymmetry, "Asymmetry", "deg"));
        if (variant == dsp::Variant::Full)
        {
            layout.add (makeFloat<pm::Shuffle> (ids::shuffle, "Shuffle", ""));
            layout.add (makeFloat<pm::Frequency> (ids::frequency, "Frequency", "Hz"));
            layout.add (makeFloat<pm::BassTrim> (ids::bassTrim, "Bass Trim", "dB"));
        }
        layout.add (makeBool (ids::swap, "Swap", "Off", "On"));
        layout.add (makeBool (ids::polarity1, "Polarity 1", "+", "-"));
        layout.add (makeBool (ids::polarity2, "Polarity 2", "+", "-"));
        layout.add (makeBool (ids::inputMode, "Input Mode", "L-R", "M-S"));
    }
    layout.add (makeBool (ids::bypass, "Bypass", "Off", "On"));
    return layout;
}

} // namespace craft
