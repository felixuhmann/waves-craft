#include "ParameterLayout.h"

namespace craft::multiband
{

juce::String ids::band (int b, BandParam p)
{
    static const char* const suffix[] = { "Gain", "Range", "Threshold", "Attack", "Release", "Solo", "Bypass" };
    return "b" + juce::String (b + 1) + suffix[int (p)];
}

namespace
{
enum class Kind { db, hz, q, knee, ms };

Kind kindOf (const juce::String& id)
{
    if (id.startsWith ("xover"))
        return id == ids::xoverQ ? Kind::q : Kind::hz;
    if (id == ids::knee)
        return Kind::knee;
    if (id.endsWith ("Attack") || id.endsWith ("Release"))
        return Kind::ms;
    return Kind::db;
}
} // namespace

juce::String formatValue (const juce::String& id, double v)
{
    switch (kindOf (id))
    {
        case Kind::hz:   return param::formatHz (v);
        case Kind::q:    return param::formatQ (v);
        case Kind::knee: return param::formatKnee (v);
        case Kind::ms:   return param::formatMs (v);
        case Kind::db:   break;
    }
    return param::formatDb (v);
}

namespace
{
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name, const juce::String& unit,
                                                      double defaultValue)
{
    // The host-normalised value is the spec's r; mapping and quantisation live in convertFrom0to1.
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
                              if (paramId == ids::knee)
                              {
                                  if (t.startsWithIgnoreCase ("s")) return 0.0f;
                                  if (t.startsWithIgnoreCase ("h")) return 0.75f;
                              }
                              return float (param::snap<P> (t.getDoubleValue()));
                          });

    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range,
                                                        float (param::snap<P> (defaultValue)), attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id, const juce::String& name, bool def,
                                                    const juce::String& offText, const juce::String& onText)
{
    auto attributes = juce::AudioParameterBoolAttributes()
                          .withStringFromValueFunction ([offText, onText] (bool v, int) { return v ? onText : offText; })
                          .withValueFromStringFunction ([onText] (const juce::String& t) {
                              return t.trim().equalsIgnoreCase (onText) || t.trim() == "1";
                          });
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def, attributes);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using BP = ids::BandParam;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (makeFloat<param::OutputGain> (ids::outputGain, "Output Gain", "dB", param::defaultOutputGain));
    layout.add (makeFloat<param::Crossover> (ids::xoverLow, "Low Crossover", "Hz", param::defaultCrossover[0]));
    layout.add (makeFloat<param::Crossover> (ids::xoverMid, "Mid Crossover", "Hz", param::defaultCrossover[1]));
    layout.add (makeFloat<param::Crossover> (ids::xoverHigh, "High Crossover", "Hz", param::defaultCrossover[2]));
    layout.add (makeFloat<param::CrossoverQ> (ids::xoverQ, "Crossover Q", "", param::defaultQ));
    layout.add (makeFloat<param::Knee> (ids::knee, "Knee", "", param::defaultKnee));
    layout.add (makeBool (ids::autoRelease, "Release Mode", true, "Manual", "Auto"));
    layout.add (makeBool (ids::opto, "Behavior", false, "Electro", "Opto"));

    for (int b = 0; b < numBands; ++b)
    {
        const juce::String n = juce::String ("Band ") + juce::String (b + 1) + " ";
        layout.add (makeFloat<param::BandGain> (ids::band (b, BP::gain), n + "Gain", "dB", param::defaultBandGain));
        layout.add (makeFloat<param::BandRange> (ids::band (b, BP::range), n + "Range", "dB", param::defaultBandRange));
        layout.add (makeFloat<param::BandThreshold> (ids::band (b, BP::threshold), n + "Threshold", "dB",
                                                     param::defaultThreshold[size_t (b)]));
        layout.add (makeFloat<param::Attack> (ids::band (b, BP::attack), n + "Attack", "ms", param::defaultAttack[size_t (b)]));
        layout.add (makeFloat<param::Release> (ids::band (b, BP::release), n + "Release", "ms", param::defaultRelease[size_t (b)]));
        layout.add (makeBool (ids::band (b, BP::solo), n + "Solo", false, "Off", "On"));
        layout.add (makeBool (ids::band (b, BP::bypass), n + "Bypass", false, "Off", "On"));
    }
    layout.add (makeBool (ids::bypass, "Bypass", false, "Off", "On"));
    return layout;
}

} // namespace craft::multiband
