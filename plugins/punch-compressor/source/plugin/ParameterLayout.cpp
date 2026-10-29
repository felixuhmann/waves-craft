#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::punch
{

juce::String formatValue (const juce::String& id, double v)
{
    if (id == ids::ratio) return param::formatRatio (v);
    if (id == ids::attack || id == ids::release) return param::formatMs (v);
    if (id == ids::note) return param::note (int (std::lround (v))).name;
    if (id == ids::tempo) return param::formatTempo (v);
    return param::formatTenth (v);
}

namespace
{
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name, const juce::String& unit)
{
    // The host-normalised value is the spec's r; the mapping (and the quantisation to the displayed resolution)
    // is applied in convertFrom0to1, so host automation, the editor and the engine all see the same values.
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (param::snap<P> (v)); });

    const juce::String paramId (id);
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (unit)
                          .withStringFromValueFunction ([paramId] (float v, int) { return formatValue (paramId, v); })
                          .withValueFromStringFunction ([paramId] (const juce::String& text) {
                              const auto t = text.trim();
                              if (paramId == ids::tempo && (t.isEmpty() || t.startsWithIgnoreCase ("off")))
                                  return float (param::Tempo::minValue);
                              if (paramId == ids::note)
                              {
                                  for (int k = 0; k <= 40; ++k)
                                      if (t.equalsIgnoreCase (param::note (k).name))
                                          return float (k);
                              }
                              return float (param::snap<P> (t.getDoubleValue()));
                          });

    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (P::defaultValue),
                                                        attributes);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const char* id, const juce::String& name, const juce::StringArray& items,
                                                        int defaultIndex)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, defaultIndex);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (makeFloat<param::Threshold> (ids::threshold, "Threshold", "dB"));
    layout.add (makeFloat<param::Ratio> (ids::ratio, "Ratio", ""));
    layout.add (makeFloat<param::Mix> (ids::mix, "Mix", "%"));
    layout.add (makeFloat<param::Attack> (ids::attack, "Attack", "ms"));
    layout.add (makeFloat<param::Punch> (ids::punch, "Punch", ""));
    layout.add (makeFloat<param::Release> (ids::release, "Release", "ms"));
    // order of the spec: BPM (r < 0.25), Host, ms (r >= 0.75)
    layout.add (makeChoice (ids::sync, "Release Sync", { "BPM", "Host", "ms" }, 2));
    layout.add (makeFloat<param::Note> (ids::note, "Release Note", ""));
    layout.add (makeChoice (ids::analog, "Analog", { "Off", "1", "2", "3", "4" }, 2));
    layout.add (makeFloat<param::Output> (ids::output, "Output", "dB"));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ids::limiter, 1 }, "Limiter", false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction (
            [] (bool v, int) { return v ? juce::String ("Limiter") : juce::String ("Clipper"); })));
    layout.add (makeChoice (ids::meter, "Meter", { "In", "GR", "Out" }, 1));
    layout.add (makeFloat<param::Tempo> (ids::tempo, "Tempo", "BPM"));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::bypass, 1 }, "Bypass", false));
    return layout;
}

} // namespace craft::punch
