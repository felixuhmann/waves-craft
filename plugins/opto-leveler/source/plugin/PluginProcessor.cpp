#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace craft::opto_leveler
{

namespace
{
void addTo (std::atomic<double>& a, double v) noexcept
{
    double prev = a.load (std::memory_order_relaxed);
    while (! a.compare_exchange_weak (prev, prev + v, std::memory_order_relaxed)) {}
}

double physical (const juce::RangedAudioParameter* p) { return double (p->convertFrom0to1 (p->getValue())); }
} // namespace

OptoLevelerProcessor::OptoLevelerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CraftOptoLevelerState", createParameterLayout())
{
    auto get = [this] (const char* id) { return state.getParameter (id); };
    gain = get (ids::gain);
    peakReduction = get (ids::peakReduction);
    mode = get (ids::mode);
    hiFreq = get (ids::hiFreq);
    analog = get (ids::analog);
    mix = get (ids::mix);
    trim = get (ids::trim);
    autoMakeup = get (ids::autoMakeup);

    for (auto* id : { ids::gain, ids::peakReduction, ids::autoMakeup })
        state.addParameterListener (id, this);
    captureMakeupOffset();

    engine.setSettings (readSettings());
    engine.prepare (48000.0);
}

OptoLevelerProcessor::~OptoLevelerProcessor()
{
    for (auto* id : { ids::gain, ids::peakReduction, ids::autoMakeup })
        state.removeParameterListener (id, this);
}

bool OptoLevelerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo (one linked detector) or mono; input and output layouts must match.
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

dsp::Settings OptoLevelerProcessor::readSettings() const
{
    dsp::Settings s;
    s.gain = physical (gain);
    s.peakReduction = physical (peakReduction);
    s.mode = juce::roundToInt (physical (mode)) == 1 ? dsp::Mode::Limit : dsp::Mode::Compress;
    s.hiFreq = physical (hiFreq);
    s.analog = dsp::AnalogNoise::Type (juce::jlimit (0, 2, juce::roundToInt (physical (analog))));
    s.mixPercent = physical (mix);
    s.trimDb = physical (trim);
    return s;
}

void OptoLevelerProcessor::prepareToPlay (double sampleRate, int)
{
    numChannels = juce::jlimit (1, 2, getMainBusNumOutputChannels());
    engine.setSettings (readSettings());
    engine.prepare (sampleRate); // also resets: parameters start at their values without a ramp
    setLatencySamples (0);
}

template <typename Sample>
void OptoLevelerProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() == 0)
        return;
    const int ch = juce::jlimit (1, 2, std::min (buffer.getNumChannels(), getMainBusNumOutputChannels()));

    engine.setSettings (readSettings()); // ramps (D4) run inside the engine
    engine.resetMeters();

    // process in sub-blocks so that the meter's gain-reduction average is sampled regularly
    constexpr int sub = 64;
    double grAcc = 0.0;
    int grCount = 0;
    for (int start = 0; start < n; start += sub)
    {
        const int len = std::min (sub, n - start);
        Sample* ptrs[2] = { buffer.getWritePointer (0, start), ch > 1 ? buffer.getWritePointer (1, start) : nullptr };
        engine.process (ptrs, ch, len);
        grAcc += -20.0 * std::log10 (std::max (engine.getCellGain(), 1.0e-6)) * len;
        grCount += len;
    }

    double inAvg = 0.0, outAvg = 0.0;
    for (int c = 0; c < ch; ++c)
    {
        inAvg = std::max (inAvg, engine.getInputAverage (c));
        outAvg = std::max (outAvg, engine.getOutputAverage (c));
    }
    addTo (inSum, inAvg * n);
    addTo (outSum, outAvg * n);
    addTo (grSum, grAcc);
    meterCount.fetch_add (grCount, std::memory_order_relaxed);

    for (int c = ch; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
}

void OptoLevelerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void OptoLevelerProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

OptoLevelerProcessor::MeterData OptoLevelerProcessor::takeMeters() noexcept
{
    MeterData m;
    const int count = meterCount.exchange (0);
    const double in = inSum.exchange (0.0), out = outSum.exchange (0.0), gr = grSum.exchange (0.0);
    if (count > 0)
    {
        m.inAverage = in / count;
        m.outAverage = out / count;
        m.grDb = gr / count;
        m.valid = true;
    }
    return m;
}

// ---------------------------------------------------------------------------------------------
// Auto Makeup (spec 9, D2): a UI coupling. While it is on, moving Peak Reduction moves Gain so that the make-up
// follows dsp::autoMakeupDb; moving Gain re-establishes the offset. The audio make-up is always T2 at the displayed
// Gain (no hidden offset). Only changes made on the message thread (editor, host GUI) are coupled; host automation
// of Peak Reduction plays back the Gain automation that was recorded with it.
void OptoLevelerProcessor::captureMakeupOffset()
{
    makeupOffsetDb = dsp::gainLawDb (physical (gain)) - dsp::autoMakeupDb (physical (peakReduction));
}

void OptoLevelerProcessor::parameterChanged (const juce::String& id, float)
{
    auto* mm = juce::MessageManager::getInstanceWithoutCreating();
    if (mm == nullptr || ! mm->isThisTheMessageThread() || applyingMakeup)
        return;
    if (id == ids::autoMakeup || id == ids::gain)
    {
        captureMakeupOffset();
        return;
    }
    if (id == ids::peakReduction && autoMakeup->getValue() >= 0.5f)
    {
        const double targetDb = makeupOffsetDb + dsp::autoMakeupDb (physical (peakReduction));
        const double display = juce::jlimit (0.0, 100.0, dsp::gainLawInverse (targetDb));
        const juce::ScopedValueSetter<bool> guard (applyingMakeup, true);
        gain->setValueNotifyingHost (gain->convertTo0to1 (float (display)));
    }
}

// ---------------------------------------------------------------------------------------------
void OptoLevelerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void OptoLevelerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            const juce::ScopedValueSetter<bool> guard (applyingMakeup, true);
            state.replaceState (juce::ValueTree::fromXml (*xml));
        }
    captureMakeupOffset();
}

juce::AudioProcessorEditor* OptoLevelerProcessor::createEditor() { return new OptoLevelerEditor (*this); }

} // namespace craft::opto_leveler

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new craft::opto_leveler::OptoLevelerProcessor(); }
