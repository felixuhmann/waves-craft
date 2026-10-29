#include "PluginProcessor.h"

#include "dsp/Parameters.h"
#include "ui/PluginEditor.h"

#ifndef CRAFT_FLANGER_VARIANT
 #define CRAFT_FLANGER_VARIANT 0
#endif

namespace craft::flanger
{

juce::AudioProcessor::BusesProperties FlangerProcessor::busesFor (dsp::Variant v)
{
    const auto in = v == dsp::Variant::Stereo ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono();
    const auto out = v == dsp::Variant::Mono ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    return BusesProperties().withInput ("Input", in, true).withOutput ("Output", out, true);
}

FlangerProcessor::FlangerProcessor (dsp::Variant v)
    : AudioProcessor (busesFor (v)),
      variant (v),
      state (*this, nullptr, "CraftFlangerState", createParameterLayout (v, tempoSyncShown)),
      engine (v)
{
    auto get = [this] (const char* id) { return state.getParameter (id); };
    delay = get (ids::delay);
    depth = get (ids::depth);
    rate = get (ids::rate);
    mix = get (ids::mix);
    feedback = get (ids::feedback);
    gain = get (ids::gain);
    stereo = get (ids::stereo);
    filterFreq = get (ids::filterFreq);
    filterType = get (ids::filterType);
    filterOn = get (ids::filterOn);
    waveform = get (ids::waveform);
    wetInvert = get (ids::wetInvert);
    feedbackInvert = get (ids::feedbackInvert);
    tape = get (ids::tape);
    stop = get (ids::stop);
    link = get (ids::link);
    tempoSync = get (ids::tempoSync);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (get (ids::bypass));

    for (const char* id : { ids::rate, ids::depth, ids::link, ids::tempoSync })
        state.addParameterListener (id, this);
    linkState.capture (dsp::param::Rate::defaultValue, dsp::param::Depth::defaultValue);
}

FlangerProcessor::~FlangerProcessor()
{
    cancelPendingUpdate();
    for (const char* id : { ids::rate, ids::depth, ids::link, ids::tempoSync })
        state.removeParameterListener (id, this);
}

const juce::String FlangerProcessor::getName() const
{
    switch (variant)
    {
        case dsp::Variant::Stereo:       return "Craft Flanger";
        case dsp::Variant::Mono:         return "Craft Flanger Mono";
        case dsp::Variant::MonoToStereo: return "Craft Flanger Mono-Stereo";
    }
    return "Craft Flanger";
}

bool FlangerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto expected = busesFor (variant);
    return layouts.getMainInputChannelSet() == expected.inputLayouts.getReference (0).defaultLayout
        && layouts.getMainOutputChannelSet() == expected.outputLayouts.getReference (0).defaultLayout;
}

dsp::Settings FlangerProcessor::readSettings() const
{
    namespace pm = dsp::param;
    dsp::Settings s;
    s.delayMs = pm::Delay::fromNorm (normalised (delay));
    s.depthPct = pm::Depth::fromNorm (normalised (depth));
    s.rateHz = pm::Rate::fromNorm (normalised (rate));
    s.mixPct = pm::Mix::fromNorm (normalised (mix));
    s.feedbackPct = pm::Feedback::fromNorm (normalised (feedback));
    s.gainDb = pm::Gain::fromNorm (normalised (gain));
    s.stereoDeg = stereo != nullptr ? pm::Stereo::fromNorm (normalised (stereo)) : 0.0;
    s.filterHz = pm::FilterFreq::fromNorm (normalised (filterFreq));
    s.filterHighPass = isOn (filterType);
    s.filterOn = isOn (filterOn);
    s.triangle = isOn (waveform);
    s.wetInvert = isOn (wetInvert);
    s.feedbackInvert = isOn (feedbackInvert);
    s.tape = isOn (tape);
    s.stop = isOn (stop);
    s.tempoSync = isOn (tempoSync);
    s.bpm = lastBpm;
    return s;
}

void FlangerProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setSettings (readSettings());
    engine.setBypassed (isOn (bypassParam));
    engine.reset(); // theta = 0 at the first processed sample (spec 5.2)
    wasPlaying = false;
    setLatencySamples (dsp::FlangerEngine::getLatencySamples());
}

template <typename Sample>
void FlangerProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int outCh = dsp::numOutputChannels (variant);
    if (buffer.getNumChannels() < outCh || n == 0)
        return;

    // Transport: tempo for Tempo Sync, LFO restart at every start of playback (spec 9).
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 0.0)
                lastBpm = *bpm;
            const bool playing = pos->getIsPlaying();
            if (playing && ! wasPlaying)
                engine.restartLfo();
            wasPlaying = playing;
        }

    engine.setSettings (readSettings());
    engine.setBypassed (isOn (bypassParam));

    auto* c0 = buffer.getWritePointer (0);
    auto* c1 = outCh > 1 ? buffer.getWritePointer (1) : nullptr;
    engine.process (c0, c1, n);

    for (int ch = 0; ch < outCh; ++ch)
    {
        const float pk = float (buffer.getMagnitude (ch, 0, n));
        float prev = peaks[ch].load (std::memory_order_relaxed);
        while (pk > prev && ! peaks[ch].compare_exchange_weak (prev, pk, std::memory_order_relaxed)) {}
    }
    for (int ch = outCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    for (int ch = 0; ch < 2; ++ch)
    {
        double p = engine.getPhase() + (ch == 1 ? engine.getRightPhaseOffset() : 0.0);
        viewPhase[ch].store (float (p - std::floor (p)), std::memory_order_relaxed);
        viewValue[ch].store (float (engine.getLfoValue (ch)), std::memory_order_relaxed);
        viewDelay[ch].store (float (engine.getDelaySamples (ch) * 1000.0 / fs), std::memory_order_relaxed);
    }
}

void FlangerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void FlangerProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

std::array<float, 2> FlangerProcessor::takePeaks() noexcept
{
    return { peaks[0].exchange (0.0f), peaks[1].exchange (0.0f) };
}

FlangerProcessor::LfoView FlangerProcessor::getLfoView() const noexcept
{
    LfoView v;
    for (int ch = 0; ch < 2; ++ch)
    {
        v.phase[ch] = viewPhase[ch].load (std::memory_order_relaxed);
        v.value[ch] = viewValue[ch].load (std::memory_order_relaxed);
        v.delayMs[ch] = viewDelay[ch].load (std::memory_order_relaxed);
    }
    return v;
}

// ---------------------------------------------------------------------------------------------
// Link (spec 5.5): coupling happens on the message thread, for GUI and host changes alike.
void FlangerProcessor::parameterChanged (const juce::String& id, float)
{
    if (id == ids::tempoSync)
    {
        tempoSyncShown->store (isOn (tempoSync));
        return;
    }
    if (applyingLink.load())
        return;
    if (id == ids::rate)
        pendingRate = true;
    else if (id == ids::depth)
        pendingDepth = true;
    else if (id == ids::link)
        pendingCapture = true;
    triggerAsyncUpdate();
}

void FlangerProcessor::setPhysical (juce::RangedAudioParameter* p, double value)
{
    if (p == nullptr)
        return;
    const float norm = p->convertTo0to1 (float (value));
    if (std::abs (norm - p->getValue()) < 1.0e-6f)
        return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (norm);
    p->endChangeGesture();
}

void FlangerProcessor::handleAsyncUpdate()
{
    namespace pm = dsp::param;
    const bool r = pendingRate.exchange (false), d = pendingDepth.exchange (false), cap = pendingCapture.exchange (false);
    const double rateNow = pm::Rate::fromNorm (normalised (rate)), depthNow = pm::Depth::fromNorm (normalised (depth));

    if (! isOn (link) || cap || (r && d) || linkState.getProduct() <= 0.0)
    {
        linkState.capture (rateNow, depthNow);
        return;
    }

    applyingLink = true;
    if (r)
    {
        const double nd = linkState.depthForRate (rateNow);
        if (nd >= 0.0)
            setPhysical (depth, nd);
    }
    else if (d)
    {
        const double nr = linkState.rateForDepth (depthNow);
        if (nr >= 0.0)
            setPhysical (rate, nr);
    }
    applyingLink = false;
}

// ---------------------------------------------------------------------------------------------
void FlangerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FlangerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            applyingLink = true;
            state.replaceState (juce::ValueTree::fromXml (*xml));
            applyingLink = false;
            tempoSyncShown->store (isOn (tempoSync));
            pendingCapture = true;
            triggerAsyncUpdate();
        }
}

juce::AudioProcessorEditor* FlangerProcessor::createEditor()
{
    return new FlangerEditor (*this);
}

} // namespace craft::flanger

// Plug-in entry point: the variant is selected at compile time per target.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::flanger::FlangerProcessor (static_cast<craft::flanger::dsp::Variant> (CRAFT_FLANGER_VARIANT));
}
