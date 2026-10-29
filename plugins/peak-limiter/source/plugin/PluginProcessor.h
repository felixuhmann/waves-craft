// Craft Peak Limiter - AudioProcessor (stereo or mono, chosen by the host's bus layout).
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/LimiterEngine.h"

namespace craft::limiter
{

// Peaks collected on the audio thread and consumed by the editor.
struct MeterSnapshot
{
    std::array<float, 2> input {}, output {};
    float minGain = 1.0f; // smallest applied gain (linear, without makeup)
    int channels = 2;
};

class PeakLimiterProcessor : public juce::AudioProcessor
{
public:
    PeakLimiterProcessor();
    ~PeakLimiterProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Craft Peak Limiter"; }
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

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    Settings readSettings() const;

    // Thread-safe (GUI side): peaks since the previous call.
    MeterSnapshot takeMeters() noexcept;
    int getMeterChannels() const noexcept { return meterChannels.load(); }

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer, bool forceBypass);

    juce::AudioProcessorValueTreeState state;
    LimiterEngine engine;

    juce::RangedAudioParameter *threshold = nullptr, *ceiling = nullptr, *release = nullptr, *autoRelease = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<float> inPeak[2] {}, outPeak[2] {};
    std::atomic<float> minGain { 1.0f };
    std::atomic<int> meterChannels { 2 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PeakLimiterProcessor)
};

} // namespace craft::limiter
