#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::ddp
{

DynamicDistortionProcessor::DynamicDistortionProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftDynamicDistortionState", createParameterLayout())
{
    auto get = [this] (const juce::String& id) { return state.getParameter (id); };
    input = get (ids::input);
    direct = get (ids::direct);
    drive = get (ids::drive);
    distLevel = get (ids::distLevel);
    attack = get (ids::attack);
    release = get (ids::release);
    bass = get (ids::bass);
    mid = get (ids::mid);
    midFreq = get (ids::midFreq);
    treble = get (ids::treble);
    lowPass = get (ids::lowPass);
    output = get (ids::output);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (get (ids::bypass));
}

bool DynamicDistortionProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
    return (in == stereo && out == stereo) || (in == mono && out == mono) || (in == mono && out == stereo);
}

dd::Settings DynamicDistortionProcessor::readSettings() const
{
    dd::Settings s;
    s.inputDb = value (input);
    s.directPercent = value (direct);
    s.drivePercent = value (drive);
    s.distPercent = value (distLevel);
    s.attackMs = value (attack);
    s.releaseMs = value (release);
    s.bass = value (bass);
    s.mid = value (mid);
    s.midFreq = value (midFreq);
    s.treble = value (treble);
    s.lowPassHz = value (lowPass);
    s.outputDb = value (output);
    s.bypass = bypassParam != nullptr && bypassParam->get();
    return s;
}

void DynamicDistortionProcessor::prepareToPlay (double sampleRate, int)
{
    dsp.setSettings (readSettings());
    dsp.prepare (sampleRate);
    setLatencySamples (dsp.getLatencySamples());
}

template <typename Sample>
void DynamicDistortionProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int inCh = mainIn.getNumChannels(), outCh = mainOut.getNumChannels();
    if (n == 0 || outCh == 0)
        return;

    // mono input feeding a stereo output: both channels process the same signal
    if (inCh == 1 && outCh > 1)
        mainOut.copyFrom (1, 0, mainOut, 0, 0, n);
    const int channels = std::min (outCh, 2);

    for (int c = 0; c < channels; ++c)
    {
        const float pk = float (mainOut.getMagnitude (c, 0, n));
        if (pk > live.inPeak[c].load())
            live.inPeak[c].store (pk);
    }

    dsp.setSettings (readSettings());
    Sample* ptrs[2] = { mainOut.getWritePointer (0), channels > 1 ? mainOut.getWritePointer (1) : nullptr };
    dsp.process (ptrs, channels, n);

    for (int c = 0; c < channels; ++c)
    {
        const float pk = float (mainOut.getMagnitude (c, 0, n));
        if (pk > live.outPeak[c].load())
            live.outPeak[c].store (pk);
    }
    live.expansionDb.store (float (std::max (dsp.getExpansionDb (0), channels > 1 ? dsp.getExpansionDb (1) : 0.0)));

    for (int c = 2; c < outCh; ++c)
        mainOut.clear (c, 0, n);
}

void DynamicDistortionProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void DynamicDistortionProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

void DynamicDistortionProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DynamicDistortionProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* DynamicDistortionProcessor::createEditor()
{
    return new DynamicDistortionEditor (*this);
}

} // namespace craft::ddp

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::ddp::DynamicDistortionProcessor();
}
