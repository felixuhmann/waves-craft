// Craft De-Esser - AudioProcessor (stereo or mono, chosen per CMake target).
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/DeEsserEngine.h"

namespace craft::deesser
{

class DeEsserProcessor : public juce::AudioProcessor
{
public:
    explicit DeEsserProcessor (int numChannels);
    ~DeEsserProcessor() override = default;

    int getNumAudioChannels() const noexcept { return channels; }

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

    const juce::String getName() const override;
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

    // Display values since the previous call (GUI thread).
    struct MeterValues
    {
        float sidechainPeak = 0.0f;  // detector envelope, linear
        float minGain = 1.0f;        // smallest applied gain, linear
        std::array<float, 2> output {}; // output peaks, linear
        bool monitoringSidechain = false;
    };
    MeterValues takeMeters() noexcept;

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    float normalised (juce::RangedAudioParameter* p) const noexcept { return p != nullptr ? p->getValue() : 0.0f; }

    const int channels;
    juce::AudioProcessorValueTreeState state;
    dsp::DeEsserEngine engine;

    juce::RangedAudioParameter *threshold = nullptr, *frequency = nullptr, *sidechain = nullptr, *audio = nullptr,
                               *monitor = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<float> meterSidechain { 0.0f }, meterMinGain { 1.0f };
    std::atomic<float> meterOut[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeEsserProcessor)
};

} // namespace craft::deesser
