// Craft Mastering Limiter - AudioProcessor (spec r1 section 1: stereo linked, mono via bus layout).
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/MasteringEngine.h"

namespace craft::mastering_limiter
{

class MasteringLimiterProcessor : public juce::AudioProcessor
{
public:
    MasteringLimiterProcessor();
    ~MasteringLimiterProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Craft Mastering Limiter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

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
        float input[2] {};   // linear sample peaks since the last call
        float output[2] {};
        float minGain = 1.0f; // limiter gain (without make-up), minimum since the last call
    };
    MeterData takeMeters() noexcept; // GUI thread

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    juce::AudioProcessorValueTreeState state;
    dsp::MasteringEngine engine;

    juce::RangedAudioParameter *threshold = nullptr, *ceiling = nullptr, *release = nullptr, *adaptive = nullptr,
                               *wordLength = nullptr, *dither = nullptr, *shaping = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<int> numChannels { 2 };
    std::atomic<float> inPeak[2] {}, outPeak[2] {}, minGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasteringLimiterProcessor)
};

} // namespace craft::mastering_limiter
