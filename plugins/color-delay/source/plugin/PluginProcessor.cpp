#include "PluginProcessor.h"

#include "dsp/Parameters.h"
#include "ui/PluginEditor.h"

namespace craft
{

namespace
{
const char* const linkedIds[] = { ids::delayMsL, ids::delayMsR, ids::noteL, ids::noteR, ids::feedbackL, ids::feedbackR,
                                  ids::hp,       ids::lp,       ids::delayLink, ids::filterLink };
}

ColourDelayProcessor::ColourDelayProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftColourDelayState", createParameterLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (ids::bypass));
    for (auto* id : linkedIds)
        state.addParameterListener (id, this);
}

ColourDelayProcessor::~ColourDelayProcessor()
{
    cancelPendingUpdate();
    for (auto* id : linkedIds)
        state.removeParameterListener (id, this);
}

bool ColourDelayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // stereo, and mono-to-stereo (the mono input feeds both channels; spec section 1)
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

double ColourDelayProcessor::physical (const char* id) const
{
    if (auto* v = state.getRawParameterValue (id))
        return (double) v->load();
    return 0.0;
}

cdl::Settings ColourDelayProcessor::readSettings() const
{
    cdl::Settings s;
    s.inputDb = physical (ids::input);
    s.outputDb = physical (ids::output);
    s.hostSync = physical (ids::mode) >= 0.5;
    s.delayMs = { physical (ids::delayMsL), physical (ids::delayMsR) };
    s.noteIndex = { (int) std::lround (physical (ids::noteL)), (int) std::lround (physical (ids::noteR)) };
    s.feedbackPct = { physical (ids::feedbackL), physical (ids::feedbackR) };
    s.delayLink = physical (ids::delayLink) >= 0.5;
    s.dryWetPct = physical (ids::dryWet);
    s.hpHz = physical (ids::hp);
    s.lpHz = physical (ids::lp);
    s.verbOn = physical (ids::verbOn) >= 0.5;
    s.distOn = physical (ids::distOn) >= 0.5;
    s.doublerOn = physical (ids::doublerOn) >= 0.5;
    s.phaserOn = physical (ids::phaserOn) >= 0.5;
    s.verbSize = physical (ids::verbSize);
    s.verbLevel = physical (ids::verbLevel);
    s.distLevel = physical (ids::distLevel);
    s.distSend = physical (ids::distSend);
    s.distWidth = physical (ids::distWidth);
    s.doublerLevel = physical (ids::doublerLevel);
    s.doublerCents = physical (ids::doublerDetune);
    s.phaserLevel = physical (ids::phaserLevel);
    s.phaserRateHz = physical (ids::phaserRate);
    s.monitor = (cdl::Monitor) juce::jlimit (0, 2, (int) std::lround (physical (ids::monitor)));
    s.bypass = physical (ids::bypass) >= 0.5;
    return s;
}

void ColourDelayProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setSettings (readSettings());
    engine.setTempo (tempo.load());
    engine.prepare (sampleRate);
    setLatencySamples (0);
}

template <typename Sample>
void ColourDelayProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    // mono-to-stereo: the mono input feeds both channels
    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 0.0)
                    tempo.store (*bpm);

    const auto s = readSettings();
    engine.setTempo (tempo.load());
    engine.setSettings (s);

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);

    const auto gi = (Sample) std::pow (10.0, s.inputDb / 20.0);
    Sample pil = 0, pir = 0;
    for (int i = 0; i < n; ++i)
    {
        pil = std::max (pil, std::abs (l[i]));
        pir = std::max (pir, std::abs (r[i]));
    }

    engine.process (l, r, n);

    Sample pol = 0, por = 0;
    for (int i = 0; i < n; ++i)
    {
        pol = std::max (pol, std::abs (l[i]));
        por = std::max (por, std::abs (r[i]));
    }
    auto raise = [] (std::atomic<float>& a, float v) {
        float prev = a.load (std::memory_order_relaxed);
        while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
    };
    raise (inPeak[0], float (pil * gi));
    raise (inPeak[1], float (pir * gi));
    raise (outPeak[0], float (pol));
    raise (outPeak[1], float (por));
    gainReduction.store ((float) engine.getGainReductionDb());
}

void ColourDelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void ColourDelayProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

// ---------------------------------------------------------------------------------------------
// Delay Link (spec section 2): while linked the right channel follows the left (the engine already uses the left
// values for both channels); the right parameters mirror the left ones, so turning the link off keeps the right
// channel at the values it had while linked. Moving a right control while linked moves the left one too.
// HP/LP Link: f_HP * f_LP stays constant (captured when the link is engaged); the controls stop where they meet.
void ColourDelayProcessor::parameterChanged (const juce::String& id, float)
{
    if (applyingLink)
        return;
    int flag = none;
    if (id == ids::delayMsL || id == ids::noteL || id == ids::feedbackL) flag = delayFromL;
    else if (id == ids::delayMsR || id == ids::noteR || id == ids::feedbackR) flag = delayFromR;
    else if (id == ids::hp) flag = hpMoved;
    else if (id == ids::lp) flag = lpMoved;
    else if (id == ids::delayLink) flag = delayLinkOn;
    else if (id == ids::filterLink) flag = filterLinkOn;
    pending.fetch_or (flag);
    triggerAsyncUpdate();
}

void ColourDelayProcessor::setPhysical (const char* id, double value)
{
    if (auto* p = state.getParameter (id))
    {
        const float norm = p->convertTo0to1 ((float) value);
        if (std::abs (norm - p->getValue()) < 1.0e-7f)
            return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

void ColourDelayProcessor::handleAsyncUpdate()
{
    const int flags = pending.exchange (none);
    const juce::ScopedValueSetter<bool> guard (applyingLink, true);

    if (physical (ids::delayLink) >= 0.5)
    {
        const bool fromR = (flags & delayFromR) != 0 && (flags & (delayFromL | delayLinkOn)) == 0;
        const char* pairs[][2] = { { ids::delayMsL, ids::delayMsR }, { ids::noteL, ids::noteR }, { ids::feedbackL, ids::feedbackR } };
        for (auto& p : pairs)
        {
            const char* src = fromR ? p[1] : p[0];
            const char* dst = fromR ? p[0] : p[1];
            setPhysical (dst, physical (src));
        }
    }

    if (physical (ids::filterLink) >= 0.5)
    {
        const double hp = physical (ids::hp), lp = physical (ids::lp);
        if ((flags & filterLinkOn) != 0)
        {
            filterProduct = hp * lp;
        }
        else if ((flags & (hpMoved | lpMoved)) != 0)
        {
            const double meet = std::sqrt (filterProduct);
            namespace pm = cdl::param;
            if ((flags & hpMoved) != 0)
            {
                const double h = std::min (hp, meet);
                if (h != hp) setPhysical (ids::hp, pm::FilterHz::snap (h));
                setPhysical (ids::lp, pm::FilterHz::snap (std::max (filterProduct / h, meet)));
            }
            else
            {
                const double l = std::max (lp, meet);
                if (l != lp) setPhysical (ids::lp, pm::FilterHz::snap (l));
                setPhysical (ids::hp, pm::FilterHz::snap (std::min (filterProduct / l, meet)));
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------
void ColourDelayProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto copy = state.copyState();
    copy.setProperty ("filterProduct", filterProduct, nullptr);
    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, destData);
}

void ColourDelayProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            const juce::ScopedValueSetter<bool> guard (applyingLink, true);
            auto tree = juce::ValueTree::fromXml (*xml);
            filterProduct = (double) tree.getProperty ("filterProduct", physical (ids::hp) * physical (ids::lp));
            state.replaceState (tree);
        }
}

juce::AudioProcessorEditor* ColourDelayProcessor::createEditor()
{
    return new ColourDelayEditor (*this);
}

} // namespace craft

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::ColourDelayProcessor();
}
