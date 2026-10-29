#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::hdelay
{

HDelayProcessor::HDelayProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftHybridDelayState", createParameterLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (ids::bypass));
}

bool HDelayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

Settings HDelayProcessor::readSettings() const
{
    auto raw = [this] (const char* id) { return state.getRawParameterValue (id)->load(); };
    auto on = [&] (const char* id) { return raw (id) >= 0.5f; };
    auto norm = [this] (const char* id) { return double (state.getParameter (id)->getValue()); };

    Settings s;
    s.sync = static_cast<param::Sync> (juce::jlimit (0, 2, int (raw (ids::sync))));
    s.bpm = raw (ids::bpm);
    s.hostBpm = hostBpm.load();
    s.delayMs = raw (ids::delayMs);
    s.delayRMs = raw (ids::delayRMs);
    s.delayNote = int (raw (ids::delayNote));
    s.delayRNote = int (raw (ids::delayRNote));
    s.dual = on (ids::dual);
    s.feedbackPct = raw (ids::feedback);
    s.pingPong = on (ids::pingPong);
    s.pingPongStartRight = int (raw (ids::pingPongStart)) == 1;
    s.phaseL = on (ids::phaseL);
    s.phaseR = on (ids::phaseR);
    s.mixPct = raw (ids::mix);
    s.outputDb = raw (ids::output);
    s.analog = int (raw (ids::analog));
    s.lofi = on (ids::lofi);
    s.modRateHz = raw (ids::modRate);
    s.modDepthPct = raw (ids::modDepth);
    s.hiPassHz = param::HiPass::fromNorm (norm (ids::hiPass));
    s.loPassHz = param::LoPass::fromNorm (norm (ids::loPass));
    return s;
}

void HDelayProcessor::prepareToPlay (double sampleRate, int)
{
    monoInput = getTotalNumInputChannels() == 1;
    engine.setInputMode (monoInput ? InputMode::MonoToStereo : InputMode::Stereo);
    engine.setSettings (readSettings());
    engine.prepare (sampleRate);
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());
    engine.reset();
    setLatencySamples (HybridDelayEngine::getLatencySamples());
}

template <typename Sample>
void HDelayProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 0.0)
                    hostBpm = *bpm;

    const bool mono = getTotalNumInputChannels() == 1;
    monoInput = mono;
    engine.setInputMode (mono ? InputMode::MonoToStereo : InputMode::Stereo);
    engine.setSettings (readSettings());
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    engine.process (l, r, n);

    Sample pl = 0, pr = 0;
    for (int i = 0; i < n; ++i)
    {
        pl = std::max (pl, std::abs (l[i]));
        pr = std::max (pr, std::abs (r[i]));
    }
    auto raise = [] (std::atomic<float>& a, Sample v) {
        float prev = a.load (std::memory_order_relaxed);
        while (float (v) > prev && ! a.compare_exchange_weak (prev, float (v), std::memory_order_relaxed)) {}
    };
    raise (peaks[0], pl);
    raise (peaks[1], pr);
}

void HDelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void HDelayProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

void HDelayProcessor::setParameterValue (const char* id, float physicalValue)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (physicalValue));
        p->endChangeGesture();
    }
}

void HDelayProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HDelayProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* HDelayProcessor::createEditor()
{
    return new HDelayEditor (*this);
}

} // namespace craft::hdelay

#ifndef CRAFT_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::hdelay::HDelayProcessor();
}
#endif
