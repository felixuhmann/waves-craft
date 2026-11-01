#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::enhancer
{

EnhancerProcessor::EnhancerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftEnhancerState", createParameterLayout())
{
    auto raw = [this] (const juce::String& id) { return state.getRawParameterValue (id); };
    input = raw (ids::input);
    output = raw (ids::output);
    direct = raw (ids::direct);
    punch = raw (ids::punch);
    for (int b = 0; b < dsp::numBands; ++b)
    {
        bandGain[(size_t) b] = raw (ids::bandGain (b));
        width[(size_t) b] = raw (ids::width (b));
        solo[(size_t) b] = raw (ids::solo (b));
        bypass[(size_t) b] = raw (ids::bypass (b));
    }
    for (int k = 0; k < dsp::numCrossovers; ++k)
        crossover[(size_t) k] = raw (ids::crossover (k));
}

bool EnhancerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

dsp::Settings EnhancerProcessor::readSettings() const
{
    dsp::Settings s;
    s.inputDb = input->load();
    s.outputDb = output->load();
    s.directDb = direct->load();
    s.punch = punch->load();
    for (size_t b = 0; b < dsp::numBands; ++b)
    {
        s.bandDb[b] = bandGain[b]->load();
        s.width[b] = width[b]->load();
        s.solo[b] = solo[b]->load() >= 0.5f;
        s.bypass[b] = bypass[b]->load() >= 0.5f;
    }
    for (size_t k = 0; k < dsp::numCrossovers; ++k)
        s.crossoverHz[k] = crossover[k]->load();
    return s;
}

void EnhancerProcessor::prepareToPlay (double sampleRate, int)
{
    currentRate = sampleRate;
    engine.setSettings (readSettings());
    engine.prepare (sampleRate, juce::jlimit (1, 2, getMainBusNumOutputChannels()));
    setLatencySamples (dsp::EnhancerEngine::getLatencySamples());
}

template <typename Sample>
void EnhancerProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), channels = juce::jmin (2, buffer.getNumChannels());
    if (n == 0 || channels == 0)
        return;

    auto raise = [] (std::atomic<float>& a, float v) {
        float prev = a.load (std::memory_order_relaxed);
        while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
    };
    for (int c = 0; c < channels; ++c)
        raise (inPeaks[c], float (buffer.getMagnitude (c, 0, n)));

    engine.setSettings (readSettings());
    engine.process (buffer.getArrayOfWritePointers(), n);

    for (int c = 0; c < channels; ++c)
        raise (outPeaks[c], float (buffer.getMagnitude (c, 0, n)));
}

void EnhancerProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b); }
void EnhancerProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b); }

void EnhancerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EnhancerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* EnhancerProcessor::createEditor() { return new EnhancerEditor (*this); }

} // namespace craft::enhancer

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new craft::enhancer::EnhancerProcessor(); }
