#include "ParameterLayout.h"

namespace craft::consoleg
{

using dsp::TaperId;

const std::vector<ContinuousParam>& continuousParams()
{
    static const std::vector<ContinuousParam> list {
        { ids::compThreshold, "Comp Threshold", TaperId::compThreshold },
        { ids::compRatio, "Comp Ratio", TaperId::compRatio },
        { ids::compRelease, "Comp Release", TaperId::compRelease },
        { ids::expThreshold, "Exp Threshold", TaperId::expThreshold },
        { ids::expRange, "Exp Range", TaperId::expRange },
        { ids::expRelease, "Exp Release", TaperId::expRelease },
        { ids::hpFreq, "HP Freq", TaperId::hpFreq },
        { ids::lpFreq, "LP Freq", TaperId::lpFreq },
        { ids::lfGain, "LF Gain", TaperId::lfGain },
        { ids::lfFreq, "LF Freq", TaperId::lfFreq },
        { ids::lmfGain, "LMF Gain", TaperId::lmfGain },
        { ids::lmfFreq, "LMF Freq", TaperId::lmfFreq },
        { ids::lmfQ, "LMF Q", TaperId::lmfQ },
        { ids::hmfGain, "HMF Gain", TaperId::hmfGain },
        { ids::hmfFreq, "HMF Freq", TaperId::hmfFreq },
        { ids::hmfQ, "HMF Q", TaperId::hmfQ },
        { ids::hfGain, "HF Gain", TaperId::hfGain },
        { ids::hfFreq, "HF Freq", TaperId::hfFreq },
        { ids::inputTrim, "Input Trim", TaperId::inputTrim },
        { ids::outputGain, "Output", TaperId::outputGain },
    };
    return list;
}

namespace
{
double defaultR (const char* id)
{
    const auto d = dsp::Settings::defaults();
    const juce::String s (id);
    if (s == ids::compThreshold) return d.compThreshold;
    if (s == ids::compRatio) return d.compRatio;
    if (s == ids::compRelease) return d.compRelease;
    if (s == ids::expThreshold) return d.expThreshold;
    if (s == ids::expRange) return d.expRange;
    if (s == ids::expRelease) return d.expRelease;
    if (s == ids::hpFreq) return d.hpFreq;
    if (s == ids::lpFreq) return d.lpFreq;
    if (s == ids::lfGain) return d.lfGain;
    if (s == ids::lfFreq) return d.lfFreq;
    if (s == ids::lmfGain) return d.lmfGain;
    if (s == ids::lmfFreq) return d.lmfFreq;
    if (s == ids::lmfQ) return d.lmfQ;
    if (s == ids::hmfGain) return d.hmfGain;
    if (s == ids::hmfFreq) return d.hmfFreq;
    if (s == ids::hmfQ) return d.hmfQ;
    if (s == ids::hfGain) return d.hfGain;
    if (s == ids::hfFreq) return d.hfFreq;
    if (s == ids::inputTrim) return d.inputTrim;
    if (s == ids::outputGain) return d.outputGain;
    return 0.0;
}

// Parses typed text ("-10", "4 dB", "2.5k", "Off", "inf") into a displayed value of the taper.
double parseTyped (const dsp::Taper& taper, juce::String text)
{
    auto t = text.trim().toLowerCase();
    if (t.startsWith ("off") || t.startsWith ("out"))
        return std::numeric_limits<double>::quiet_NaN();
    if (t.contains ("inf"))
        return std::numeric_limits<double>::infinity();
    const bool kilo = t.containsChar ('k');
    double v = t.retainCharacters ("0123456789.-+").getDoubleValue();
    if (kilo || (taper.kiloDisplay && ! t.contains ("hz") && v < 100.0))
        v *= 1000.0;
    return v;
}

std::unique_ptr<juce::AudioParameterFloat> makeContinuous (const ContinuousParam& p)
{
    const auto& taper = dsp::Tapers::get()[p.taper];
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (taper.unit)
                          .withStringFromValueFunction ([&taper] (float r, int) { return juce::String (taper.format (r, false)); })
                          .withValueFromStringFunction ([&taper] (const juce::String& text) {
                              return float (taper.rForValue (parseTyped (taper, text)));
                          });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { p.id, 1 }, p.name,
                                                        juce::NormalisableRange<float> (0.0f, 1.0f), float (defaultR (p.id)),
                                                        attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id, const char* name, bool def = false)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& p : continuousParams())
        layout.add (makeContinuous (p));
    layout.add (makeBool (ids::compFast, "Comp Fast Attack"));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::expGate, 1 }, "Exp/Gate Mode",
                                                              juce::StringArray { "Expander", "Gate" }, 0));
    layout.add (makeBool (ids::expFast, "Exp Fast Attack"));
    layout.add (makeBool (ids::dynBypass, "Dynamics Bypass"));
    layout.add (makeBool (ids::dynPostEq, "Dynamics Post-EQ"));
    layout.add (makeBool (ids::filtersSidechain, "Filters to Sidechain"));
    layout.add (makeBool (ids::filtersSplit, "Filters Split"));
    layout.add (makeBool (ids::eqBypass, "EQ Bypass"));
    layout.add (makeBool (ids::lmfDiv3, "LMF /3"));
    layout.add (makeBool (ids::hmfX3, "HMF x3"));
    layout.add (makeBool (ids::analog, "Analog"));
    layout.add (makeBool (ids::polarity, "Polarity Invert"));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::meterSource, 1 }, "Meter Source",
                                                              juce::StringArray { "In", "Out" }, 1));
    return layout;
}

dsp::Settings readSettings (juce::AudioProcessorValueTreeState& st)
{
    auto r = [&st] (const char* id) {
        auto* p = st.getParameter (id);
        return p != nullptr ? double (p->getValue()) : 0.0;
    };
    auto b = [&st] (const char* id) {
        auto* p = st.getParameter (id);
        return p != nullptr && p->getValue() >= 0.5f;
    };
    dsp::Settings s;
    s.compThreshold = r (ids::compThreshold);
    s.compRatio = r (ids::compRatio);
    s.compRelease = r (ids::compRelease);
    s.compFast = b (ids::compFast);
    s.expThreshold = r (ids::expThreshold);
    s.expRange = r (ids::expRange);
    s.expRelease = r (ids::expRelease);
    s.expGate = b (ids::expGate);
    s.expFast = b (ids::expFast);
    s.dynBypass = b (ids::dynBypass);
    s.dynPostEq = b (ids::dynPostEq);
    s.filtersSidechain = b (ids::filtersSidechain);
    s.filtersSplit = b (ids::filtersSplit);
    s.eqBypass = b (ids::eqBypass);
    s.hpFreq = r (ids::hpFreq);
    s.lpFreq = r (ids::lpFreq);
    s.lfGain = r (ids::lfGain);
    s.lfFreq = r (ids::lfFreq);
    s.lmfGain = r (ids::lmfGain);
    s.lmfFreq = r (ids::lmfFreq);
    s.lmfQ = r (ids::lmfQ);
    s.lmfDiv3 = b (ids::lmfDiv3);
    s.hmfGain = r (ids::hmfGain);
    s.hmfFreq = r (ids::hmfFreq);
    s.hmfQ = r (ids::hmfQ);
    s.hmfX3 = b (ids::hmfX3);
    s.hfGain = r (ids::hfGain);
    s.hfFreq = r (ids::hfFreq);
    s.inputTrim = r (ids::inputTrim);
    s.outputGain = r (ids::outputGain);
    s.analog = b (ids::analog);
    s.polarity = b (ids::polarity);
    return s;
}

} // namespace craft::consoleg
