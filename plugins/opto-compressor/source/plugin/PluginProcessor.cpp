#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::opto
{

namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using F = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    using Id = juce::ParameterID;
    using R = juce::NormalisableRange<float>;
    auto fmt = [] (int decimals, const char* unit) {
        return juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [decimals, unit] (float v, int) { return juce::String (v, decimals) + unit; });
    };
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<F> (Id (ids::gain, 1), "Gain", R (0.0f, 10.0f, 0.01f), 4.08f, fmt (2, "")));
    l.add (std::make_unique<F> (Id (ids::peakReduction, 1), "Peak Reduction", R (0.0f, 10.0f, 0.01f), 0.0f, fmt (2, "")));
    l.add (std::make_unique<C> (Id (ids::mode, 1), "Comp/Limit", juce::StringArray { "Comp", "Limit" }, 0));
    l.add (std::make_unique<F> (Id (ids::hiFreq, 1), "Hi Freq", R (0.0f, 100.0f, 0.1f), 100.0f, fmt (1, "")));
    l.add (std::make_unique<C> (Id (ids::analog, 1), "Analog", juce::StringArray { "Off", "50 Hz", "60 Hz" }, 0));
    l.add (std::make_unique<F> (Id (ids::mix, 1), "Mix", R (0.0f, 100.0f, 0.1f), 100.0f, fmt (1, " %")));
    l.add (std::make_unique<F> (Id (ids::trim, 1), "Trim", R (-18.0f, 18.0f, 0.1f), 0.0f, fmt (1, " dB")));
    l.add (std::make_unique<B> (Id (ids::autoMakeup, 1), "Auto Makeup", false));
    l.add (std::make_unique<C> (Id (ids::vuDisplay, 1), "VU Display", juce::StringArray { "In", "GR", "Out" }, 1));
    l.add (std::make_unique<B> (Id (ids::bypass, 1), "Bypass", false));
    return l;
}
} // namespace

OptoProcessor::OptoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftOptoCompressorState", createLayout())
{
    bypassParam = state.getParameter (ids::bypass);
}

bool OptoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

double OptoProcessor::physical (const char* id) const noexcept
{
    auto* p = state.getParameter (id);
    return double (p->convertFrom0to1 (p->getValue()));
}

dsp::Settings OptoProcessor::readSettings() const noexcept
{
    dsp::Settings s;
    s.gain = physical (ids::gain);
    s.peakReduction = physical (ids::peakReduction);
    s.limit = physical (ids::mode) >= 0.5;
    s.hiFreq = physical (ids::hiFreq);
    s.analog = juce::roundToInt (physical (ids::analog));
    s.mix = physical (ids::mix) * 0.01;
    s.trimDb = physical (ids::trim);
    s.bypass = bypassParam->getValue() >= 0.5f;
    return s;
}

void OptoProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setSettings (readSettings());
    engine.prepare (sampleRate);
    setLatencySamples (0);
}

template <typename Sample>
void OptoProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    if (n == 0 || nch == 0)
        return;

    auto storeMax = [] (std::atomic<float>& a, float v) {
        float prev = a.load (std::memory_order_relaxed);
        while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
    };
    engine.setSettings (readSettings());
    for (int c = 0; c < nch; ++c)
        storeMax (inPeak, float (buffer.getMagnitude (c, 0, n)));
    engine.process (buffer.getWritePointer (0), nch > 1 ? buffer.getWritePointer (1) : static_cast<Sample*> (nullptr), n);
    for (int c = 0; c < nch; ++c)
        storeMax (outPeak, float (buffer.getMagnitude (c, 0, n)));
    for (int c = nch; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    reduction.store (float (engine.getReductionDb()), std::memory_order_relaxed);
}

void OptoProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b); }
void OptoProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b); }

OptoProcessor::MeterView OptoProcessor::takeMeters() noexcept
{
    return { inPeak.exchange (0.0f), outPeak.exchange (0.0f), reduction.load() };
}

void OptoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void OptoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* OptoProcessor::createEditor() { return new OptoEditor (*this); }

} // namespace craft::opto

#if ! CRAFT_OPTO_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new craft::opto::OptoProcessor(); }
#endif
