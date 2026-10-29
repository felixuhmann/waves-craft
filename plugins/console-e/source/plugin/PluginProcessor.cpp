#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::consolee
{

namespace
{
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

ConsoleProcessor::ConsoleProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftConsoleEState", createParameterLayout())
{
    auto get = [this] (const char* id) { return state.getParameter (id); };
    params.inputTrim = get (ids::inputTrim);
    params.outputFader = get (ids::outputFader);
    params.polarity = get (ids::polarity);
    params.analogue = get (ids::analogue);
    params.meterOutput = get (ids::meterOutput);
    params.hp = get (ids::hp);
    params.lp = get (ids::lp);
    params.split = get (ids::split);
    params.lfGain = get (ids::lfGain);
    params.lfFreq = get (ids::lfFreq);
    params.lfBell = get (ids::lfBell);
    params.lmfGain = get (ids::lmfGain);
    params.lmfFreq = get (ids::lmfFreq);
    params.lmfQ = get (ids::lmfQ);
    params.hmfGain = get (ids::hmfGain);
    params.hmfFreq = get (ids::hmfFreq);
    params.hmfQ = get (ids::hmfQ);
    params.hfGain = get (ids::hfGain);
    params.hfFreq = get (ids::hfFreq);
    params.hfBell = get (ids::hfBell);
    params.eqBypass = get (ids::eqBypass);
    params.eqToSidechain = get (ids::eqToSidechain);
    params.compRatio = get (ids::compRatio);
    params.compThreshold = get (ids::compThreshold);
    params.compRelease = get (ids::compRelease);
    params.compFast = get (ids::compFast);
    params.expRange = get (ids::expRange);
    params.expThreshold = get (ids::expThreshold);
    params.expRelease = get (ids::expRelease);
    params.expFast = get (ids::expFast);
    params.gate = get (ids::gate);
    params.dynBypass = get (ids::dynBypass);
    params.channelOut = get (ids::channelOut);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (get (ids::bypass));
}

bool ConsoleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

dsp::Settings ConsoleProcessor::readSettings() const noexcept
{
    dsp::Settings s;
    s.inputTrim = value (params.inputTrim);
    s.outputFader = value (params.outputFader);
    s.polarity = on (params.polarity);
    s.analogue = on (params.analogue);
    s.eq.hp = value (params.hp);
    s.eq.lp = value (params.lp);
    s.eq.lfGain = value (params.lfGain);
    s.eq.lfFreq = value (params.lfFreq);
    s.eq.lfBell = on (params.lfBell);
    s.eq.lmfGain = value (params.lmfGain);
    s.eq.lmfFreq = value (params.lmfFreq);
    s.eq.lmfQ = value (params.lmfQ);
    s.eq.hmfGain = value (params.hmfGain);
    s.eq.hmfFreq = value (params.hmfFreq);
    s.eq.hmfQ = value (params.hmfQ);
    s.eq.hfGain = value (params.hfGain);
    s.eq.hfFreq = value (params.hfFreq);
    s.eq.hfBell = on (params.hfBell);
    s.split = on (params.split);
    s.eqBypass = on (params.eqBypass);
    s.eqToSidechain = on (params.eqToSidechain);
    s.compRatio = value (params.compRatio);
    s.compThreshold = value (params.compThreshold);
    s.compRelease = value (params.compRelease);
    s.compFastAttack = on (params.compFast);
    s.expRange = value (params.expRange);
    s.expThreshold = value (params.expThreshold);
    s.expRelease = value (params.expRelease);
    s.expFastAttack = on (params.expFast);
    s.gate = on (params.gate);
    s.dynamicsBypass = on (params.dynBypass);
    s.channelOut = on (params.channelOut);
    return s;
}

void ConsoleProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setNumChannels (getMainBusNumInputChannels() == 1 ? 1 : 2);
    engine.setSettings (readSettings());
    engine.prepare (sampleRate);
    setLatencySamples (dsp::ConsoleEngine::getLatencySamples());
}

template <typename Sample>
void ConsoleProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = engine.getNumChannels();
    for (int c = channels; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    if (buffer.getNumChannels() < channels || n == 0)
        return;

    engine.setSettings (readSettings());
    Sample* ptrs[2] = { buffer.getWritePointer (0), channels > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.process (ptrs, n);

    const auto m = engine.takeMeters();
    for (int c = 0; c < 2; ++c)
    {
        const int src = std::min (c, channels - 1);
        raise (meterIn[c], float (m.inputPeak[src]));
        raise (meterOut[c], float (m.outputPeak[src]));
    }
    lower (meterComp, float (m.compReductionDb));
    lower (meterExp, float (m.expanderGainDb));
}

void ConsoleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void ConsoleProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

ConsoleProcessor::MeterValues ConsoleProcessor::takeMeters() noexcept
{
    MeterValues v;
    v.showsOutput = on (params.meterOutput);
    for (int c = 0; c < 2; ++c)
    {
        const float in = meterIn[c].exchange (0.0f), out = meterOut[c].exchange (0.0f);
        v.level[c] = v.showsOutput ? out : in;
    }
    v.compReductionDb = meterComp.exchange (0.0f);
    v.expanderGainDb = meterExp.exchange (0.0f);
    return v;
}

void ConsoleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ConsoleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ConsoleProcessor::createEditor()
{
    return new ConsoleEditor (*this);
}

} // namespace craft::consolee

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::consolee::ConsoleProcessor();
}
