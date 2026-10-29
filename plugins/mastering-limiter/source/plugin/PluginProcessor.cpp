#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::mastering_limiter
{

namespace
{
void storeMax (std::atomic<float>& a, float v) noexcept
{
    float prev = a.load (std::memory_order_relaxed);
    while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
}

void storeMin (std::atomic<float>& a, float v) noexcept
{
    float prev = a.load (std::memory_order_relaxed);
    while (v < prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
}
} // namespace

MasteringLimiterProcessor::MasteringLimiterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftMasteringLimiterState", createParameterLayout())
{
    auto get = [this] (const char* id) { return state.getParameter (id); };
    threshold = get (ids::threshold);
    ceiling = get (ids::ceiling);
    release = get (ids::release);
    adaptive = get (ids::adaptive);
    wordLength = get (ids::wordLength);
    dither = get (ids::dither);
    shaping = get (ids::shaping);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (get (ids::bypass));
    state.state.setProperty (linkMarkersProperty, false, nullptr);

    engine.setSettings (readSettings());
    engine.prepare (48000.0);
    setLatencySamples (engine.getLatencySamples());
}

bool MasteringLimiterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo (linked detection) or mono; input and output layouts must match.
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

double MasteringLimiterProcessor::getTailLengthSeconds() const
{
    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    return double (engine.getLatencySamples()) / fs;
}

dsp::Settings MasteringLimiterProcessor::readSettings() const
{
    auto physical = [] (juce::RangedAudioParameter* p) { return double (p->convertFrom0to1 (p->getValue())); };
    auto index = [] (juce::RangedAudioParameter* p) {
        return juce::roundToInt (p->convertFrom0to1 (p->getValue())); // choice parameters: index as value
    };
    dsp::Settings s;
    s.thresholdDb = physical (threshold);
    s.ceilingDb = physical (ceiling);
    s.releaseMs = physical (release);
    s.adaptiveRelease = adaptive->getValue() >= 0.5f;
    s.wordLength = dsp::wordLengthForIndex (index (wordLength));
    s.dither = dsp::Dither (juce::jlimit (0, 2, index (dither)));
    s.shaping = dsp::Shaping (juce::jlimit (0, 3, index (shaping)));
    return s;
}

void MasteringLimiterProcessor::prepareToPlay (double sampleRate, int)
{
    numChannels = juce::jlimit (1, 2, getMainBusNumOutputChannels());
    engine.setSettings (readSettings());
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());
    engine.prepare (sampleRate); // also resets: Threshold / Ceiling start at their values without a ramp
    setLatencySamples (engine.getLatencySamples()); // spec 3: 64 / 128 / 256 samples
}

template <typename Sample>
void MasteringLimiterProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ch = juce::jlimit (1, 2, std::min (buffer.getNumChannels(), getMainBusNumOutputChannels()));
    if (n == 0 || buffer.getNumChannels() == 0)
        return;

    // Word length / dither / shaping switch here, at the block boundary (spec 8).
    engine.setSettings (readSettings());
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());
    engine.resetMeters();
    engine.process (buffer.getArrayOfWritePointers(), ch, n);

    for (int c = 0; c < ch; ++c)
    {
        storeMax (inPeak[c], float (engine.getInputPeak (c)));
        storeMax (outPeak[c], float (engine.getOutputPeak (c)));
    }
    storeMin (minGain, float (engine.getMinGain()));
    for (int c = ch; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
}

void MasteringLimiterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void MasteringLimiterProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

MasteringLimiterProcessor::MeterData MasteringLimiterProcessor::takeMeters() noexcept
{
    MeterData m;
    for (int c = 0; c < 2; ++c)
    {
        m.input[c] = inPeak[c].exchange (0.0f);
        m.output[c] = outPeak[c].exchange (0.0f);
    }
    m.minGain = minGain.exchange (1.0f);
    return m;
}

void MasteringLimiterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MasteringLimiterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MasteringLimiterProcessor::createEditor()
{
    return new MasteringLimiterEditor (*this);
}

} // namespace craft::mastering_limiter

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::mastering_limiter::MasteringLimiterProcessor();
}
