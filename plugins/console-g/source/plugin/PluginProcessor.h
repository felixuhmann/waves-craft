// Craft Console G - AudioProcessor (stereo with linked dynamics, or mono, chosen by the host's bus layout).
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/ConsoleEngine.h"

namespace craft::consoleg
{

struct MeterSnapshot
{
    std::array<float, 2> input {}, output {}; // linear peaks
    float gainReductionDb = 0.0f;             // <= 0
    int channels = 2;
};

class ConsoleGProcessor : public juce::AudioProcessor
{
public:
    ConsoleGProcessor();
    ~ConsoleGProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Craft Console G"; }
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
    MeterSnapshot takeMeters() noexcept;

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);
    dsp::Settings readSettingsFast() const noexcept;

    juce::AudioProcessorValueTreeState state;
    dsp::ConsoleEngine engine;

    struct Cache
    {
        juce::RangedAudioParameter *compThreshold, *compRatio, *compRelease, *compFast, *expThreshold, *expRange,
            *expRelease, *expGate, *expFast, *dynBypass, *dynPostEq, *filtersSidechain, *filtersSplit, *eqBypass, *hpFreq,
            *lpFreq, *lfGain, *lfFreq, *lmfGain, *lmfFreq, *lmfQ, *lmfDiv3, *hmfGain, *hmfFreq, *hmfQ, *hmfX3, *hfGain,
            *hfFreq, *inputTrim, *outputGain, *analog, *polarity;
    } p {};

    std::atomic<float> inPeak[2] {}, outPeak[2] {}, minGr { 0.0f };
    std::atomic<int> meterChannels { 2 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConsoleGProcessor)
};

} // namespace craft::consoleg
