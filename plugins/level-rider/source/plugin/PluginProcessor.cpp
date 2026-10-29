#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

#ifndef LEVEL_RIDER_LIVE
 #define LEVEL_RIDER_LIVE 0
#endif

namespace craft::levelrider
{

namespace
{
void raise (std::atomic<float>& a, float v) noexcept
{
    float prev = a.load (std::memory_order_relaxed);
    while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
}

bool isMonoOrStereo (const juce::AudioChannelSet& s)
{
    return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo();
}
} // namespace

LevelRiderProcessor::LevelRiderProcessor (bool isLive)
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Music", juce::AudioChannelSet::stereo(), false)),
      live (isLive),
      state (*this, nullptr, "CraftLevelRiderState", createParameterLayout (isLive))
{
    target = state.getParameter (ids::target);
    vocalSens = state.getParameter (ids::vocalSens);
    musicSens = state.getParameter (ids::musicSens);
    rangeMax = state.getParameter (ids::rangeMax);
    rangeMin = state.getParameter (ids::rangeMin);
    idle = state.getParameter (ids::idle);
    rider = state.getParameter (ids::rider);
    output = state.getParameter (ids::output);
    attack = state.getParameter (ids::attack);
    spill = live ? state.getParameter (ids::spill) : nullptr;
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (ids::bypass));
    state.state.setProperty (ids::automationMode, int (dsp::AutomationMode::Off), nullptr);

    engine.setSettings (readSettings());
    engine.prepare (48000.0);
    startTimerHz (30);
}

LevelRiderProcessor::~LevelRiderProcessor()
{
    stopTimer();
}

const juce::String LevelRiderProcessor::getName() const
{
    return live ? "Craft Level Rider Live" : "Craft Level Rider";
}

bool LevelRiderProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out || ! isMonoOrStereo (in))
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && ! isMonoOrStereo (sc))
            return false;
    }
    return true;
}

dsp::Settings LevelRiderProcessor::readSettings() noexcept
{
    namespace pm = dsp::param;
    dsp::Settings s;
    s.targetDb = pm::Target::fromNorm (normalised (target));
    s.vocalSensDb = pm::VocalSens::fromNorm (normalised (vocalSens));
    const double ms = normalised (musicSens);
    s.musicOff = pm::MusicSens::isOff (ms);
    s.musicSensDb = pm::MusicSens::fromNorm (ms);
    s.idleDb = pm::Idle::fromNorm (normalised (idle));
    s.outputDb = pm::Output::fromNorm (normalised (output));
    s.fast = pm::attackIsFast (normalised (attack));
    s.spillEnabled = live;
    s.spillDb = pm::Spill::fromNorm (normalised (spill));

    // Section 2: Range Min stays >= 1 dB below Range Max; the most recently changed control wins.
    const float minRaw = normalised (rangeMin), maxRaw = normalised (rangeMax);
    if (maxRaw != lastMaxRaw)
        maxChangedLast = true;
    else if (minRaw != lastMinRaw)
        maxChangedLast = false;
    lastMinRaw = minRaw;
    lastMaxRaw = maxRaw;
    const auto rr = dsp::resolveRange (pm::RangeMin::fromNorm (minRaw), pm::RangeMax::fromNorm (maxRaw), maxChangedLast);
    s.rangeMinDb = rr.minDb;
    s.rangeMaxDb = rr.maxDb;
    resolvedMin = float (rr.minDb);
    resolvedMax = float (rr.maxDb);
    return s;
}

void LevelRiderProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setSettings (readSettings());
    engine.setAutomationMode (getAutomationMode());
    engine.setHostRiderDb (dsp::param::Rider::fromNorm (normalised (rider)));
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());
    engine.prepare (sampleRate);
    setLatencySamples (dsp::RiderEngine::getLatencySamples());
}

template <typename Sample>
void LevelRiderProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    auto main = getBusBuffer (buffer, true, 0);
    const int numCh = std::min (2, main.getNumChannels());
    if (numCh == 0 || n == 0)
        return;

    // optional music sidechain (section 8)
    const Sample* sc[2] = { nullptr, nullptr };
    int numSc = 0;
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            auto scBuf = getBusBuffer (buffer, true, 1);
            numSc = std::min (2, scBuf.getNumChannels());
            for (int c = 0; c < numSc; ++c)
                sc[c] = scBuf.getReadPointer (c);
        }

    engine.setSettings (readSettings());
    engine.setAutomationMode (getAutomationMode());
    engine.setHostRiderDb (dsp::param::Rider::fromNorm (normalised (rider)));
    engine.setOverride (overrideActive.load(), overrideDb.load());
    engine.setBypassed (bypassParam != nullptr && bypassParam->get());

    engine.process (main.getArrayOfWritePointers(), numCh, numSc > 0 ? sc : nullptr, numSc, n);

    // clear any output channel beyond the processed ones (never expected with in == out layouts)
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    float peak = 0.0f;
    for (int c = 0; c < numCh; ++c)
        peak = std::max (peak, float (main.getMagnitude (c, 0, n)));
    raise (dispOutPeak, peak);
    if (peak > 1.0f)
        clipLatch = true;
    dispGain = float (engine.getGainDb());
    dispLevel = float (engine.getSmoothLevelDb());
    dispMusic = float (engine.getMusicOffsetDb());
    dispRiding = engine.isRiding();
    dispSidechain = numSc > 0;
}

void LevelRiderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void LevelRiderProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

LevelRiderProcessor::Display LevelRiderProcessor::takeDisplay() noexcept
{
    Display d;
    d.gainDb = dispGain.load();
    d.vocalLevelDb = dispLevel.load();
    d.musicOffsetDb = dispMusic.load();
    d.riding = dispRiding.load();
    d.sidechainActive = dispSidechain.load();
    d.outputPeak = dispOutPeak.exchange (0.0f);
    d.clipped = clipLatch.load();
    d.rangeMinDb = resolvedMin.load();
    d.rangeMaxDb = resolvedMax.load();
    return d;
}

// ---- section 9 ---------------------------------------------------------------------------------
void LevelRiderProcessor::setAutomationMode (dsp::AutomationMode m)
{
    automationMode = int (m);
    state.state.setProperty (ids::automationMode, int (m), nullptr);
}

void LevelRiderProcessor::beginRiderDrag (double db)
{
    overrideDb = float (juce::jlimit (-12.0, 12.0, db));
    overrideActive = true;
    dispGain = overrideDb.load();
    if (getAutomationMode() == dsp::AutomationMode::Write && ! writeGestureOpen && ! dragGestureOpen)
    {
        rider->beginChangeGesture();
        dragGestureOpen = true;
    }
    timerCallback();
}

void LevelRiderProcessor::dragRider (double db)
{
    overrideDb = float (juce::jlimit (-12.0, 12.0, db));
    dispGain = overrideDb.load();
    timerCallback();
}

void LevelRiderProcessor::endRiderDrag()
{
    overrideActive = false;
    if (dragGestureOpen)
    {
        rider->endChangeGesture();
        dragGestureOpen = false;
    }
}

void LevelRiderProcessor::timerCallback()
{
    // Range rule: push the losing control by the 1 dB minimum distance (message thread).
    auto push = [] (juce::RangedAudioParameter* p, float physical) {
        if (p != nullptr && std::abs (p->convertFrom0to1 (p->getValue()) - physical) > 0.05f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
            p->endChangeGesture();
        }
    };
    push (rangeMin, resolvedMin.load());
    push (rangeMax, resolvedMax.load());

    // Rider parameter (section 9)
    const auto mode = getAutomationMode();
    const float g = overrideActive.load() ? overrideDb.load() : dispGain.load();
    const float current = rider->convertFrom0to1 (rider->getValue());
    if (mode == dsp::AutomationMode::Write)
    {
        if (! writeGestureOpen && ! dragGestureOpen)
        {
            rider->beginChangeGesture();
            writeGestureOpen = true;
        }
        if (std::abs (current - g) >= 0.05f)
            rider->setValueNotifyingHost (rider->convertTo0to1 (g));
    }
    else
    {
        if (writeGestureOpen)
        {
            rider->endChangeGesture();
            writeGestureOpen = false;
        }
        // Off: the value follows g silently (readable by the host, never recorded as automation).
        // Read: the host's automation drives g, so the parameter is left alone.
        if (mode == dsp::AutomationMode::Off && std::abs (current - g) >= 0.05f)
            rider->setValue (rider->convertTo0to1 (g));
    }
}

// ---- state ---------------------------------------------------------------------------------------
void LevelRiderProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto copy = state.copyState();
    copy.setProperty (ids::automationMode, automationMode.load(), nullptr);
    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, destData);
}

void LevelRiderProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const int m = juce::jlimit (0, 2, int (tree.getProperty (ids::automationMode, 0)));
            state.replaceState (tree);
            automationMode = m;
        }
}

juce::AudioProcessorEditor* LevelRiderProcessor::createEditor()
{
    return new LevelRiderEditor (*this);
}

} // namespace craft::levelrider

// Plug-in entry point: the product (standard / Live) is selected at compile time per target.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::levelrider::LevelRiderProcessor (LEVEL_RIDER_LIVE != 0);
}
