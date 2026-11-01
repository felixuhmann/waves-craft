#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::mfp
{

MotionFilterProcessor::MotionFilterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      state (*this, nullptr, "CraftMotionFilterState", createParameterLayout())
{
    auto get = [this] (const juce::String& id) { return state.getParameter (id); };
    type = get (ids::type);
    slope = get (ids::slope);
    freq = get (ids::freq);
    res = get (ids::res);
    spread = get (ids::spread);
    smooth = get (ids::smooth);
    drive = get (ids::drive);
    crush = get (ids::crush);
    input = get (ids::input);
    output = get (ids::output);
    mix = get (ids::mix);
    lfoShape = get (ids::lfoShape);
    lfoRate = get (ids::lfoRate);
    lfoSync = get (ids::lfoSync);
    lfoSyncRate = get (ids::lfoSyncRate);
    seqLast = get (ids::seqLast);
    seqRate = get (ids::seqRate);
    seqSync = get (ids::seqSync);
    seqSyncRate = get (ids::seqSyncRate);
    swing = get (ids::swing);
    dlyTime = get (ids::dlyTime);
    dlySync = get (ids::dlySync);
    dlySyncRate = get (ids::dlySyncRate);
    dlyRatio = get (ids::dlyRatio);
    dlyFeedback = get (ids::dlyFeedback);
    dlyMix = get (ids::dlyMix);
    dlyMode = get (ids::dlyMode);
    dlyRoute = get (ids::dlyRoute);
    folSource = get (ids::folSource);
    kbd = get (ids::kbd);
    for (int k = 0; k < 16; ++k)
        steps[k] = get (ids::step (k));
    for (int s = 0; s < 3; ++s)
        for (int d = 0; d < 3; ++d)
            mods[s][d] = get (ids::mod (s, d));
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (get (ids::bypass));

    state.state.setProperty (ids::drawMode, 0, nullptr);
}

bool MotionFilterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
    const bool mainOk = (in == stereo && out == stereo) || (in == mono && out == stereo) || (in == mono && out == mono);
    if (! mainOk)
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! (sc.isDisabled() || sc == mono || sc == stereo))
            return false;
    }
    return true;
}

mf::Settings MotionFilterProcessor::readSettings() const
{
    namespace pm = mf::param;
    mf::Settings s;
    s.type = static_cast<mf::FilterType> (juce::jlimit (0, mf::numFilterTypes - 1, index (type)));
    s.slope24 = isOn (slope);
    s.freqNorm = freq != nullptr ? double (freq->getValue()) : 1.0; // normalised = knob position r
    s.resonance = value (res);
    s.spread = value (spread);
    s.smooth = value (smooth);
    s.drive = value (drive);
    s.crush = value (crush);
    s.inputDb = value (input);
    s.outputDb = value (output);
    s.mix = value (mix);
    s.lfoShape = static_cast<mf::LfoShape> (juce::jlimit (0, mf::numLfoShapes - 1, index (lfoShape)));
    s.lfoRateHz = value (lfoRate);
    s.lfoSync = isOn (lfoSync);
    s.lfoSyncIndex = index (lfoSyncRate);
    for (int k = 0; k < 16; ++k)
        s.steps[size_t (k)] = value (steps[k]);
    s.lastStep = index (seqLast);
    s.seqRate = value (seqRate);
    s.seqSync = isOn (seqSync);
    s.seqSyncIndex = index (seqSyncRate);
    s.swing = value (swing);
    s.delayMs = value (dlyTime);
    s.delaySync = isOn (dlySync);
    s.delaySyncIndex = index (dlySyncRate);
    s.delayRatioIndex = index (dlyRatio);
    s.delayFeedback = value (dlyFeedback);
    s.delayMix = value (dlyMix);
    s.delayAnalog = isOn (dlyMode);
    s.delayRouteFeedback = isOn (dlyRoute);
    s.followSidechain = isOn (folSource);
    s.keyboardTracking = isOn (kbd);
    for (int src = 0; src < 3; ++src)
        for (int d = 0; d < 3; ++d)
            s.mod[src][d] = value (mods[src][d]);
    s.bypass = isOn (bypassParam);
    return s;
}

void MotionFilterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.setSettings (readSettings());
    dsp.prepare (sampleRate);
    scratchF.assign (size_t (std::max (samplesPerBlock, 1)), 0.0f);
    scratchD.assign (size_t (std::max (samplesPerBlock, 1)), 0.0);
    setLatencySamples (dsp.getLatencySamples());
}

template <typename Sample>
void MotionFilterProcessor::processAudio (juce::AudioBuffer<Sample>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0)
        return;

    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int inCh = mainIn.getNumChannels(), outCh = mainOut.getNumChannels();
    if (outCh == 0)
        return;

    auto& scratch = [this]() -> std::vector<Sample>& {
        if constexpr (std::is_same_v<Sample, float>) return scratchF;
        else return scratchD;
    }();
    if (scratch.size() < size_t (n))
        scratch.resize (size_t (n)); // only if the host exceeds its announced block size

    Sample* left = mainOut.getWritePointer (0);
    Sample* right = outCh > 1 ? mainOut.getWritePointer (1) : scratch.data();
    if (inCh < 2 || outCh < 2)
        std::copy (left, left + n, right); // mono input feeds both channels

    // sidechain (optional bus)
    const Sample* scL = nullptr;
    const Sample* scR = nullptr;
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 1);
            if (sc.getNumChannels() > 0)
            {
                scL = sc.getReadPointer (0);
                scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL;
            }
        }

    // host transport
    mf::Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
            {
                t.valid = true;
                t.bpm = *bpm;
            }
            t.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition())
                t.ppq = *ppq;
            else
                t.playing = false;
        }

    dsp.setSettings (readSettings());

    // MIDI notes for keyboard tracking, sample accurate: process up to each event
    int done = 0;
    auto run = [&] (int upTo) {
        if (upTo <= done)
            return;
        mf::Transport tt = t;
        tt.ppq += (t.bpm / 60.0) * done / getSampleRate();
        dsp.process (left + done, right + done, scL != nullptr ? scL + done : nullptr, scR != nullptr ? scR + done : nullptr,
                     upTo - done, tt);
        done = upTo;
    };
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int pos = juce::jlimit (0, n, meta.samplePosition);
        if (msg.isNoteOn())
        {
            run (pos);
            currentNote = msg.getNoteNumber();
            dsp.setNote (currentNote);
        }
    }
    run (n);

    const auto& tel = dsp.getTelemetry();
    for (int c = 0; c < 2; ++c)
    {
        live.cutoff[c].store (float (tel.cutoffHz[c]));
        live.combMs[c].store (float (tel.combDelayMs[c]));
        live.resonance[c].store (float (tel.resonance[c]));
    }
    live.lfo.store (float (tel.lfo));
    live.step.store (tel.step);
    live.follower.store (float (std::max (tel.follower[0], tel.follower[1])));

    for (int c = 2; c < outCh; ++c)
        mainOut.clear (c, 0, n);
}

void MotionFilterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) { processAudio (buffer, midi); }
void MotionFilterProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi) { processAudio (buffer, midi); }

void MotionFilterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MotionFilterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MotionFilterProcessor::createEditor()
{
    return new MotionFilterEditor (*this);
}

} // namespace craft::mfp

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::mfp::MotionFilterProcessor();
}
