// Craft Opto Leveler - AudioProcessor (spec r1: stereo with one linked detector, mono via bus layout).
#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/LevelerEngine.h"

namespace craft::opto_leveler
{

class OptoLevelerProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    OptoLevelerProcessor();
    ~OptoLevelerProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Craft Opto Leveler"; }
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

    // Parameters / GUI support
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    dsp::Settings readSettings() const;
    int getNumMeterChannels() const noexcept { return numChannels.load (std::memory_order_relaxed); }

    struct MeterData
    {
        double inAverage = 0.0;  // rectified average of the input (max of the channels)
        double outAverage = 0.0; // rectified average of the output
        double grDb = 0.0;       // mean gain reduction of the cell
        bool valid = false;
    };
    MeterData takeMeters() noexcept; // GUI thread

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void captureMakeupOffset();

    juce::AudioProcessorValueTreeState state;
    dsp::LevelerEngine engine;

    juce::RangedAudioParameter *gain = nullptr, *peakReduction = nullptr, *mode = nullptr, *hiFreq = nullptr,
                               *analog = nullptr, *mix = nullptr, *trim = nullptr, *autoMakeup = nullptr;

    std::atomic<int> numChannels { 2 };
    std::atomic<double> inSum { 0.0 }, outSum { 0.0 }, grSum { 0.0 };
    std::atomic<int> meterCount { 0 };

    // Auto Makeup (message thread only)
    double makeupOffsetDb = 0.0;
    bool applyingMakeup = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoLevelerProcessor)
};

} // namespace craft::opto_leveler
