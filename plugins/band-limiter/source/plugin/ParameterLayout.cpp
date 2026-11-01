#include "ParameterLayout.h"

namespace craft::band_limiter
{

namespace
{
using Range = juce::NormalisableRange<float>;

Range mapped (float lo, float hi, std::function<double (double)> fromNorm, std::function<double (double)> toNorm)
{
    return Range (lo, hi,
                  [fromNorm] (float, float, float r) { return float (fromNorm (double (r))); },
                  [toNorm] (float, float, float v) { return float (toNorm (double (v))); });
}

juce::String dbText (float v, int) { return juce::String (std::abs (v) < 0.05f ? 0.0f : v, 1) + " dB"; }
juce::String msText (float v, int) { return v < 100.0f ? juce::String (v, 2) + " ms" : juce::String (v, 0) + " ms"; }
juce::String hzText (float v, int) { return juce::String (juce::roundToInt (v)) + " Hz"; }

float parseValue (const juce::String& t) { return t.retainCharacters ("0123456789.,-").replaceCharacter (',', '.').getFloatValue(); }

template <typename Fn>
auto floatAttrs (Fn textFn)
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction (textFn).withValueFromStringFunction (parseValue);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (dsp::Variant variant)
{
    namespace pm = dsp::param;
    using Float = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Bool = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    constexpr int version = 1;
    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, version }; };

    auto levelRange = mapped (-30.0f, 0.0f, pm::levelFromNorm, pm::levelToNorm);
    layout.add (std::make_unique<Float> (pid (ids::threshold), "Threshold", levelRange, 0.0f, floatAttrs (dbText)));
    layout.add (std::make_unique<Float> (pid (ids::ceiling), "Out Ceiling", levelRange, 0.0f, floatAttrs (dbText)));

    if (dsp::isProfileVariant (variant))
    {
        juce::StringArray names;
        for (const auto& p : dsp::profiles)
            names.add (p.name);
        layout.add (std::make_unique<Choice> (pid (ids::profile), "Profile", names, 0));
        layout.add (std::make_unique<Float> (pid (ids::multiplier), "Release Multiplier",
                                             mapped (0.1f, 10.0f, pm::multiplierFromNorm, pm::multiplierToNorm), 1.0f,
                                             floatAttrs ([] (float v, int) { return "x" + juce::String (v, 2); })));
    }
    else
    {
        for (int b = 0; b < dsp::numBands; ++b)
        {
            const juce::String n = "Band " + juce::String (b + 1) + " ";
            auto bandRange = mapped (-12.0f, 12.0f, pm::bandDbFromNorm, pm::bandDbToNorm);
            layout.add (std::make_unique<Float> (pid (ids::gain (b)), n + "Gain", bandRange, 0.0f, floatAttrs (dbText)));
            layout.add (std::make_unique<Float> (pid (ids::priority (b)), n + "Priority", bandRange, 0.0f,
                                                 floatAttrs ([] (float v, int) { return juce::String (v, 1); })));
            layout.add (std::make_unique<Float> (pid (ids::release (b)), n + "Release",
                                                 mapped (0.01f, 2500.0f, pm::releaseFromNorm, pm::releaseToNorm),
                                                 float (dsp::defaultReleaseMs[size_t (b)]), floatAttrs (msText)));
            layout.add (std::make_unique<Bool> (pid (ids::solo (b)), n + "Solo", false));
        }
        for (int k = 0; k < dsp::numCrossovers; ++k)
        {
            // Host value 16 * 1334.8^r Hz; the clamp to the crossover's range (D3) is part of the mapping.
            auto range = mapped (float (dsp::crossoverMinHz[size_t (k)]), float (dsp::crossoverMaxHz[size_t (k)]),
                                 [k] (double r) { return pm::crossoverFromNorm (k, r); }, pm::crossoverToNorm);
            layout.add (std::make_unique<Float> (pid (ids::crossover (k)), juce::String (crossoverNames[k]) + " Crossover", range,
                                                 float (dsp::defaultCrossoverHz[size_t (k)]), floatAttrs (hzText)));
        }
        layout.add (std::make_unique<Float> (pid (ids::separation), "Separation", Range (0.0f, 100.0f), 100.0f,
                                             floatAttrs ([] (float v, int) { return juce::String (v, 0) + " %"; })));
        if (variant == dsp::Variant::LinearMulti)
            layout.add (std::make_unique<Choice> (pid (ids::behaviour), "Release Behaviour",
                                                  juce::StringArray { "Adaptive", "Aggressive", "Warm", "Scaled", "Manual" }, 0));
    }

    layout.add (std::make_unique<Choice> (pid (ids::quantize), "Quantize", juce::StringArray { "24", "22", "20", "18", "16" }, 0));
    layout.add (std::make_unique<Choice> (pid (ids::dither), "Dither", juce::StringArray { "None", "Type 1", "Type 2" }, 1));
    layout.add (std::make_unique<Choice> (pid (ids::shaping), "Shaping", juce::StringArray { "None", "Moderate", "Normal", "Ultra" }, 2));
    layout.add (std::make_unique<Bool> (pid (ids::bypass), "Bypass", false));
    return layout;
}

} // namespace craft::band_limiter
