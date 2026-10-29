#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

#ifndef DOUBLER_VOICES
 #define DOUBLER_VOICES 4
#endif

namespace craft::doubler
{

DoublerProcessor::DoublerProcessor (int voices)
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      numVoices (voices >= 4 ? 4 : 2),
      state (*this, nullptr, "CraftDoublerState", createParameterLayout (numVoices)),
      engine (numVoices)
{
    auto get = [this] (const juce::String& id) { return state.getParameter (id); };
    master = get (ids::master);
    fbHighPass = get (ids::fbHighPass);
    modReset = get (ids::modReset);
    range = get (ids::range);
    align = get (ids::align);
    directOn = get (ids::directOn);
    directGain = get (ids::directGain);
    directPan = get (ids::directPan);
    eqLowOn = get (ids::eqLowOn);
    eqLowGain = get (ids::eqLowGain);
    eqLowFreq = get (ids::eqLowFreq);
    eqHighOn = get (ids::eqHighOn);
    eqHighGain = get (ids::eqHighGain);
    eqHighFreq = get (ids::eqHighFreq);
    routingA = get (ids::routingA);
    routingB = get (ids::routingB);
    for (int n = 0; n < numVoices; ++n)
    {
        auto& v = voiceParams[size_t (n)];
        v.on = get (ids::voice (n, ids::on));
        v.gain = get (ids::voice (n, ids::gain));
        v.pan = get (ids::voice (n, ids::pan));
        v.delay = get (ids::voice (n, ids::delay));
        v.feedback = get (ids::voice (n, ids::feedback));
        v.octave = get (ids::voice (n, ids::octave));
        v.detune = get (ids::voice (n, ids::detune));
        v.depth = get (ids::voice (n, ids::depth));
        v.rate = get (ids::voice (n, ids::rate));
    }
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (get (ids::bypass));
}

const juce::String DoublerProcessor::getName() const
{
    return numVoices == 4 ? "Craft Doubler 4" : "Craft Doubler 2";
}

bool DoublerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // stereo in / stereo out; a mono input is fed to both input channels (spec section 1)
    const auto in = layouts.getMainInputChannelSet();
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

Settings DoublerProcessor::readSettings() const
{
    auto value = [] (juce::RangedAudioParameter* p) { return double (p->convertFrom0to1 (p->getValue())); };
    auto on = [] (juce::RangedAudioParameter* p) { return p->getValue() >= 0.5f; };

    Settings s = defaultSettings (numVoices);
    s.masterDb = value (master);
    s.fbHighPassHz = value (fbHighPass);
    s.modReset = on (modReset);
    s.range20 = on (range);
    s.alignDirect = on (align);
    s.directOn = on (directOn);
    s.directGainDb = value (directGain);
    s.directPanDeg = value (directPan);
    s.eqLowOn = on (eqLowOn);
    s.eqLowGainDb = value (eqLowGain);
    s.eqLowFreqHz = value (eqLowFreq);
    s.eqHighOn = on (eqHighOn);
    s.eqHighGainDb = value (eqHighGain);
    s.eqHighFreqHz = value (eqHighFreq);
    s.routingA = static_cast<Routing> (int (std::lround (value (routingA))));
    s.routingB = static_cast<Routing> (int (std::lround (value (routingB))));
    for (int n = 0; n < numVoices; ++n)
    {
        const auto& p = voiceParams[size_t (n)];
        auto& v = s.voices[size_t (n)];
        v.on = on (p.on);
        v.gainDb = value (p.gain);
        v.panDeg = value (p.pan);
        v.delayMs = value (p.delay);
        v.feedbackPct = value (p.feedback);
        v.octave = on (p.octave);
        v.detuneCents = value (p.detune);
        v.depthCents = value (p.depth);
        v.rateHz = value (p.rate);
    }
    s.bypass = bypassParam != nullptr && bypassParam->get();
    return s;
}

void DoublerProcessor::prepareToPlay (double sampleRate, int)
{
    lastSampleRate = sampleRate;
    engine.prepare (sampleRate);
    engine.setSettings (readSettings());
    engine.reset();
    setLatencySamples (DoublerEngine::latencySamples);
}

template <typename Sample>
void DoublerProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;
    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    engine.setSettings (readSettings());

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    engine.process (l, r, n);

    for (int v = 0; v < numVoices; ++v)
        voiceCents[size_t (v)].store (engine.getVoice (v).getCents(), std::memory_order_relaxed);

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

void DoublerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void DoublerProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

void DoublerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DoublerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* DoublerProcessor::createEditor()
{
    return new DoublerEditor (*this);
}

} // namespace craft::doubler

// Plug-in entry point: the number of voices is selected at compile time per target.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::doubler::DoublerProcessor (DOUBLER_VOICES);
}
