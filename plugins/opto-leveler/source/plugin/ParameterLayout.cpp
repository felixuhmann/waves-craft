#include "ParameterLayout.h"

#include "dsp/Laws.h"

namespace craft::opto_leveler
{

namespace
{
juce::AudioParameterFloatAttributes textAttributes (int decimals, const char* suffix)
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([decimals, suffix] (float v, int) {
            return juce::String (dsp::formatFixed (double (v), decimals, suffix));
        })
        .withValueFromStringFunction ([] (const juce::String& text) {
            return float (dsp::parseNumber (text.toStdString(), 0.0));
        });
}

// Gain / Peak Reduction: display units 0 ... 100 with the host taper of T0 (normalised r <-> display) and
// 0.01 display resolution.
std::unique_ptr<juce::AudioParameterFloat> makeTapered (const char* id, const juce::String& name, dsp::Taper taper,
                                                        float defaultValue)
{
    juce::NormalisableRange<float> range (
        0.0f, 100.0f,
        [taper] (float, float, float r) { return float (dsp::snapTo (taper.toDisplay (double (r)), 0.01)); },
        [taper] (float, float, float v) { return float (taper.toNormalised (double (v))); },
        [] (float, float, float v) { return juce::jlimit (0.0f, 100.0f, float (dsp::snapTo (double (v), 0.01))); });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, defaultValue,
                                                        textAttributes (2, ""));
}

std::unique_ptr<juce::AudioParameterFloat> makeLinear (const char* id, const juce::String& name, float lo, float hi,
                                                       float step, float defaultValue, int decimals, const char* suffix)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                        juce::NormalisableRange<float> (lo, hi, step), defaultValue,
                                                        textAttributes (decimals, suffix));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (makeTapered (ids::gain, "Gain", dsp::gainTaper(), 40.0f));
    layout.add (makeTapered (ids::peakReduction, "Peak Reduction", dsp::peakReductionTaper(), 46.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::mode, 1 }, "Mode",
                                                              juce::StringArray { "Compress", "Limit" }, 0));
    layout.add (makeLinear (ids::hiFreq, "Hi Freq", 0.0f, 100.0f, 0.1f, 50.0f, 1, ""));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::analog, 1 }, "Analog",
                                                              juce::StringArray { "Off", "50 Hz", "60 Hz" }, 0));
    layout.add (makeLinear (ids::mix, "Mix", 0.0f, 100.0f, 0.1f, 100.0f, 1, " %"));
    layout.add (makeLinear (ids::trim, "Trim", -18.0f, 18.0f, 0.1f, 0.0f, 1, " dB"));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ids::autoMakeup, 1 }, "Auto Makeup", false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return v ? "On" : "Off"; })));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::vu, 1 }, "VU Display",
                                                              juce::StringArray { "In", "GR", "Out" }, 1));
    return layout;
}

} // namespace craft::opto_leveler
