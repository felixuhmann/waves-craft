// Craft FET Limiter - AudioProcessor (spec r1 section 1: stereo linked, mono via bus layout).
#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/FetEngine.h"

namespace craft::fet_limiter
{

class FetLimiterProcessor : public juce::AudioProcessor
{
public:
    FetLimiterProcessor();
    ~FetLimiterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Craft FET Limiter"; }
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
    dsp::Settings readSettings() const;
    int getNumMeterChannels() const noexcept { return numChannels.load (std::memory_order_relaxed); }

    struct MeterData
    {
        float inputPeak = 0.0f;  // linear sample peak since the last call (both channels)
        float outputPeak = 0.0f;
        float grDb = 0.0f;       // largest gain reduction since the last call (dB, >= 0)
    };
    MeterData takeMeters() noexcept; // GUI thread

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    juce::AudioProcessorValueTreeState state;
    dsp::FetEngine engine;

    juce::RangedAudioParameter *input = nullptr, *output = nullptr, *attack = nullptr, *release = nullptr,
                               *ratio = nullptr, *analog = nullptr, *revision = nullptr, *compOff = nullptr,
                               *mix = nullptr, *trim = nullptr;

    std::atomic<int> numChannels { 2 };
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, maxGr { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FetLimiterProcessor)
};

} // namespace craft::fet_limiter
