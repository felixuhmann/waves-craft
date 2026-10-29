#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::limiter
{

PeakLimiterProcessor::PeakLimiterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftPeakLimiterState", createParameterLayout())
{
    threshold = state.getParameter (ids::threshold);
    ceiling = state.getParameter (ids::ceiling);
    release = state.getParameter (ids::release);
    autoRelease = state.getParameter (ids::autoRelease);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (ids::bypass));

    // report a plausible latency before the first prepareToPlay (some hosts query it early)
    setLatencySamples (LimiterEngine::latencyForSampleRate (44100.0));
}

bool PeakLimiterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo (linked) and mono, input layout = output layout.
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

Settings PeakLimiterProcessor::readSettings() const
{
    Settings s;
    s.thresholdDb = param::Threshold::fromNorm (threshold->getValue());
    s.ceilingDb = param::Ceiling::fromNorm (ceiling->getValue());
    s.releaseMs = param::Release::fromNorm (release->getValue());
    s.autoRelease = autoRelease->getValue() >= 0.5f;
    return s;
}

double PeakLimiterProcessor::getTailLengthSeconds() const
{
    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    return LimiterEngine::latencyForSampleRate (fs) / fs;
}

void PeakLimiterProcessor::prepareToPlay (double sampleRate, int)
{
    const int channels = std::max (1, std::min (2, getMainBusNumOutputChannels()));
    engine.prepare (sampleRate, channels);
    engine.setSettings (readSettings());
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());
    engine.reset();
    meterChannels = channels;
    setLatencySamples (engine.getLatencySamples()); // spec section 3: 64 / 128 / 256
}

template <typename Sample>
void PeakLimiterProcessor::processAudio (juce::AudioBuffer<Sample>& buffer, bool forceBypass)
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
    const float g = float (engine.getMinGain());
    float prev = minGain.load (std::memory_order_relaxed);
    while (g < prev && ! minGain.compare_exchange_weak (prev, g, std::memory_order_relaxed)) {}
}

void PeakLimiterProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b, false); }
void PeakLimiterProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b, false); }
// Hosts that bypass without our parameter still get the latency-compensated dry signal.
void PeakLimiterProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b, true); }
void PeakLimiterProcessor::processBlockBypassed (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b, true); }

MeterSnapshot PeakLimiterProcessor::takeMeters() noexcept
{
    MeterSnapshot m;
    for (int c = 0; c < 2; ++c)
    {
        m.input[size_t (c)] = inPeak[c].exchange (0.0f);
        m.output[size_t (c)] = outPeak[c].exchange (0.0f);
    }
    m.minGain = minGain.exchange (1.0f);
    m.channels = meterChannels.load();
    return m;
}

void PeakLimiterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PeakLimiterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PeakLimiterProcessor::createEditor()
{
    return new PeakLimiterEditor (*this);
}

} // namespace craft::limiter

#ifndef CRAFT_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::limiter::PeakLimiterProcessor();
}
#endif
