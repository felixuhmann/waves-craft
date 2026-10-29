#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::hdelay
{

namespace
{
using Attr = juce::AudioParameterFloatAttributes;

// Parses "1.2k", "1.2 kHz", "437Hz", "375 ms", "-6 dB" ... into a plain number (k = x1000).
double parseNumber (const juce::String& text)
{
    auto t = text.trim().toLowerCase().removeCharacters (" ");
    for (auto unit : { "khz", "hz", "ms", "db", "%" })
        if (t.endsWith (unit))
        {
            const bool kilo = juce::String (unit) == "khz";
            t = t.dropLastCharacters (int (std::strlen (unit)));
            return t.getDoubleValue() * (kilo ? 1000.0 : 1.0);
        }
    if (t.endsWithChar ('k'))
        return t.dropLastCharacters (1).getDoubleValue() * 1000.0;
    return t.getDoubleValue();
}

// Float parameter whose normalised value is the spec's r, mapped by P::fromNorm / P::toNorm.
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> mapped (const char* id, const juce::String& name, const juce::String& unit,
                                                   std::function<std::string (double)> format, bool quantiseToInteger = false)
{
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [quantiseToInteger] (float, float, float v) {
            const double c = std::clamp (double (v), P::minValue, P::maxValue);
            return float (quantiseToInteger ? std::round (c) : c);
        });
    auto attributes = Attr()
                          .withLabel (unit)
                          .withStringFromValueFunction ([format] (float v, int) { return juce::String (format (v)); })
                          .withValueFromStringFunction ([] (const juce::String& text) {
                              return float (std::clamp (parseNumber (text), P::minValue, P::maxValue));
                          });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (P::defaultValue),
                                                        attributes);
}

// Filter parameter: the host value is r itself (0..1); "Off" at one end.
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> filter (const char* id, const juce::String& name, float defaultNorm, bool offAtTop)
{
    auto attributes = Attr()
                          .withLabel ("")
                          .withStringFromValueFunction ([] (float r, int) { return juce::String (param::formatFilter (P::fromNorm (r))); })
                          .withValueFromStringFunction ([offAtTop] (const juce::String& text) {
                              if (text.trim().equalsIgnoreCase ("off"))
                                  return offAtTop ? 1.0f : 0.0f;
                              const double r = param::Filter::toNorm (parseNumber (text));
                              return float (offAtTop ? std::min (r, param::LoPass::maxOnNorm) : std::max (r, param::HiPass::minOnNorm));
                          });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                        juce::NormalisableRange<float> (0.0f, 1.0f), defaultNorm, attributes);
}

std::unique_ptr<juce::AudioParameterBool> toggle (const char* id, const juce::String& name, const juce::String& offText,
                                                  const juce::String& onText, bool def = false)
{
    auto attributes = juce::AudioParameterBoolAttributes().withStringFromValueFunction (
        [offText, onText] (bool v, int) { return v ? onText : offText; });
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def, attributes);
}

template <size_t N>
juce::StringArray toStringArray (const std::array<const char*, N>& names)
{
    juce::StringArray a;
    for (auto* n : names)
        a.add (n);
    return a;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    namespace pm = param;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (mapped<pm::DelayMs> (ids::delayMs, "Delay", "", pm::formatMs, true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::delayNote, 1 }, "Delay Note",
                                                              toStringArray (pm::noteNames), pm::defaultNote));
    layout.add (mapped<pm::DelayMs> (ids::delayRMs, "Delay R", "", pm::formatMs, true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::delayRNote, 1 }, "Delay R Note",
                                                              toStringArray (pm::noteNames), pm::defaultNote));
    layout.add (toggle (ids::dual, "Dual", "Off", "On"));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::sync, 1 }, "Sync",
                                                              toStringArray (pm::syncNames), int (pm::Sync::Host)));
    layout.add (mapped<pm::Bpm> (ids::bpm, "BPM", "BPM", pm::formatBpm));
    layout.add (mapped<pm::Feedback> (ids::feedback, "Feedback", "%", pm::formatPercent));
    layout.add (toggle (ids::pingPong, "Ping Pong", "Off", "On"));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::pingPongStart, 1 }, "Ping Pong Start",
                                                              juce::StringArray { "Left", "Right" }, 0));
    layout.add (toggle (ids::phaseL, "Phase L", "+", "-"));
    layout.add (toggle (ids::phaseR, "Phase R", "+", "-"));
    layout.add (mapped<pm::Mix> (ids::mix, "Dry/Wet", "", pm::formatPercent));
    layout.add (mapped<pm::Output> (ids::output, "Output", "", pm::formatOutput));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::analog, 1 }, "Analog",
                                                              toStringArray (pm::analogNames), pm::defaultAnalog));
    layout.add (toggle (ids::lofi, "LoFi", "Off", "On"));
    layout.add (mapped<pm::ModRate> (ids::modRate, "Mod Rate", "", pm::formatRate));
    layout.add (mapped<pm::ModDepth> (ids::modDepth, "Mod Depth", "", pm::formatDepth));
    layout.add (filter<pm::HiPass> (ids::hiPass, "HiPass", 0.0f, false));
    layout.add (filter<pm::LoPass> (ids::loPass, "LoPass", 1.0f, true));
    layout.add (toggle (ids::bypass, "Bypass", "Off", "On"));
    return layout;
}

} // namespace craft::hdelay
