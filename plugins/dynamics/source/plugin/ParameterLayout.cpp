#include "ParameterLayout.h"

namespace craft::dynamics
{

dsp::Quantity quantityFor (const juce::String& id)
{
    using Q = dsp::Quantity;
    if (id == ids::gateOpen || id == ids::gateClose) return Q::GateThreshold;
    if (id == ids::ratio) return Q::Ratio;
    if (id == ids::compAttack || id == ids::compRelease || id == ids::gateAttack || id == ids::gateRelease || id == ids::hold)
        return Q::Time;
    if (id == ids::pdr) return Q::Pdr;
    if (id == ids::floor) return Q::Floor;
    if (id == ids::filterFreq) return Q::Frequency;
    if (id == ids::filterQ) return Q::Q;
    return Q::Decibel;
}

namespace
{
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name)
{
    const auto quantity = quantityFor (id);
    // The host sees the spec 2 normalised value r; mapping and quantisation live in convertFrom0to1.
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); },
        [] (float, float, float v) { return float (dsp::param::snap<P> (v)); });

    auto attributes = juce::AudioParameterFloatAttributes()
                          .withStringFromValueFunction ([quantity] (float v, int) {
                              return juce::String (dsp::formatDisplay (quantity, double (v)));
                          })
                          .withValueFromStringFunction ([quantity] (const juce::String& text) {
                              const double v = dsp::parseDisplay (quantity, text.toStdString());
                              return float (dsp::param::snap<P> (dsp::clampTo (v, P::minValue, P::maxValue)));
                          });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (P::defaultValue), attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id, const juce::String& name, bool def, const juce::String& offText = "Off",
                                                    const juce::String& onText = "On")
{
    auto attributes = juce::AudioParameterBoolAttributes().withStringFromValueFunction (
        [offText, onText] (bool v, int) { return v ? onText : offText; });
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def, attributes);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const char* id, const juce::String& name, const juce::StringArray& items, int def = 0)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (dsp::Variant variant)
{
    namespace pm = dsp::param;
    const auto info = dsp::variantInfo (variant);
    const juce::StringArray eqModes { "Wideband", "Sidechain", "Split" };
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    if (info.comp)
    {
        layout.add (makeFloat<pm::CompThreshold> (ids::compThreshold, "Comp Threshold"));
        layout.add (makeFloat<pm::Ratio> (ids::ratio, "Ratio"));
        layout.add (makeFloat<pm::Attack> (ids::compAttack, "Comp Attack"));
        layout.add (makeFloat<pm::CompRelease> (ids::compRelease, "Comp Release"));
        layout.add (makeFloat<pm::Pdr> (ids::pdr, "PDR"));
        layout.add (makeFloat<pm::Makeup> (ids::makeup, "Makeup"));
        layout.add (makeChoice (ids::levelRef, "Level Reference", { "LowRef", "PeakRef" }));
        layout.add (makeBool (ids::compBypass, "Comp Bypass", false));
        if (info.filter)
            layout.add (makeChoice (ids::compEq, "Comp EQ Mode", eqModes));
    }
    if (info.gate)
    {
        layout.add (makeChoice (ids::gateMode, "Gate Mode", { "Gate", "Expander" }));
        layout.add (makeFloat<pm::GateThreshold> (ids::gateOpen, "Gate Open"));
        layout.add (makeFloat<pm::GateThreshold> (ids::gateClose, "Gate Close"));
        layout.add (makeFloat<pm::Attack> (ids::gateAttack, "Gate Attack"));
        layout.add (makeFloat<pm::GateRelease> (ids::gateRelease, "Gate Release"));
        layout.add (makeFloat<pm::Hold> (ids::hold, "Hold"));
        layout.add (makeFloat<pm::Floor> (ids::floor, "Floor"));
        layout.add (makeBool (ids::gateBypass, "Gate Bypass", false));
        if (info.filter)
            layout.add (makeChoice (ids::gateEq, "Gate EQ Mode", eqModes));
    }
    if (info.filter)
    {
        layout.add (makeChoice (ids::filterType, "Filter Type", { "Low-pass", "Band-pass", "High-pass", "Band-reject" }));
        layout.add (makeFloat<pm::FilterFreq> (ids::filterFreq, "Filter Freq"));
        layout.add (makeFloat<pm::FilterQ> (ids::filterQ, "Filter Q"));
        layout.add (makeChoice (ids::monitor, "Monitor", { "Audio", "Sidechain", "Passive" }));
        layout.add (makeChoice (ids::keyMode, "Key Mode", { "Stereo", "L>R", "R>L" }));
        layout.add (makeBool (ids::lookahead, "LookAhead", false));
        layout.add (makeFloat<pm::InputGain> (ids::inputGainL, "Input Gain L"));
        layout.add (makeFloat<pm::InputGain> (ids::inputGainR, "Input Gain R"));
    }
    layout.add (makeFloat<pm::OutputGain> (ids::outputGain, "Output Gain"));
    if (info.comp && info.gate)
    {
        layout.add (makeBool (ids::linkThreshold, "Link Thresholds", false));
        layout.add (makeBool (ids::linkAttack, "Link Attacks", false));
        layout.add (makeBool (ids::linkRelease, "Link Releases", false));
    }
    layout.add (makeBool (ids::bypass, "Bypass", false));
    return layout;
}

} // namespace craft::dynamics
