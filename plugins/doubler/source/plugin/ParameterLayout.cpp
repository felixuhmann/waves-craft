#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::doubler
{

namespace
{
bool endsWith (const juce::String& id, const char* suffix) { return id.startsWithChar ('v') && id.endsWith (suffix); }

juce::String routingName (bool b, int index)
{
    static const char* a[3] = { "L", "L+R", "L-R" };
    static const char* bn[3] = { "R", "R+L", "R-L" };
    return (b ? bn : a)[juce::jlimit (0, 2, index)];
}
} // namespace

juce::String formatValue (const juce::String& id, double v)
{
    auto fmt = [] (double x, int decimals) {
        const double q = std::pow (10.0, decimals);
        x = std::round (x * q) / q;
        if (x == 0.0)
            x = 0.0; // no "-0.0"
        return juce::String (x, decimals);
    };
    if (id == ids::routingA || id == ids::routingB)
        return routingName (id == ids::routingB, int (std::lround (v)));
    if (id == ids::fbHighPass || id == ids::eqLowFreq || id == ids::eqHighFreq)
        return fmt (v, 0);
    if (endsWith (id, ids::feedback) || endsWith (id, ids::detune) || endsWith (id, ids::depth))
    {
        const auto s = fmt (v, 0);
        return (v > 0.0 && ! endsWith (id, ids::feedback)) ? "+" + s : s;
    }
    if (id == ids::directPan || endsWith (id, ids::pan) || id == ids::eqLowGain || id == ids::eqHighGain)
        return (v > 0.0 && id != ids::directPan && ! endsWith (id, ids::pan) ? "+" : "") + fmt (v, 1);
    return fmt (v, 1);
}

namespace
{
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name,
                                                      const juce::String& unit, double defaultValue)
{
    // The host sees the normalised value r; the mapping of spec section 3 and the quantisation are
    // applied by convertFrom0to1, so every value reported back is a step value.
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (param::snap<P> (v)); });

    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (unit)
                          .withStringFromValueFunction ([id] (float v, int) { return formatValue (id, v); })
                          .withValueFromStringFunction ([id] (const juce::String& text) {
                              auto t = text.trim().toLowerCase();
                              if (id == ids::routingA || id == ids::routingB)
                              {
                                  if (t.contains ("+")) return 1.0f;
                                  if (t.contains ("-")) return 2.0f;
                                  if (t == "l" || t == "r") return 0.0f;
                                  return float (juce::jlimit (0, 2, t.getIntValue()));
                              }
                              double mult = 1.0;
                              if (t.endsWithChar ('k'))
                              {
                                  mult = 1000.0;
                                  t = t.dropLastCharacters (1);
                              }
                              return float (param::snap<P> (t.getDoubleValue() * mult));
                          });
    // the default is given as a physical value; store its normalised position
    auto p = std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range,
                                                          float (param::snap<P> (defaultValue)), attributes);
    return p;
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id, const juce::String& name, bool def,
                                                    const juce::String& offText, const juce::String& onText)
{
    auto attributes = juce::AudioParameterBoolAttributes().withStringFromValueFunction (
        [offText, onText] (bool v, int) { return v ? onText : offText; });
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def, attributes);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (int numVoices)
{
    namespace pm = param;
    const Settings d = defaultSettings (numVoices);
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeFloat<pm::MasterGain> (ids::master, "Master Gain", "dB", d.masterDb));
    layout.add (makeFloat<pm::Frequency> (ids::fbHighPass, "Feedback High-Pass", "Hz", d.fbHighPassHz));
    layout.add (makeBool (ids::modReset, "Modulation Reset", false, "-", "Reset"));
    layout.add (makeBool (ids::range, "Range", d.range20, "80 Hz", "20 Hz"));
    layout.add (makeBool (ids::align, "Align Direct", d.alignDirect, "No", "Yes"));
    layout.add (makeBool (ids::directOn, "Direct On", d.directOn, "Off", "On"));
    layout.add (makeFloat<pm::LevelGain> (ids::directGain, "Direct Gain", "dB", d.directGainDb));
    layout.add (makeFloat<pm::Pan> (ids::directPan, "Direct Pan", "deg", d.directPanDeg));
    layout.add (makeBool (ids::eqLowOn, "EQ Low On", d.eqLowOn, "Off", "On"));
    layout.add (makeFloat<pm::EqGain> (ids::eqLowGain, "EQ Low Gain", "dB", d.eqLowGainDb));
    layout.add (makeFloat<pm::Frequency> (ids::eqLowFreq, "EQ Low Freq", "Hz", d.eqLowFreqHz));
    layout.add (makeBool (ids::eqHighOn, "EQ High On", d.eqHighOn, "Off", "On"));
    layout.add (makeFloat<pm::EqGain> (ids::eqHighGain, "EQ High Gain", "dB", d.eqHighGainDb));
    layout.add (makeFloat<pm::Frequency> (ids::eqHighFreq, "EQ High Freq", "Hz", d.eqHighFreqHz));

    for (int n = 0; n < numVoices; ++n)
    {
        const auto& v = d.voices[size_t (n)];
        const juce::String name = "Voice " + juce::String (n + 1) + " ";
        layout.add (makeBool (ids::voice (n, ids::on), name + "On", v.on, "Off", "On"));
        layout.add (makeFloat<pm::LevelGain> (ids::voice (n, ids::gain), name + "Gain", "dB", v.gainDb));
        layout.add (makeFloat<pm::Pan> (ids::voice (n, ids::pan), name + "Pan", "deg", v.panDeg));
        layout.add (makeFloat<pm::DelayMs> (ids::voice (n, ids::delay), name + "Delay", "ms", v.delayMs));
        layout.add (makeFloat<pm::Feedback> (ids::voice (n, ids::feedback), name + "Feedback", "%", v.feedbackPct));
        layout.add (makeBool (ids::voice (n, ids::octave), name + "Octave", v.octave, "Off", "-1 Oct"));
        layout.add (makeFloat<pm::Detune> (ids::voice (n, ids::detune), name + "Detune", "cents", v.detuneCents));
        layout.add (makeFloat<pm::Depth> (ids::voice (n, ids::depth), name + "Mod Depth", "cents", v.depthCents));
        layout.add (makeFloat<pm::Rate> (ids::voice (n, ids::rate), name + "Mod Rate", "Hz", v.rateHz));
    }

    layout.add (makeFloat<pm::Routing> (ids::routingA, numVoices > 2 ? "Input Routing 1/3" : "Input Routing 1", "", 0.0));
    layout.add (makeFloat<pm::Routing> (ids::routingB, numVoices > 2 ? "Input Routing 2/4" : "Input Routing 2", "", 0.0));
    layout.add (makeBool (ids::bypass, "Bypass", false, "Off", "On"));
    return layout;
}

} // namespace craft::doubler
