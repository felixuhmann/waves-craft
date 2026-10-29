// Craft Punch Compressor - AudioProcessor (stereo, or mono via the host's bus layout).
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/PunchEngine.h"

namespace craft::punch
{

// Peaks collected on the audio thread and consumed by the editor.
struct MeterSnapshot
{
    float input = 0.0f, output = 0.0f; // linked sample peaks (linear)
    float minGain = 1.0f;              // smallest gain-reduction gain (linear, without makeup)
};

class PunchCompressorProcessor : public juce::AudioProcessor
{
public:
    PunchCompressorProcessor();
    ~PunchCompressorProcessor() override = default;

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

    const juce::String getName() const override { return "Craft Punch Compressor"; }
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

    // Thread-safe (GUI side)
    MeterSnapshot takeMeters() noexcept;
    double getHostBpm() const noexcept { return hostBpm.load(); }
    bool hasHostTempo() const noexcept { return hostTempoKnown.load(); }

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer, bool forceBypass);
    void updateHostTempo();

    juce::AudioProcessorValueTreeState state;
    PunchEngine engine;

    juce::RangedAudioParameter *threshold = nullptr, *ratio = nullptr, *mix = nullptr, *attack = nullptr, *punch = nullptr,
                               *release = nullptr, *sync = nullptr, *note = nullptr, *analog = nullptr, *output = nullptr,
                               *limiter = nullptr, *tempo = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostTempoKnown { false };
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, minGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchCompressorProcessor)
};

} // namespace craft::punch
