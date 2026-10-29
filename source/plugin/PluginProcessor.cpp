#include "PluginProcessor.h"

#include "dsp/Parameters.h"
#include "ui/PluginEditor.h"

#ifndef CRAFT_VARIANT
 #define CRAFT_VARIANT 0
#endif

namespace craft
{

CraftProcessor::CraftProcessor (dsp::Variant v)
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      variant (v),
      state (*this, nullptr, "CraftImagerState", createParameterLayout (v)),
      engine (v)
{
    auto get = [this] (const char* id) { return state.getParameter (id); };
    gain = get (ids::gain);
    width = get (ids::width);
    rotation = get (ids::rotation);
    asymmetry = get (ids::asymmetry);
    shuffle = get (ids::shuffle);
    frequency = get (ids::frequency);
    bassTrim = get (ids::bassTrim);
    swap = get (ids::swap);
    polarity1 = get (ids::polarity1);
    polarity2 = get (ids::polarity2);
    inputMode = get (ids::inputMode);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (get (ids::bypass));

    state.state.setProperty (ids::meterMode, 0, nullptr);
}

const juce::String CraftProcessor::getName() const
{
    switch (variant)
    {
        case dsp::Variant::Full:     return "Craft Shuffler";
        case dsp::Variant::Imager:   return "Craft Imager";
        case dsp::Variant::MsMatrix: return "Craft MS Matrix";
    }
    return "Craft";
}

bool CraftProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

dsp::Settings CraftProcessor::readSettings() const
{
    namespace pm = dsp::param;
    dsp::Settings s;
    if (gain != nullptr)      s.gainDb = pm::Gain::fromNorm (normalised (gain));
    if (width != nullptr)     s.width = pm::Width::fromNorm (normalised (width));
    if (rotation != nullptr)  s.rotationDeg = pm::Rotation::fromNorm (normalised (rotation));
    if (asymmetry != nullptr) s.asymmetryDeg = pm::Asymmetry::fromNorm (normalised (asymmetry));
    if (shuffle != nullptr)   s.shuffle = pm::Shuffle::fromNorm (normalised (shuffle));
    if (frequency != nullptr) s.frequencyHz = pm::Frequency::fromNorm (normalised (frequency));
    if (bassTrim != nullptr)  s.bassTrimDb = pm::BassTrim::fromNorm (normalised (bassTrim));
    s.swap = isOn (swap);
    s.polarity1 = isOn (polarity1);
    s.polarity2 = isOn (polarity2);
    s.msInput = isOn (inputMode);
    return s;
}

void CraftProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setSettings (readSettings());
    engine.setBypassed (isOn (bypassParam));
    engine.reset();
    setLatencySamples (dsp::ImagerEngine::getLatencySamples());
}

template <typename Sample>
void CraftProcessor::processAudio (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    engine.setSettings (readSettings());
    engine.setBypassed (isOn (bypassParam));

    auto* c1 = buffer.getWritePointer (0);
    auto* c2 = buffer.getWritePointer (1);
    engine.process (c1, c2, n);

    // output metering (display only): L/R and M/S = (L+R)/2, (L-R)/2
    Sample pl = 0, pr = 0, pm = 0, ps = 0;
    for (int i = 0; i < n; ++i)
    {
        const Sample l = c1[i], r = c2[i];
        pl = std::max (pl, std::abs (l));
        pr = std::max (pr, std::abs (r));
        pm = std::max (pm, std::abs ((l + r) * Sample (0.5)));
        ps = std::max (ps, std::abs ((l - r) * Sample (0.5)));
    }
    auto raise = [] (std::atomic<float>& a, Sample v) {
        float prev = a.load (std::memory_order_relaxed);
        while (float (v) > prev && ! a.compare_exchange_weak (prev, float (v), std::memory_order_relaxed)) {}
    };
    raise (peaks[0][0], pl);
    raise (peaks[0][1], pr);
    raise (peaks[1][0], pm);
    raise (peaks[1][1], ps);
}

void CraftProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { processAudio (buffer); }
void CraftProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { processAudio (buffer); }

std::array<float, 2> CraftProcessor::takePeaks (int meterMode) noexcept
{
    const int m = meterMode != 0 ? 1 : 0;
    return { peaks[m][0].exchange (0.0f), peaks[m][1].exchange (0.0f) };
}

void CraftProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CraftProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CraftProcessor::createEditor()
{
    return new CraftEditor (*this);
}

} // namespace craft

// Plug-in entry point: the variant is selected at compile time per target.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new craft::CraftProcessor (static_cast<craft::dsp::Variant> (CRAFT_VARIANT));
}
