#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::consolee
{

namespace pm = dsp::param;

namespace
{
struct ContinuousDef
{
    const char* id;
    const char* name;
    double defaultR;
};

const pm::Taper* taperFor (const juce::String& id)
{
    if (id == ids::inputTrim) return &pm::inputTrim;
    if (id == ids::outputFader) return &pm::outputFader;
    if (id == ids::compThreshold) return &pm::compThreshold;
    if (id == ids::compRelease) return &pm::compRelease;
    if (id == ids::expThreshold) return &pm::expThreshold;
    if (id == ids::expRange) return &pm::expRange;
    if (id == ids::expRelease) return &pm::expRelease;
    if (id == ids::lfGain || id == ids::lmfGain || id == ids::hmfGain || id == ids::hfGain) return &pm::bandGain;
    if (id == ids::lfFreq) return &pm::lfFreq;
    if (id == ids::lmfFreq) return &pm::lmfFreq;
    if (id == ids::hmfFreq) return &pm::hmfFreq;
    if (id == ids::hfFreq) return &pm::hfFreq;
    if (id == ids::lmfQ || id == ids::hmfQ) return &pm::midQ;
    return nullptr;
}

int decimalsFor (const juce::String& id)
{
    if (id == ids::compRelease || id == ids::expRelease) return 2;
    if (id == ids::lmfFreq || id == ids::hmfFreq || id == ids::hfFreq || id == ids::lmfQ || id == ids::hmfQ) return 2;
    if (id == ids::lfFreq || id == ids::hp) return 0;
    return 1;
}

juce::String fixed (double v, int decimals)
{
    const double q = std::pow (10.0, decimals);
    v = std::round (v * q) / q;
    if (v == 0.0)
        v = 0.0; // no "-0.0"
    return juce::String (v, decimals);
}
} // namespace

juce::String formatValue (const juce::String& id, double r)
{
    if (id == ids::compRatio)
    {
        const double R = pm::ratioValue (r);
        if (std::isinf (R))
            return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")) + ":1";
        return fixed (R, 1) + ":1";
    }
    if (id == ids::hp)
        return r <= 0.0 ? juce::String ("Off") : fixed (pm::hpLabel (r), 0);
    if (id == ids::lp)
        return r <= 0.0 ? juce::String ("Off") : fixed (pm::lpLabel (r), 1);
    if (const auto* t = taperFor (id))
        return fixed ((*t) (r), decimalsFor (id));
    return fixed (r, 2);
}

juce::String unitFor (const juce::String& id, double r)
{
    if (id == ids::hp) return r <= 0.0 ? "" : "Hz";
    if (id == ids::lp) return r <= 0.0 ? "" : "kHz";
    if (id == ids::lfFreq) return "Hz";
    if (id == ids::lmfFreq || id == ids::hmfFreq || id == ids::hfFreq) return "kHz";
    if (id == ids::compRelease || id == ids::expRelease) return "s";
    if (id == ids::lmfQ || id == ids::hmfQ || id == ids::compRatio) return "";
    return "dB";
}

double parseValue (const juce::String& id, const juce::String& text, double currentR)
{
    auto t = text.trim().toLowerCase();
    if (id == ids::hp || id == ids::lp)
        if (t.startsWith ("off"))
            return 0.0;
    if (id == ids::compRatio)
    {
        if (t.startsWith ("inf") || t.startsWithChar (juce::juce_wchar (0x221e)))
            return 1.0;
        return pm::ratioToNorm (t.upToFirstOccurrenceOf (":", false, false).getDoubleValue());
    }
    const auto numeric = t.retainCharacters ("0123456789.-+");
    if (numeric.isEmpty())
        return currentR;
    double v = numeric.getDoubleValue();
    const bool khz = t.contains ("k");
    if (id == ids::hp)
    {
        if (khz) v *= 1000.0;
        if (v <= 20.0) return juce::jlimit (0.001, 0.2, (v - 17.0) / 15.0);
        return pm::Taper { { 0.0, 20.0, 120.0, 300.0, 350.0 } }.toNorm (v);
    }
    if (id == ids::lp)
    {
        if (! khz && v > 100.0) v /= 1000.0;
        if (v >= 12.0) return juce::jlimit (0.001, 0.2, (20.0 - v) / 40.0);
        return pm::Taper { { 20.0, 12.0, 5.0, 3.5, 3.0 } }.toNorm (v);
    }
    if (id == ids::lfFreq && khz) v *= 1000.0;
    if ((id == ids::lmfFreq || id == ids::hmfFreq || id == ids::hfFreq) && ! khz && v > 100.0) v /= 1000.0;
    if (const auto* tp = taperFor (id))
        return tp->toNorm (v);
    return juce::jlimit (0.0, 1.0, v);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    namespace d = pm::defaults;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const juce::String& name, double defaultR) {
        const juce::String pid (id);
        auto attributes = juce::AudioParameterFloatAttributes()
                              .withLabel (unitFor (pid, defaultR))
                              .withStringFromValueFunction ([pid] (float v, int) {
                                  const auto u = unitFor (pid, v);
                                  return u.isEmpty() ? formatValue (pid, v) : formatValue (pid, v) + " " + u;
                              })
                              .withValueFromStringFunction ([pid] (const juce::String& text) { return float (parseValue (pid, text, 0.5)); });
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f),
                                                                 float (defaultR), attributes));
    };
    auto addBool = [&layout] (const char* id, const juce::String& name, bool def, const char* off, const char* on) {
        const juce::String a (off), b (on);
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id, 1 }, name, def,
            juce::AudioParameterBoolAttributes().withStringFromValueFunction ([a, b] (bool v, int) { return v ? b : a; })));
    };

    // I/O
    addFloat (ids::inputTrim, "Input Trim", d::inputTrim);
    addFloat (ids::outputFader, "Output Fader", d::outputFader);
    addBool (ids::polarity, "Polarity", false, "Normal", "Inverted");
    addBool (ids::analogue, "Analogue", false, "Off", "On");
    addBool (ids::meterOutput, "Meter", false, "Input", "Output");
    // filters
    addFloat (ids::hp, "HP Freq", d::hp);
    addFloat (ids::lp, "LP Freq", d::lp);
    addBool (ids::split, "Filter Split", false, "Off", "On");
    // EQ
    addFloat (ids::lfGain, "LF Gain", d::gain);
    addFloat (ids::lfFreq, "LF Freq", d::lfFreq);
    addBool (ids::lfBell, "LF Bell", false, "Shelf", "Bell");
    addFloat (ids::lmfGain, "LMF Gain", d::gain);
    addFloat (ids::lmfFreq, "LMF Freq", d::lmfFreq);
    addFloat (ids::lmfQ, "LMF Q", d::lmfQ);
    addFloat (ids::hmfGain, "HMF Gain", d::gain);
    addFloat (ids::hmfFreq, "HMF Freq", d::hmfFreq);
    addFloat (ids::hmfQ, "HMF Q", d::hmfQ);
    addFloat (ids::hfGain, "HF Gain", d::gain);
    addFloat (ids::hfFreq, "HF Freq", d::hfFreq);
    addBool (ids::hfBell, "HF Bell", false, "Shelf", "Bell");
    addBool (ids::eqBypass, "EQ Bypass", false, "In", "Bypassed");
    addBool (ids::eqToSidechain, "EQ to Sidechain", false, "Off", "On");
    // dynamics
    addFloat (ids::compRatio, "Comp Ratio", d::compRatio);
    addFloat (ids::compThreshold, "Comp Threshold", d::compThreshold);
    addFloat (ids::compRelease, "Comp Release", d::compRelease);
    addBool (ids::compFast, "Comp Fast Attack", false, "Off", "On");
    addFloat (ids::expRange, "Exp Range", d::expRange);
    addFloat (ids::expThreshold, "Exp Threshold", d::expThreshold);
    addFloat (ids::expRelease, "Exp Release", d::expRelease);
    addBool (ids::expFast, "Exp Fast Attack", false, "Off", "On");
    addBool (ids::gate, "Exp/Gate", false, "Expander", "Gate");
    addBool (ids::dynBypass, "Dynamics Bypass", false, "In", "Bypassed");
    addBool (ids::channelOut, "Dynamics Post-EQ", false, "Off", "On");

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::bypass, 1 }, "Bypass", false));
    return layout;
}

} // namespace craft::consolee
