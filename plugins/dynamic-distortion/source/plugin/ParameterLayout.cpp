#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::ddp
{

namespace dd = craft::dd;

namespace
{
juce::String fixed (double v, int decimals)
{
    const double q = std::pow (10.0, decimals);
    v = std::round (v * q) / q;
    if (v == 0.0)
        v = 0.0; // no "-0"
    return juce::String (v, decimals);
}

juce::String withSign (double v, int decimals)
{
    auto t = fixed (v, decimals);
    return std::round (v * std::pow (10.0, decimals)) > 0 ? "+" + t : t;
}

using ToText = std::function<juce::String (double)>;

// Float parameter whose normalised value is r and whose value is the spec's mapping of r.
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name, const juce::String& unit,
                                                      ToText toText)
{
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); });

    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (unit)
                          .withStringFromValueFunction ([toText] (float v, int) { return toText (double (v)); })
                          .withValueFromStringFunction ([] (const juce::String& t) {
                              auto s = t.trim().toLowerCase();
                              double v = s.getDoubleValue();
                              if (s.endsWith ("khz") || (s.endsWithChar ('k') && ! s.endsWith ("db")))
                                  v *= 1000.0;
                              else if (s.endsWith (" s") || (s.endsWithChar ('s') && ! s.endsWith ("ms")))
                                  v *= 1000.0; // seconds typed for a time parameter
                              return float (juce::jlimit (P::minValue, P::maxValue, v));
                          });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (P::defaultValue),
                                                        attributes);
}
} // namespace

namespace text
{
    juce::String decibels (double v) { return withSign (v, 1) + " dB"; }
    juce::String percent (double v) { return fixed (v, 1) + " %"; }
    juce::String attackMs (double v) { return fixed (v, 1) + " ms"; }
    juce::String releaseMs (double v) { return v >= 1000.0 ? fixed (v / 1000.0, 3) + " s" : fixed (v, 2) + " ms"; }
    juce::String toneValue (double v) { return withSign (std::round (v * 2.0) / 2.0, 1); }
    juce::String hertz (double v) { return fixed (v, 0) + " Hz"; }
} // namespace text

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    namespace pm = dd::param;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeFloat<pm::Input> (ids::input, "Input", "dB", text::decibels));
    layout.add (makeFloat<pm::DirectLevel> (ids::direct, "Direct Level", "%", text::percent));
    layout.add (makeFloat<pm::Drive> (ids::drive, "Drive", "%", text::percent));
    layout.add (makeFloat<pm::DistLevel> (ids::distLevel, "Distortion Level", "%", text::percent));
    layout.add (makeFloat<pm::Attack> (ids::attack, "Attack", "ms", text::attackMs));
    layout.add (makeFloat<pm::Release> (ids::release, "Release", "ms", text::releaseMs));
    layout.add (makeFloat<pm::Bass> (ids::bass, "Bass", "", text::toneValue));
    layout.add (makeFloat<pm::Mid> (ids::mid, "Mid", "", text::toneValue));
    layout.add (makeFloat<pm::MidFreq> (ids::midFreq, "Mid Freq", "Hz", text::hertz));
    layout.add (makeFloat<pm::Treble> (ids::treble, "Treble", "", text::toneValue));
    layout.add (makeFloat<pm::LowPass> (ids::lowPass, "Low-pass", "Hz", text::hertz));
    layout.add (makeFloat<pm::Output> (ids::output, "Output", "dB", text::decibels));

    auto bypassAttr = juce::AudioParameterBoolAttributes().withStringFromValueFunction (
        [] (bool v, int) { return v ? juce::String ("On") : juce::String ("Off"); });
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::bypass, 1 }, "Bypass", false, bypassAttr));
    return layout;
}

} // namespace craft::ddp
