#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

#ifndef DE_ESSER_CHANNELS
 #define DE_ESSER_CHANNELS 2
#endif

namespace craft::deesser
{

namespace
{
juce::AudioChannelSet layoutFor (int channels)
{
    return channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
}

void raise (std::atomic<float>& a, float v) noexcept
{
    float prev = a.load (std::memory_order_relaxed);
    while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
}

void lower (std::atomic<float>& a, float v) noexcept
{
    float prev = a.load (std::memory_order_relaxed);
    while (v < prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
}
} // namespace

DeEsserProcessor::DeEsserProcessor (int numChannels)
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", layoutFor (numChannels), true)
                          .withOutput ("Output", layoutFor (numChannels), true)),
      channels (numChannels == 1 ? 1 : 2),
      state (*this, nullptr, "CraftDeEsserState", createParameterLayout()),
      engine (channels)
{
    threshold = state.getParameter (ids::threshold);
    frequency = state.getParameter (ids::frequency);
    sidechain = state.getParameter (ids::sidechain);
    audio = state.getParameter (ids::audio);
    monitor = state.getParameter (ids::monitor);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (ids::bypass));
}

const juce::String DeEsserProcessor::getName() const
{
    return channels == 1 ? "Craft De-Esser Mono" : "Craft De-Esser";
}

bool DeEsserProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == layoutFor (channels)
        && layouts.getMainOutputChannelSet() == layoutFor (channels);
}

dsp::Settings DeEsserProcessor::readSettings() const
{
    namespace pm = dsp::param;
    dsp::Settings s;
    s.thresholdDb = pm::Threshold::fromNorm (normalised (threshold));
    s.frequencyHz = pm::Frequency::fromNorm (normalised (frequency));
    s.sidechain = dsp::switchState (normalised (sidechain)) ? dsp::SidechainMode::HighPass : dsp::SidechainMode::BandPass;
    s.audio = dsp::switchState (normalised (audio)) ? dsp::AudioMode::Wideband : dsp::AudioMode::Split;
    s.monitor = dsp::switchState (normalised (monitor)) ? dsp::MonitorMode::Sidechain : dsp::MonitorMode::Output;
    return s;
}

void DeEsserProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setSettings (readSettings());
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());
    engine.prepare (sampleRate);
    setLatencySamples (dsp::DeEsserEngine::getLatencySamples());
}

template <typename Sample>
void DeEsserProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < channels || n == 0)
        return;

    engine.setSettings (readSettings());
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());

    Sample* ptrs[2] = { buffer.getWritePointer (0), channels > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.process (ptrs, n);

    raise (meterSidechain, float (engine.takeMeterEnvelope()));
    lower (meterMinGain, float (engine.takeMeterMinGain()));
    for (int c = 0; c < channels; ++c)
        raise (meterOut[c], float (buffer.getMagnitude (c, 0, n)));
}

void DeEsserProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void DeEsserProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

DeEsserProcessor::MeterValues DeEsserProcessor::takeMeters() noexcept
{
    MeterValues m;
    m.sidechainPeak = meterSidechain.exchange (0.0f);
    m.minGain = meterMinGain.exchange (1.0f);
    m.output[0] = meterOut[0].exchange (0.0f);
    m.output[1] = channels > 1 ? meterOut[1].exchange (0.0f) : m.output[0];
    m.monitoringSidechain = dsp::switchState (normalised (monitor));
    return m;
}

void DeEsserProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DeEsserProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* DeEsserProcessor::createEditor()
{
    return new DeEsserEditor (*this);
}

} // namespace craft::deesser

// Plug-in entry point: the channel count is selected at compile time per target.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::deesser::DeEsserProcessor (DE_ESSER_CHANNELS);
}
