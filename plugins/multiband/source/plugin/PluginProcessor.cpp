#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::multiband
{

MultibandProcessor::MultibandProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftMultibandState", createParameterLayout())
{
    using BP = ids::BandParam;
    outputGain = state.getParameter (ids::outputGain);
    q = state.getParameter (ids::xoverQ);
    knee = state.getParameter (ids::knee);
    autoRelease = state.getParameter (ids::autoRelease);
    opto = state.getParameter (ids::opto);
    for (int k = 0; k < numCrossovers; ++k)
        xover[size_t (k)] = state.getParameter (ids::crossover (k));
    for (int b = 0; b < numBands; ++b)
    {
        auto& p = bandParams[size_t (b)];
        p.gain = state.getParameter (ids::band (b, BP::gain));
        p.range = state.getParameter (ids::band (b, BP::range));
        p.threshold = state.getParameter (ids::band (b, BP::threshold));
        p.attack = state.getParameter (ids::band (b, BP::attack));
        p.release = state.getParameter (ids::band (b, BP::release));
        p.solo = state.getParameter (ids::band (b, BP::solo));
        p.bypass = state.getParameter (ids::band (b, BP::bypass));
    }
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (ids::bypass));
}

bool MultibandProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

Settings MultibandProcessor::readSettings() const
{
    Settings s;
    s.outputGainDb = param::OutputGain::fromNorm (outputGain->getValue());
    for (int k = 0; k < numCrossovers; ++k)
        s.crossoverHz[size_t (k)] = param::Crossover::fromNorm (xover[size_t (k)]->getValue());
    s.q = param::CrossoverQ::fromNorm (q->getValue());
    s.knee = param::Knee::fromNorm (knee->getValue());
    s.autoRelease = autoRelease->getValue() >= 0.5f;
    s.opto = opto->getValue() >= 0.5f;
    for (int b = 0; b < numBands; ++b)
    {
        const auto& p = bandParams[size_t (b)];
        auto& bs = s.bands[size_t (b)];
        bs.gainDb = param::BandGain::fromNorm (p.gain->getValue());
        bs.rangeDb = param::BandRange::fromNorm (p.range->getValue());
        bs.thresholdDb = param::BandThreshold::fromNorm (p.threshold->getValue());
        bs.attackMs = param::Attack::fromNorm (p.attack->getValue());
        bs.releaseMs = param::Release::fromNorm (p.release->getValue());
        bs.solo = p.solo->getValue() >= 0.5f;
        bs.bypass = p.bypass->getValue() >= 0.5f;
    }
    return s;
}

void MultibandProcessor::prepareToPlay (double sampleRate, int)
{
    const int channels = std::max (1, std::min (2, getMainBusNumOutputChannels()));
    engine.setSettings (readSettings());
    engine.prepare (sampleRate, channels);
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());
    meterChannels = channels;
    sampleRateForUi = sampleRate;
    setLatencySamples (0); // spec section 1: minimum-phase crossover, no look-ahead
}

template <typename Sample>
void MultibandProcessor::processAudio (juce::AudioBuffer<Sample>& buffer, bool forceBypass)
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
    {
        raise (inPeak[c], engine.getInputPeak (c));
        raise (outPeak[c], engine.getOutputPeak (c));
    }
    for (int b = 0; b < numBands; ++b)
    {
        raise (bandLevel[size_t (b)], engine.getBandLevel (b));
        bandGain[size_t (b)].store (float (engine.getBandGainDb (b)), std::memory_order_relaxed);
    }
}

void MultibandProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b, false); }
void MultibandProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b, false); }
void MultibandProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b, true); }
void MultibandProcessor::processBlockBypassed (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b, true); }

MeterSnapshot MultibandProcessor::takeMeters() noexcept
{
    MeterSnapshot m;
    for (int c = 0; c < 2; ++c)
    {
        m.input[size_t (c)] = inPeak[c].exchange (0.0f);
        m.output[size_t (c)] = outPeak[c].exchange (0.0f);
    }
    for (int b = 0; b < numBands; ++b)
    {
        m.bandLevel[size_t (b)] = bandLevel[size_t (b)].exchange (0.0f);
        m.bandGainDb[size_t (b)] = bandGain[size_t (b)].load (std::memory_order_relaxed);
    }
    m.channels = meterChannels.load();
    return m;
}

void MultibandProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MultibandProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MultibandProcessor::createEditor()
{
    return new MultibandEditor (*this);
}

} // namespace craft::multiband

#ifndef CRAFT_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::multiband::MultibandProcessor();
}
#endif
