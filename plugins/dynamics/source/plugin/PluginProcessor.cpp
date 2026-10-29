#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

#ifndef CRAFT_DYNAMICS_VARIANT
 #define CRAFT_DYNAMICS_VARIANT 0
#endif

namespace craft::dynamics
{

DynamicsProcessor::DynamicsProcessor (dsp::Variant v)
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      variant (v),
      state (*this, nullptr, "CraftDynamicsState", createParameterLayout (v)),
      engine (v)
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (ids::bypass));
    for (auto* id : { ids::lookahead, ids::linkThreshold, ids::linkAttack, ids::linkRelease, ids::compThreshold, ids::gateOpen,
                      ids::compAttack, ids::gateAttack, ids::compRelease, ids::gateRelease })
        if (state.getParameter (id) != nullptr)
            state.addParameterListener (id, this);
    captureLinkValues();
}

DynamicsProcessor::~DynamicsProcessor()
{
    cancelPendingUpdate();
    for (auto* id : { ids::lookahead, ids::linkThreshold, ids::linkAttack, ids::linkRelease, ids::compThreshold, ids::gateOpen,
                      ids::compAttack, ids::gateAttack, ids::compRelease, ids::gateRelease })
        if (state.getParameter (id) != nullptr)
            state.removeParameterListener (id, this);
}

const juce::String DynamicsProcessor::getName() const
{
    switch (variant)
    {
        case dsp::Variant::Full:             return "Craft Dynamics";
        case dsp::Variant::Compressor:       return "Craft Compressor";
        case dsp::Variant::Gate:             return "Craft Gate";
        case dsp::Variant::CompressorFilter: return "Craft Compressor SC";
    }
    return "Craft Dynamics";
}

bool DynamicsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // stereo (primary) or mono (same engine on one channel, spec 1 / D7); input and output must match
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

float DynamicsProcessor::norm (const char* id) const noexcept
{
    auto* p = state.getParameter (id);
    return p != nullptr ? p->getValue() : 0.0f;
}

int DynamicsProcessor::choice (const char* id) const noexcept
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
        return p->getIndex();
    return 0;
}

double DynamicsProcessor::physical (const char* id, double fallback) const noexcept
{
    auto* p = state.getParameter (id);
    return p != nullptr ? double (p->convertFrom0to1 (p->getValue())) : fallback;
}

dsp::Settings DynamicsProcessor::readSettings() const
{
    dsp::Settings s;
    auto gateDb = [] (double v) { return dsp::param::isMinusInf (v) ? dsp::minusInfinityDb : v; };
    s.compThresholdDb = physical (ids::compThreshold, 0.0);
    s.ratioPos = physical (ids::ratio, dsp::ratio::neutralPos);
    s.compAttackMs = physical (ids::compAttack, 2.0);
    s.compReleaseMs = physical (ids::compRelease, 50.0);
    s.pdrMs = physical (ids::pdr, 0.0);
    s.makeupDb = physical (ids::makeup, 0.0);
    s.peakRef = choice (ids::levelRef) == 1;
    s.compBypass = isOn (ids::compBypass);
    s.compEq = dsp::EqMode (choice (ids::compEq));
    s.expander = choice (ids::gateMode) == 1;
    s.gateOpenDb = gateDb (physical (ids::gateOpen, -100.0));
    s.gateCloseDb = gateDb (physical (ids::gateClose, -100.0));
    s.gateAttackMs = physical (ids::gateAttack, 2.0);
    s.gateReleaseMs = physical (ids::gateRelease, 30.0);
    s.holdMs = physical (ids::hold, 0.01);
    s.floorValue = physical (ids::floor, dsp::floorvalue::offValue);
    s.gateBypass = isOn (ids::gateBypass);
    s.gateEq = dsp::EqMode (choice (ids::gateEq));
    s.filterType = dsp::FilterType (choice (ids::filterType));
    s.filterFreqHz = physical (ids::filterFreq, 7179.0);
    s.filterQ = physical (ids::filterQ, 0.6);
    s.monitor = dsp::Monitor (choice (ids::monitor));
    s.key = dsp::KeyMode (choice (ids::keyMode));
    s.lookahead = isOn (ids::lookahead);
    s.inputGainLDb = physical (ids::inputGainL, 0.0);
    s.inputGainRDb = physical (ids::inputGainR, 0.0);
    s.outputGainDb = physical (ids::outputGain, 0.0);

    if (isOn (ids::bypass))
    {
        // plug-in bypass: neutral path (crossfaded by the engine); the lookahead delay stays so latency is constant
        s.compBypass = s.gateBypass = true;
        s.monitor = dsp::Monitor::Audio;
        s.key = dsp::KeyMode::Stereo;
        s.inputGainLDb = s.inputGainRDb = s.outputGainDb = 0.0;
    }
    return s;
}

void DynamicsProcessor::prepareToPlay (double sampleRate, int)
{
    numChannels = juce::jlimit (1, 2, getTotalNumOutputChannels());
    sampleRateUsed = sampleRate;
    engine.setSettings (readSettings());
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
}

template <typename Sample>
void DynamicsProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    if (n == 0 || nch == 0)
        return;

    engine.setSettings (readSettings());

    for (int c = 0; c < nch; ++c)
    {
        const float pk = float (buffer.getMagnitude (c, 0, n));
        float prev = inPeaks[c].load (std::memory_order_relaxed);
        while (pk > prev && ! inPeaks[c].compare_exchange_weak (prev, pk, std::memory_order_relaxed)) {}
    }

    engine.process (buffer.getWritePointer (0), nch > 1 ? buffer.getWritePointer (1) : static_cast<Sample*> (nullptr), n);

    for (int c = 0; c < nch; ++c)
    {
        const float pk = float (buffer.getMagnitude (c, 0, n));
        float prev = outPeaks[c].load (std::memory_order_relaxed);
        while (pk > prev && ! outPeaks[c].compare_exchange_weak (prev, pk, std::memory_order_relaxed)) {}
    }
    for (int c = nch; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    const auto m = engine.getMeters();
    compLevel.store (float (m.compLevelDb), std::memory_order_relaxed);
    compGain.store (float (m.compGainDb), std::memory_order_relaxed);
    gateLevel.store (float (m.gateLevelDb), std::memory_order_relaxed);
    gateGain.store (float (m.gateGainDb), std::memory_order_relaxed);
    gateOpenFlag.store (m.gateOpen, std::memory_order_relaxed);
}

void DynamicsProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processAudio (b); }
void DynamicsProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processAudio (b); }

DynamicsProcessor::MeterView DynamicsProcessor::takeMeters() noexcept
{
    MeterView v;
    v.compLevelDb = compLevel.load();
    v.compGainDb = compGain.load();
    v.gateLevelDb = gateLevel.load();
    v.gateGainDb = gateGain.load();
    v.gateOpen = gateOpenFlag.load();
    for (int c = 0; c < 2; ++c)
    {
        v.inPeak[c] = inPeaks[c].exchange (0.0f);
        v.outPeak[c] = outPeaks[c].exchange (0.0f);
    }
    return v;
}

// ---------------------------------------------------------------------------------------------
// Latency follows LookAhead; link offsets move the partner control (spec 2). Both on the message thread.
void DynamicsProcessor::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate();
}

void DynamicsProcessor::captureLinkValues()
{
    for (auto& l : links)
    {
        l.lastA = norm (l.a);
        l.lastB = norm (l.b);
    }
}

void DynamicsProcessor::handleAsyncUpdate()
{
    const int latency = (getInfo().filter && isOn (ids::lookahead)) ? dsp::DynamicsEngine::lookaheadSamples : 0;
    if (latency != getLatencySamples())
        setLatencySamples (latency);

    if (applyingLink.load())
        return;
    applyingLink = true;
    for (auto& l : links)
    {
        auto* pa = state.getParameter (l.a);
        auto* pb = state.getParameter (l.b);
        if (pa == nullptr || pb == nullptr || state.getParameter (l.link) == nullptr)
            continue;
        const float a = pa->getValue(), b = pb->getValue();
        if (isOn (l.link))
        {
            const bool aMoved = a != l.lastA, bMoved = b != l.lastB;
            auto move = [] (juce::RangedAudioParameter* p, float target) {
                target = juce::jlimit (0.0f, 1.0f, target);
                if (std::abs (target - p->getValue()) < 1.0e-6f)
                    return;
                p->beginChangeGesture();
                p->setValueNotifyingHost (target);
                p->endChangeGesture();
            };
            if (aMoved && ! bMoved)
                move (pb, b + (a - l.lastA));
            else if (bMoved && ! aMoved)
                move (pa, a + (b - l.lastB));
        }
        l.lastA = pa->getValue();
        l.lastB = pb->getValue();
    }
    applyingLink = false;
}

// ---------------------------------------------------------------------------------------------
void DynamicsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DynamicsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            applyingLink = true;
            state.replaceState (juce::ValueTree::fromXml (*xml));
            captureLinkValues();
            applyingLink = false;
            triggerAsyncUpdate();
        }
}

juce::AudioProcessorEditor* DynamicsProcessor::createEditor()
{
    return new DynamicsEditor (*this);
}

} // namespace craft::dynamics

// Plug-in entry point: the variant is selected at compile time per target.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::dynamics::DynamicsProcessor (static_cast<craft::dynamics::dsp::Variant> (CRAFT_DYNAMICS_VARIANT));
}
