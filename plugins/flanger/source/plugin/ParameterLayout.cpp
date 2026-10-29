#include "ParameterLayout.h"

#include "dsp/Format.h"
#include "dsp/Parameters.h"

namespace craft::flanger
{

juce::String unitFor (const juce::String& id, bool tempoSync)
{
    if (id == ids::delay) return "ms";
    if (id == ids::depth || id == ids::mix || id == ids::feedback) return "%";
    if (id == ids::rate) return tempoSync ? "x/Beat" : "Hz";
    if (id == ids::gain) return "dB";
    if (id == ids::stereo) return juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); // degree sign
    return {};
}

namespace
{
using Q = dsp::Quantity;

template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name, const juce::String& label,
                                                      Q quantity, std::function<juce::String (float)> suffix = {})
{
    // The normalised value r is what the host sees; the spec 2 mapping and the internal quantisation are
    // applied by convertFrom0to1, so every value reported back is a step value.
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (dsp::param::snap<P> (v)); });

    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (label)
                          .withStringFromValueFunction ([quantity, suffix] (float v, int) {
                              juce::String s (dsp::formatNumber (quantity, double (v)));
                              return suffix ? s + suffix (v) : s;
                          })
                          .withValueFromStringFunction ([] (const juce::String& text) {
                              auto t = text.trim().toLowerCase().retainCharacters ("0123456789.-+k");
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

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id, const juce::String& name, bool def,
                                                    const juce::String& offText, const juce::String& onText)
{
    auto attributes = juce::AudioParameterBoolAttributes().withStringFromValueFunction (
        [offText, onText] (bool v, int) { return v ? onText : offText; });
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def, attributes);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (dsp::Variant variant,
                                                                          std::shared_ptr<std::atomic<bool>> sync)
{
    namespace pm = dsp::param;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeFloat<pm::Delay> (ids::delay, "Delay", "ms", Q::Delay));
    layout.add (makeFloat<pm::Depth> (ids::depth, "Depth", "%", Q::Percent));
    layout.add (makeFloat<pm::Rate> (ids::rate, "Rate", "", Q::Rate, [sync] (float) {
        return juce::String (sync != nullptr && sync->load() ? " x/Beat" : " Hz");
    }));
    layout.add (makeFloat<pm::Mix> (ids::mix, "Mix", "%", Q::Percent));
    layout.add (makeFloat<pm::Feedback> (ids::feedback, "Feedback", "%", Q::Percent));
    layout.add (makeFloat<pm::Gain> (ids::gain, "Output Gain", "dB", Q::Gain));
    if (variant != dsp::Variant::Mono)
        layout.add (makeFloat<pm::Stereo> (ids::stereo, "Stereo", "deg", Q::Stereo));
    layout.add (makeFloat<pm::FilterFreq> (ids::filterFreq, "Filter Freq", "Hz", Q::FilterFreq));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::filterType, 1 }, "Filter Type",
                                                              juce::StringArray { "Low-pass", "High-pass" }, 1));
    layout.add (makeBool (ids::filterOn, "Filter On", false, "Off", "On"));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::waveform, 1 }, "Waveform",
                                                              juce::StringArray { "Sine", "Triangle" }, 0));
    layout.add (makeBool (ids::wetInvert, "Wet Polarity", false, "+", "-"));
    layout.add (makeBool (ids::feedbackInvert, "Feedback Polarity", false, "+", "-"));
    layout.add (makeBool (ids::tape, "Tape", false, "Off", "On"));
    layout.add (makeBool (ids::stop, "Stop", false, "Off", "On"));
    layout.add (makeBool (ids::link, "Link", false, "Off", "On"));
    layout.add (makeBool (ids::tempoSync, "Tempo Sync", false, "Off", "On"));
    layout.add (makeBool (ids::bypass, "Bypass", false, "Off", "On"));
    return layout;
}

} // namespace craft::flanger
