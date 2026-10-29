#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft
{

namespace pm = cdl::param;

juce::String formatValue (const juce::String& id, double v)
{
    auto fmt = [] (double x, int decimals) {
        const double q = std::pow (10.0, decimals);
        x = std::round (x * q) / q;
        if (x == 0.0)
            x = 0.0; // no "-0.0"
        return juce::String (x, decimals);
    };
    if (id == ids::input || id == ids::output)
        return (v > 0.0 ? "+" : "") + fmt (v, 1);
    if (id == ids::delayMsL || id == ids::delayMsR)
        return fmt (v, 0);
    if (id == ids::hp)
        return v <= pm::FilterHz::minValue ? juce::String ("Off") : (v >= 1000.0 ? fmt (v / 1000.0, 2) + "k" : fmt (v, 0));
    if (id == ids::lp)
        return v >= pm::FilterHz::maxValue ? juce::String ("Off") : (v >= 1000.0 ? fmt (v / 1000.0, 2) + "k" : fmt (v, 0));
    if (id == ids::doublerDetune)
        return (v > 0.0 ? "+" : "") + fmt (v, 0);
    if (id == ids::phaserRate)
        return v < 0.01 ? fmt (v, 3) : fmt (v, 2);
    return fmt (v, 1); // percentages
}

namespace
{
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name, const juce::String& unit,
                                                      double defaultValue)
{
    // The host-normalised value is the spec's r; mapping and quantisation (spec section 2) are applied in
    // convertFrom0to1, so hosts and the editor only see step values.
    juce::NormalisableRange<float> range (
        float (P::fromNorm (0.0)), float (P::fromNorm (1.0)),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (P::snap (v)); });

    const juce::String paramId (id);
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (unit)
                          .withStringFromValueFunction ([paramId] (float v, int) { return formatValue (paramId, v); })
                          .withValueFromStringFunction ([paramId] (const juce::String& text) {
                              auto t = text.trim().toLowerCase();
                              if (t == "off")
                                  return float (paramId == ids::hp ? P::fromNorm (0.0) : P::fromNorm (1.0));
                              double mult = 1.0;
                              if (t.endsWithChar ('k'))
                              {
                                  mult = 1000.0;
                                  t = t.dropLastCharacters (1);
                              }
                              return float (P::snap (t.getDoubleValue() * mult));
                          });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (defaultValue), attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id, const juce::String& name, bool def,
                                                    const juce::String& offText = "Off", const juce::String& onText = "On")
{
    auto attributes = juce::AudioParameterBoolAttributes().withStringFromValueFunction (
        [offText, onText] (bool v, int) { return v ? onText : offText; });
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def, attributes);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const char* id, const juce::String& name, const juce::StringArray& items,
                                                        int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::StringArray notes;
    for (auto* n : pm::noteNames)
        notes.add (n);

    layout.add (makeFloat<pm::GainDb> (ids::input, "Input", "dB", 0.0));
    layout.add (makeFloat<pm::GainDb> (ids::output, "Output", "dB", 0.0));
    layout.add (makeChoice (ids::mode, "Mode", { "ms", "Host" }, 1));
    layout.add (makeFloat<pm::DelayMs> (ids::delayMsL, "Delay L ms", "ms", pm::DelayMs::defaultValue));
    layout.add (makeFloat<pm::DelayMs> (ids::delayMsR, "Delay R ms", "ms", pm::DelayMs::defaultValue));
    layout.add (makeChoice (ids::noteL, "Delay L note", notes, pm::noteDefaultIndex));
    layout.add (makeChoice (ids::noteR, "Delay R note", notes, pm::noteDefaultIndex));
    layout.add (makeFloat<pm::Percent> (ids::feedbackL, "Feedback L", "%", pm::feedbackDefault));
    layout.add (makeFloat<pm::Percent> (ids::feedbackR, "Feedback R", "%", pm::feedbackDefault));
    layout.add (makeBool (ids::delayLink, "Delay Link", true));
    layout.add (makeFloat<pm::Percent> (ids::dryWet, "Dry/Wet", "%", pm::dryWetDefault));
    layout.add (makeFloat<pm::FilterHz> (ids::hp, "HP", "Hz", pm::FilterHz::minValue));
    layout.add (makeFloat<pm::FilterHz> (ids::lp, "LP", "Hz", pm::FilterHz::maxValue));
    layout.add (makeBool (ids::filterLink, "HP/LP Link", false));
    layout.add (makeBool (ids::verbOn, "Verb On", false));
    layout.add (makeBool (ids::distOn, "Dist On", false));
    layout.add (makeBool (ids::doublerOn, "Doubler On", false));
    layout.add (makeBool (ids::phaserOn, "Phaser On", false));
    layout.add (makeFloat<pm::VerbSize> (ids::verbSize, "Verb Size", "%", pm::VerbSize::defaultValue));
    layout.add (makeFloat<pm::Percent> (ids::verbLevel, "Verb Level", "%", pm::levelDefault));
    layout.add (makeFloat<pm::Percent> (ids::distLevel, "Dist Level", "%", pm::levelDefault));
    layout.add (makeFloat<pm::Percent> (ids::distSend, "Dist Send", "%", pm::distSendDefault));
    layout.add (makeFloat<pm::Percent> (ids::distWidth, "Dist Width", "%", pm::distWidthDefault));
    layout.add (makeFloat<pm::Percent> (ids::doublerLevel, "Doubler Level", "%", pm::levelDefault));
    layout.add (makeFloat<pm::Detune> (ids::doublerDetune, "Doubler Detune", "cents", pm::Detune::defaultValue));
    layout.add (makeFloat<pm::Percent> (ids::phaserLevel, "Phaser Level", "%", pm::levelDefault));
    layout.add (makeFloat<pm::PhaserRate> (ids::phaserRate, "Phaser Rate", "Hz", pm::PhaserRate::defaultValue));
    layout.add (makeChoice (ids::monitor, "Monitor", { "L", "L+R", "R" }, 1));
    layout.add (makeBool (ids::bypass, "Bypass", false));
    return layout;
}

} // namespace craft
