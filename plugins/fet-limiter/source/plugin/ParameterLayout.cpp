#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::fet_limiter
{

namespace
{
using StringFn = std::function<juce::String (double)>;

// Continuous parameter whose host-normalised value r follows the spec 2 mapping P (fromNorm / toNorm / snap).
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name, double defaultValue,
                                                      StringFn toText)
{
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (P::snap (v)); });

    auto attributes = juce::AudioParameterFloatAttributes()
                          .withStringFromValueFunction ([toText] (float v, int) { return toText (double (v)); })
                          .withValueFromStringFunction ([defaultValue] (const juce::String& text) {
                              return float (P::snap (dsp::parseNumber (text.toStdString(), defaultValue)));
                          });

    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (defaultValue),
                                                        attributes);
}

// Stepped parameter whose value is an index 0..n-1; the host value -> index thresholds are those of spec 2
// (which differ slightly from JUCE's AudioParameterChoice rounding), the canonical host value of an index lies
// inside its band.
std::unique_ptr<juce::AudioParameterFloat> makeIndex (const char* id, const juce::String& name, const juce::StringArray& labels,
                                                      int defaultIndex, int (*fromNorm) (double), double (*toNorm) (int))
{
    const int n = labels.size();
    juce::NormalisableRange<float> range (
        0.0f, float (n - 1),
        [fromNorm] (float, float, float r) { return float (fromNorm (double (r))); },
        [toNorm] (float, float, float v) { return float (toNorm (juce::roundToInt (v))); },
        [n] (float, float, float v) { return float (juce::jlimit (0, n - 1, juce::roundToInt (v))); });
    range.interval = 1.0f;

    auto attributes = juce::AudioParameterFloatAttributes()
                          .withStringFromValueFunction ([labels] (float v, int) { return labels[juce::roundToInt (v)]; })
                          .withValueFromStringFunction ([labels, defaultIndex] (const juce::String& text) {
                              const auto t = text.trim();
                              for (int i = 0; i < labels.size(); ++i)
                                  if (labels[i].equalsIgnoreCase (t) || labels[i].upToFirstOccurrenceOf (" ", false, false).equalsIgnoreCase (t))
                                      return float (i);
                              return float (defaultIndex);
                          });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (defaultIndex),
                                                        attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id, const juce::String& name)
{
    return std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { id, 1 }, name, false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return v ? "On" : "Off"; }));
}

int meterIndex (double r) { return dsp::param::meterIndexFromNorm (r); }
double meterNorm (int i) { return juce::jlimit (0, 2, i) / 2.0; }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    namespace pm = dsp::param;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const StringFn level = [] (double v) { return juce::String (dsp::formatLevel (v)); };
    const StringFn time = [] (double v) { return juce::String (dsp::formatTime (v)); };

    layout.add (makeFloat<pm::Level> (ids::input, "Input", -30.0, level));
    layout.add (makeFloat<pm::Level> (ids::output, "Output", -18.0, level));
    layout.add (makeFloat<pm::Time> (ids::attack, "Attack", 3.0, time));
    layout.add (makeFloat<pm::Time> (ids::release, "Release", 4.0, time));
    layout.add (makeIndex (ids::ratio, "Ratio", { "20", "12", "8", "4", "All" }, 3, pm::ratioIndexFromNorm, pm::ratioNormFromIndex));
    layout.add (makeIndex (ids::analog, "Analog", { "50 Hz", "60 Hz", "Off" }, 2, pm::analogIndexFromNorm, pm::analogNormFromIndex));
    layout.add (makeIndex (ids::revision, "Revision", { "B", "A" }, 1, pm::revisionIndexFromNorm, pm::revisionNormFromIndex));
    layout.add (makeIndex (ids::meter, "Meter", { "GR", "In", "Out" }, 0, meterIndex, meterNorm));
    layout.add (makeBool (ids::compOff, "Comp Off"));
    layout.add (makeFloat<pm::Mix> (ids::mix, "Mix", 100.0, [] (double v) { return juce::String (dsp::formatPercent (v)); }));
    layout.add (makeFloat<pm::Trim> (ids::trim, "Trim", 0.0, [] (double v) { return juce::String (dsp::formatDb (v)); }));
    layout.add (makeBool (ids::autoMakeup, "Auto Makeup"));
    return layout;
}

} // namespace craft::fet_limiter
