#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::fet_limiter
{

namespace
{
void storeMax (std::atomic<float>& a, float v) noexcept
{
    float prev = a.load (std::memory_order_relaxed);
    while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
}
} // namespace

FetLimiterProcessor::FetLimiterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftFetLimiterState", createParameterLayout())
{
    auto get = [this] (const char* id) { return state.getParameter (id); };
    input = get (ids::input);
    output = get (ids::output);
    attack = get (ids::attack);
    release = get (ids::release);
    ratio = get (ids::ratio);
    analog = get (ids::analog);
    revision = get (ids::revision);
    compOff = get (ids::compOff);
    mix = get (ids::mix);
    trim = get (ids::trim);

    engine.setSettings (readSettings());
    engine.prepare (48000.0);
}

bool FetLimiterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo (linked detector) or mono; input and output layouts must match.
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

dsp::Settings FetLimiterProcessor::readSettings() const
{
    auto value = [] (juce::RangedAudioParameter* p) { return double (p->convertFrom0to1 (p->getValue())); };
    auto index = [] (juce::RangedAudioParameter* p) { return juce::roundToInt (p->convertFrom0to1 (p->getValue())); };
    dsp::Settings s;
    s.inputDisplay = value (input);
    s.outputDisplay = value (output);
    s.attack = value (attack);
    s.release = value (release);
    s.ratio = dsp::Ratio (juce::jlimit (0, 4, index (ratio)));
    s.analog = dsp::Analog (juce::jlimit (0, 2, index (analog)));
    s.revision = index (revision) <= 0 ? dsp::Revision::B : dsp::Revision::A;
    s.compOff = compOff->getValue() >= 0.5f;
    s.mixPercent = value (mix);
    s.trimDb = value (trim);
    return s;
}

void FetLimiterProcessor::prepareToPlay (double sampleRate, int)
{
    numChannels = juce::jlimit (1, 2, getMainBusNumOutputChannels());
    engine.setSettings (readSettings());
    engine.prepare (sampleRate);
    setLatencySamples (0); // spec 1
}

template <typename Sample>
void FetLimiterProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() == 0)
        return;
    const int ch = juce::jlimit (1, 2, std::min (buffer.getNumChannels(), getMainBusNumOutputChannels()));

    engine.setSettings (readSettings());
    engine.resetMeters();
    engine.process (buffer.getArrayOfWritePointers(), ch, n);

    storeMax (inPeak, float (engine.getInputPeak()));
    storeMax (outPeak, float (engine.getOutputPeak()));
    storeMax (maxGr, float (engine.getMaxGainReductionDb()));
    for (int c = ch; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
}

void FetLimiterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void FetLimiterProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

FetLimiterProcessor::MeterData FetLimiterProcessor::takeMeters() noexcept
{
    MeterData m;
    m.inputPeak = inPeak.exchange (0.0f);
    m.outputPeak = outPeak.exchange (0.0f);
    m.grDb = maxGr.exchange (0.0f);
    return m;
}

void FetLimiterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FetLimiterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FetLimiterProcessor::createEditor()
{
    return new FetLimiterEditor (*this);
}

} // namespace craft::fet_limiter

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::fet_limiter::FetLimiterProcessor();
}
