#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::levelrider
{

namespace pm = dsp::param;

juce::String formatValue (const juce::String& id, double v, bool withUnit)
{
    if (id == ids::musicSens && v <= pm::MusicSens::minValue - 0.05)
        return "Off";
    double x = std::round (v * 10.0) / 10.0;
    if (x == 0.0)
        x = 0.0; // avoid "-0.0"
    auto t = juce::String (x, 1);
    if (x > 0.0 && id != ids::target && id != ids::spill)
        t = "+" + t;
    if (withUnit && id != ids::idle)
        t << " dB";
    return t;
}

namespace
{
// The host-normalised value is the spec's r; the section 2 mapping and 0.1 dB quantisation are applied by
// convertFrom0to1, so every value reported back is a step value.
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name, bool automatable = true)
{
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (pm::snap<P> (v)); });

    const juce::String paramId (id);
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (paramId == ids::idle ? "" : "dB")
                          .withAutomatable (automatable)
                          .withStringFromValueFunction ([paramId] (float v, int) { return formatValue (paramId, v); })
                          .withValueFromStringFunction ([] (const juce::String& text) {
                              const auto t = text.trim().toLowerCase().removeCharacters (" db+");
                              return float (pm::snap<P> (t.getDoubleValue()));
                          });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (P::defaultValue),
                                                        attributes);
}

constexpr float offValue = -12.1f; // physical value of Music Sensitivity "Off"

// Music Sensitivity: r = 0 is "Off", any r > 0 is -12 + 24 r dB. The physical value of "Off" is -12.1 so that
// Off and -12.0 dB stay distinct; the processor decides on the normalised value (section 2).
std::unique_ptr<juce::AudioParameterFloat> makeMusicSens()
{

    juce::NormalisableRange<float> range (
        offValue, 12.0f,
        [] (float, float, float r) { return r <= 0.0f ? offValue : float (pm::MusicSens::fromNorm (r)); },
        [] (float, float, float v) {
            if (v <= -12.05f)
                return 0.0f;
            return std::max (0.001f, float (pm::MusicSens::toNorm (v))); // -12.0 dB stays just above Off
        },
        [] (float, float, float v) { return v <= -12.05f ? offValue : float (pm::snap<pm::MusicSens> (v)); });

    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel ("dB")
                          .withStringFromValueFunction ([] (float v, int) { return formatValue (ids::musicSens, v); })
                          .withValueFromStringFunction ([] (const juce::String& text) {
                              const auto t = text.trim().toLowerCase();
                              if (t.startsWith ("off"))
                                  return offValue;
                              return float (pm::snap<pm::MusicSens> (t.removeCharacters (" db+").getDoubleValue()));
                          });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ids::musicSens, 1 }, "Music Sensitivity", range,
                                                        float (pm::MusicSens::defaultValue), attributes);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (bool live)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (makeFloat<pm::Target> (ids::target, "Target"));
    layout.add (makeFloat<pm::VocalSens> (ids::vocalSens, "Vocal Sensitivity"));
    layout.add (makeMusicSens());
    layout.add (makeFloat<pm::RangeMax> (ids::rangeMax, "Range Max"));
    layout.add (makeFloat<pm::RangeMin> (ids::rangeMin, "Range Min"));
    layout.add (makeFloat<pm::Idle> (ids::idle, "Idle"));
    layout.add (makeFloat<pm::Rider> (ids::rider, "Rider"));
    layout.add (makeFloat<pm::Output> (ids::output, "Output"));
    // two-state switch: r < 0.5 selects Slow (section 2)
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::attack, 1 }, "Attack",
                                                              juce::StringArray { "Slow", "Fast" }, 0));
    if (live)
        layout.add (makeFloat<pm::Spill> (ids::spill, "Spill"));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::bypass, 1 }, "Bypass", false));
    return layout;
}

} // namespace craft::levelrider
