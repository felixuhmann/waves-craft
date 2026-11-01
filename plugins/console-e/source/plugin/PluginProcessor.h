// Craft Console E - AudioProcessor (stereo with one linked detector, or mono, chosen by the bus layout).
#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/ConsoleEngine.h"

namespace craft::consolee
{

class ConsoleProcessor : public juce::AudioProcessor
{
public:
    ConsoleProcessor();
    ~ConsoleProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Craft Console E"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    int getNumAudioChannels() const noexcept { return engine.getNumChannels(); }

    // Display values since the previous call (GUI thread).
    struct MeterValues
    {
        float level[2] {};          // selected meter (input or output), linear peak per channel
        float compReductionDb = 0;  // <= 0
        float expanderGainDb = 0;   // <= 0
        bool showsOutput = false;
    };
    MeterValues takeMeters() noexcept;

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);
    dsp::Settings readSettings() const noexcept;

    float value (const juce::RangedAudioParameter* p) const noexcept { return p != nullptr ? p->getValue() : 0.0f; }
    bool on (const juce::RangedAudioParameter* p) const noexcept { return value (p) >= 0.5f; }

    juce::AudioProcessorValueTreeState state;
    dsp::ConsoleEngine engine { 2 };

    struct Params
    {
        juce::RangedAudioParameter *inputTrim, *outputFader, *polarity, *analogue, *meterOutput, *hp, *lp, *split, *lfGain, *lfFreq,
            *lfBell, *lmfGain, *lmfFreq, *lmfQ, *hmfGain, *hmfFreq, *hmfQ, *hfGain, *hfFreq, *hfBell, *eqBypass, *eqToSidechain,
            *compRatio, *compThreshold, *compRelease, *compFast, *expRange, *expThreshold, *expRelease, *expFast, *gate, *dynBypass,
            *channelOut;
    } params {};
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<float> meterIn[2] {}, meterOut[2] {}, meterComp { 0.0f }, meterExp { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConsoleProcessor)
};

} // namespace craft::consolee
