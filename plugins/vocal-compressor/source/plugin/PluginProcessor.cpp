#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::vocal
{

VocalCompressorProcessor::VocalCompressorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftVocalCompressorState", createParameterLayout())
{
    compression = state.getParameter (ids::compression);
    gate = state.getParameter (ids::gate);
    gain = state.getParameter (ids::gain);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (ids::bypass));

    // report a plausible latency before the first prepareToPlay (some hosts query it early)
    setLatencySamples (VocalCompressorEngine::latencyForSampleRate (44100.0));
}

bool VocalCompressorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo (linked) and mono, input layout = output layout.
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

Settings VocalCompressorProcessor::readSettings() const
{
    Settings s;
    s.compressionDb = param::Compression::fromNorm (compression->getValue());
    s.gateDb = param::Gate::fromNorm (gate->getValue());
    s.gainDb = param::Gain::fromNorm (gain->getValue());
    return s;
}

double VocalCompressorProcessor::getTailLengthSeconds() const
{
    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    return VocalCompressorEngine::latencyForSampleRate (fs) / fs;
}

void VocalCompressorProcessor::prepareToPlay (double sampleRate, int)
{
    const int channels = std::max (1, std::min (2, getMainBusNumOutputChannels()));
    engine.prepare (sampleRate, channels);
    engine.setSettings (readSettings());
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());
    engine.reset();
    meterChannels = channels;
    setLatencySamples (engine.getLatencySamples()); // section 4.1: H = 64 / 128 / 256
}

template <typename Sample>
void VocalCompressorProcessor::processAudio (juce::AudioBuffer<Sample>& buffer, bool forceBypass)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = std::min (buffer.getNumChannels(), engine.getNumChannels());
    if (n == 0 || channels < engine.getNumChannels())
        return;

    engine.setSettings (readSettings());
    engine.setBypassed (forceBypass || (bypassParam != nullptr && bypassParam->get()));
    engine.resetMeters();
    engine.process (buffer.getArrayOfWritePointers(), n);

    for (int c = buffer.getNumChannels() - 1; c >= channels; --c)
        buffer.clear (c, 0, n);

    auto raise = [] (std::atomic<float>& a, double v) {
        float prev = a.load (std::memory_order_relaxed);
        while (float (v) > prev && ! a.compare_exchange_weak (prev, float (v), std::memory_order_relaxed)) {}
    };
    for (int c = 0; c < channels; ++c)
        raise (outPeak[c], engine.getOutputPeak (c));

    const float g = float (engine.getMinGain());
    float prev = minGain.load (std::memory_order_relaxed);
    while (g < prev && ! minGain.compare_exchange_weak (prev, g, std::memory_order_relaxed)) {}

    double e = energySum.load (std::memory_order_relaxed);
    while (! energySum.compare_exchange_weak (e, e + engine.getInputEnergySum(), std::memory_order_relaxed)) {}
    energyCount.fetch_add (n, std::memory_order_relaxed);
}

void VocalCompressorProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b, false); }
void VocalCompressorProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b, false); }
// Hosts that bypass without our parameter still get the latency-compensated dry signal.
void VocalCompressorProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b, true); }
void VocalCompressorProcessor::processBlockBypassed (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b, true); }

MeterSnapshot VocalCompressorProcessor::takeMeters() noexcept
{
    MeterSnapshot m;
    for (int c = 0; c < 2; ++c)
        m.output[size_t (c)] = outPeak[c].exchange (0.0f);
    m.minGain = minGain.exchange (1.0f);
    const double sum = energySum.exchange (0.0);
    const int count = energyCount.exchange (0);
    m.inputEnergy = count > 0 ? float (sum / count) : 0.0f;
    m.channels = meterChannels.load();
    return m;
}

void VocalCompressorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalCompressorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VocalCompressorProcessor::createEditor()
{
    return new VocalCompressorEditor (*this);
}

} // namespace craft::vocal

#ifndef CRAFT_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::vocal::VocalCompressorProcessor();
}
#endif
