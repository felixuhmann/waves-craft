#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::band_limiter
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

double physical (juce::RangedAudioParameter* p) { return double (p->convertFrom0to1 (p->getValue())); }
int indexOf (juce::RangedAudioParameter* p) { return juce::roundToInt (p->convertFrom0to1 (p->getValue())); }
} // namespace

BandLimiterProcessor::BandLimiterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftBandLimiterState", createParameterLayout (variant))
{
    auto get = [this] (const juce::String& id) { return state.getParameter (id); };
    threshold = get (ids::threshold);
    ceiling = get (ids::ceiling);
    separation = get (ids::separation);
    behaviour = get (ids::behaviour);
    multiplier = get (ids::multiplier);
    profile = get (ids::profile);
    quantize = get (ids::quantize);
    dither = get (ids::dither);
    shaping = get (ids::shaping);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (get (ids::bypass));
    for (int b = 0; b < dsp::numBands; ++b)
    {
        gain[size_t (b)] = get (ids::gain (b));
        priority[size_t (b)] = get (ids::priority (b));
        release[size_t (b)] = get (ids::release (b));
        solo[size_t (b)] = get (ids::solo (b));
        bandGainMeter[size_t (b)] = 1.0f;
        bandPeakMeter[size_t (b)] = 0.0f;
    }
    for (int k = 0; k < dsp::numCrossovers; ++k)
        crossover[size_t (k)] = get (ids::crossover (k));

    engine.setSettings (readSettings());
    engine.prepare (48000.0);
    setLatencySamples (engine.getLatencySamples());
}

bool BandLimiterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

double BandLimiterProcessor::getTailLengthSeconds() const
{
    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    return double (engine.getLatencySamples()) / fs + 0.1;
}

dsp::Settings BandLimiterProcessor::readSettings() const
{
    dsp::Settings s;
    s.thresholdDb = physical (threshold);
    s.ceilingDb = physical (ceiling);
    s.wordLength = dsp::wordLengths[juce::jlimit (0, 4, indexOf (quantize))];
    s.dither = dsp::Dither (juce::jlimit (0, 2, indexOf (dither)));
    s.shaping = dsp::Shaping (juce::jlimit (0, 3, indexOf (shaping)));

    if (dsp::isProfileVariant (variant))
    {
        dsp::applyProfile (s, juce::jlimit (0, 8, indexOf (profile)) + 1, physical (multiplier));
        return s;
    }
    for (int b = 0; b < dsp::numBands; ++b)
    {
        const auto ub = size_t (b);
        s.gainDb[ub] = physical (gain[ub]);
        s.priority[ub] = physical (priority[ub]);
        s.releaseMs[ub] = physical (release[ub]);
        s.solo[ub] = solo[ub]->getValue() >= 0.5f;
    }
    for (int k = 0; k < dsp::numCrossovers; ++k)
        s.crossoverHz[size_t (k)] = physical (crossover[size_t (k)]);
    s.separation = physical (separation) * 0.01;
    s.behaviour = behaviour != nullptr ? dsp::Behaviour (juce::jlimit (0, 4, indexOf (behaviour))) : dsp::Behaviour::Adaptive;
    return s;
}

void BandLimiterProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setSettings (readSettings());
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples()); // spec 2: 3528 (linear phase) / 64 (low latency) at every rate
}

template <typename Sample>
void BandLimiterProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ch = juce::jlimit (1, 2, std::min (buffer.getNumChannels(), getMainBusNumOutputChannels()));
    if (n == 0 || buffer.getNumChannels() == 0)
        return;

    engine.setSettings (readSettings());
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());
    engine.resetMeters();
    engine.process (buffer.getArrayOfWritePointers(), ch, n);

    storeMax (inPeak, float (std::max (engine.getInputPeak (0), engine.getInputPeak (1))));
    storeMax (outPeak, float (std::max (engine.getOutputPeak (0), engine.getOutputPeak (1))));
    storeMin (minGain, float (engine.getMinGain()));
    for (int b = 0; b < dsp::numBands; ++b)
    {
        storeMin (bandGainMeter[size_t (b)], float (engine.getBandMinGain (b)));
        storeMax (bandPeakMeter[size_t (b)], float (engine.getBandPeak (b)));
    }
    for (int c = ch; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
}

void BandLimiterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void BandLimiterProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

BandLimiterProcessor::MeterData BandLimiterProcessor::takeMeters() noexcept
{
    MeterData m;
    m.input = inPeak.exchange (0.0f);
    m.output = outPeak.exchange (0.0f);
    m.minGain = minGain.exchange (1.0f);
    for (int b = 0; b < dsp::numBands; ++b)
    {
        m.bandGain[size_t (b)] = bandGainMeter[size_t (b)].exchange (1.0f);
        m.bandPeak[size_t (b)] = bandPeakMeter[size_t (b)].exchange (0.0f);
    }
    return m;
}

void BandLimiterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BandLimiterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BandLimiterProcessor::createEditor()
{
    return new BandLimiterEditor (*this);
}

} // namespace craft::band_limiter

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::band_limiter::BandLimiterProcessor();
}
