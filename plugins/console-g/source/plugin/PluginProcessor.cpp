#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::consoleg
{

ConsoleGProcessor::ConsoleGProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftConsoleGState", createParameterLayout())
{
    auto get = [this] (const char* id) {
        auto* prm = state.getParameter (id);
        jassert (prm != nullptr);
        return prm;
    };
    p = { get (ids::compThreshold), get (ids::compRatio), get (ids::compRelease), get (ids::compFast),
          get (ids::expThreshold), get (ids::expRange), get (ids::expRelease), get (ids::expGate), get (ids::expFast),
          get (ids::dynBypass), get (ids::dynPostEq), get (ids::filtersSidechain), get (ids::filtersSplit),
          get (ids::eqBypass), get (ids::hpFreq), get (ids::lpFreq), get (ids::lfGain), get (ids::lfFreq),
          get (ids::lmfGain), get (ids::lmfFreq), get (ids::lmfQ), get (ids::lmfDiv3), get (ids::hmfGain),
          get (ids::hmfFreq), get (ids::hmfQ), get (ids::hmfX3), get (ids::hfGain), get (ids::hfFreq),
          get (ids::inputTrim), get (ids::outputGain), get (ids::analog), get (ids::polarity) };
    setLatencySamples (dsp::ConsoleEngine::getLatencySamples()); // spec 1: zero latency
}

bool ConsoleGProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // stereo (linked dynamics) or mono, input layout = output layout
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

dsp::Settings ConsoleGProcessor::readSettingsFast() const noexcept
{
    auto r = [] (juce::RangedAudioParameter* x) { return double (x->getValue()); };
    auto b = [] (juce::RangedAudioParameter* x) { return x->getValue() >= 0.5f; };
    dsp::Settings s;
    s.compThreshold = r (p.compThreshold);
    s.compRatio = r (p.compRatio);
    s.compRelease = r (p.compRelease);
    s.compFast = b (p.compFast);
    s.expThreshold = r (p.expThreshold);
    s.expRange = r (p.expRange);
    s.expRelease = r (p.expRelease);
    s.expGate = b (p.expGate);
    s.expFast = b (p.expFast);
    s.dynBypass = b (p.dynBypass);
    s.dynPostEq = b (p.dynPostEq);
    s.filtersSidechain = b (p.filtersSidechain);
    s.filtersSplit = b (p.filtersSplit);
    s.eqBypass = b (p.eqBypass);
    s.hpFreq = r (p.hpFreq);
    s.lpFreq = r (p.lpFreq);
    s.lfGain = r (p.lfGain);
    s.lfFreq = r (p.lfFreq);
    s.lmfGain = r (p.lmfGain);
    s.lmfFreq = r (p.lmfFreq);
    s.lmfQ = r (p.lmfQ);
    s.lmfDiv3 = b (p.lmfDiv3);
    s.hmfGain = r (p.hmfGain);
    s.hmfFreq = r (p.hmfFreq);
    s.hmfQ = r (p.hmfQ);
    s.hmfX3 = b (p.hmfX3);
    s.hfGain = r (p.hfGain);
    s.hfFreq = r (p.hfFreq);
    s.inputTrim = r (p.inputTrim);
    s.outputGain = r (p.outputGain);
    s.analog = b (p.analog);
    s.polarity = b (p.polarity);
    return s;
}

void ConsoleGProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setSettings (readSettingsFast());
    engine.prepare (sampleRate);
    meterChannels = std::max (1, std::min (2, getMainBusNumOutputChannels()));
    setLatencySamples (dsp::ConsoleEngine::getLatencySamples());
}

template <typename Sample>
void ConsoleGProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = std::min ({ buffer.getNumChannels(), getMainBusNumOutputChannels(), 2 });
    if (n == 0 || channels <= 0)
        return;

    engine.setSettings (readSettingsFast());
    engine.process (buffer.getArrayOfWritePointers(), channels, n);
    for (int c = buffer.getNumChannels() - 1; c >= channels; --c)
        buffer.clear (c, 0, n);

    auto raise = [] (std::atomic<float>& a, float v) {
        float prev = a.load (std::memory_order_relaxed);
        while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
    };
    // the engine meters both channels together; per-channel output peaks are read from the buffer
    raise (inPeak[0], float (engine.getInputPeak()));
    raise (inPeak[1], float (engine.getInputPeak()));
    for (int c = 0; c < channels; ++c)
        raise (outPeak[c], float (buffer.getMagnitude (c, 0, n)));
    const float gr = float (engine.getGainReductionDb());
    float prev = minGr.load (std::memory_order_relaxed);
    while (gr < prev && ! minGr.compare_exchange_weak (prev, gr, std::memory_order_relaxed)) {}
    meterChannels = channels;
}

void ConsoleGProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b); }
void ConsoleGProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b); }

MeterSnapshot ConsoleGProcessor::takeMeters() noexcept
{
    MeterSnapshot m;
    for (int c = 0; c < 2; ++c)
    {
        m.input[size_t (c)] = inPeak[c].exchange (0.0f);
        m.output[size_t (c)] = outPeak[c].exchange (0.0f);
    }
    m.gainReductionDb = minGr.exchange (0.0f);
    m.channels = meterChannels.load();
    return m;
}

void ConsoleGProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ConsoleGProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ConsoleGProcessor::createEditor()
{
    return new ConsoleGEditor (*this);
}

} // namespace craft::consoleg

#ifndef CRAFT_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::consoleg::ConsoleGProcessor();
}
#endif
