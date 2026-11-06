#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::punch
{

PunchCompressorProcessor::PunchCompressorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftPunchCompressorState", createParameterLayout())
{
    // Look the parameters up once: string-keyed lookups must not run on the audio thread.
    threshold = state.getParameter (ids::threshold);
    ratio = state.getParameter (ids::ratio);
    mix = state.getParameter (ids::mix);
    attack = state.getParameter (ids::attack);
    punch = state.getParameter (ids::punch);
    release = state.getParameter (ids::release);
    sync = state.getParameter (ids::sync);
    note = state.getParameter (ids::note);
    analog = state.getParameter (ids::analog);
    output = state.getParameter (ids::output);
    limiter = state.getParameter (ids::limiter);
    tempo = state.getParameter (ids::tempo);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (ids::bypass));

    // a plausible latency before the first prepareToPlay (some hosts query it early)
    setLatencySamples (PunchEngine::latencyForSampleRate (44100.0));
}

bool PunchCompressorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo (one linked detector) and mono, input layout = output layout.
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

Settings PunchCompressorProcessor::readSettings() const
{
    Settings s;
    s.thresholdDb = param::Threshold::fromNorm (threshold->getValue());
    s.ratio = param::Ratio::fromNorm (ratio->getValue());
    s.mixPct = param::Mix::fromNorm (mix->getValue());
    s.attackMs = param::Attack::fromNorm (attack->getValue());
    s.punch = param::Punch::fromNorm (punch->getValue());
    s.releaseMs = param::Release::fromNorm (release->getValue());
    s.sync = param::syncFromNorm (sync->getValue());
    s.note = int (param::Note::fromNorm (note->getValue()));
    s.analog = param::analogFromNorm (analog->getValue());
    s.outputDb = param::Output::fromNorm (output->getValue());
    s.limiter = limiter->getValue() >= 0.5f;
    s.manualBpm = param::Tempo::fromNorm (tempo->getValue());
    s.hostBpm = hostBpm.load();
    return s;
}

double PunchCompressorProcessor::getTailLengthSeconds() const
{
    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    return PunchEngine::latencyForSampleRate (fs) / fs;
}

void PunchCompressorProcessor::prepareToPlay (double sampleRate, int)
{
    const int channels = std::max (1, std::min (2, getMainBusNumOutputChannels()));
    engine.prepare (sampleRate, channels);
    engine.setSettings (readSettings());
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());
    engine.reset();
    setLatencySamples (engine.getLatencySamples()); // spec sections 3 / 12: H = 64 / 128 / 256
}

void PunchCompressorProcessor::updateHostTempo()
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 0.0 && std::isfinite (*bpm))
                {
                    hostBpm = *bpm;
                    hostTempoKnown = true;
                }
}

template <typename Sample>
void PunchCompressorProcessor::processAudio (juce::AudioBuffer<Sample>& buffer, bool forceBypass)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = engine.getNumChannels();
    if (n == 0 || buffer.getNumChannels() < channels)
        return;

    updateHostTempo();
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
    raise (inPeak, engine.getInputPeak());
    raise (outPeak, engine.getOutputPeak());
    const float g = float (engine.getMinGain());
    float prev = minGain.load (std::memory_order_relaxed);
    while (g < prev && ! minGain.compare_exchange_weak (prev, g, std::memory_order_relaxed)) {}
}

void PunchCompressorProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b, false); }
void PunchCompressorProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b, false); }
// Hosts that bypass without our parameter still get the latency-compensated dry signal.
void PunchCompressorProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b, true); }
void PunchCompressorProcessor::processBlockBypassed (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b, true); }

MeterSnapshot PunchCompressorProcessor::takeMeters() noexcept
{
    MeterSnapshot m;
    m.input = inPeak.exchange (0.0f);
    m.output = outPeak.exchange (0.0f);
    m.minGain = minGain.exchange (1.0f);
    return m;
}

void PunchCompressorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PunchCompressorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PunchCompressorProcessor::createEditor()
{
    return new PunchCompressorEditor (*this);
}

} // namespace craft::punch

#ifndef CRAFT_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::punch::PunchCompressorProcessor();
}
#endif
