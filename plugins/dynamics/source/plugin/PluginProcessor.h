// Craft Dynamics - AudioProcessor shared by the four products (spec r1 section 1).
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/DynamicsEngine.h"

namespace craft::dynamics
{

class DynamicsProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::AsyncUpdater
{
public:
    explicit DynamicsProcessor (dsp::Variant variant);
    ~DynamicsProcessor() override;

    dsp::Variant getVariant() const noexcept { return variant; }
    dsp::VariantInfo getInfo() const noexcept { return dsp::variantInfo (variant); }

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

    // Current physical settings (any thread; read from the parameters).
    dsp::Settings readSettings() const;

    // Live values for the editor (written by the audio thread).
    struct MeterView
    {
        float compLevelDb = -200.0f, compGainDb = 0.0f, gateLevelDb = -200.0f, gateGainDb = 0.0f;
        bool gateOpen = true;
        float inPeak[2] {}, outPeak[2] {};
    };
    MeterView takeMeters() noexcept; // peaks are "since the last call"
    int getNumChannelsInUse() const noexcept { return numChannels.load(); }
    double getCurrentSampleRate() const noexcept { return sampleRateUsed.load(); }

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void captureLinkValues();

    float norm (const char* id) const noexcept;
    bool isOn (const char* id) const noexcept { return norm (id) >= 0.5f; }
    int choice (const char* id) const noexcept;
    double physical (const char* id, double fallback) const noexcept;

    const dsp::Variant variant;
    juce::AudioProcessorValueTreeState state;
    dsp::DynamicsEngine engine;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<float> compLevel { -200.0f }, compGain { 0.0f }, gateLevel { -200.0f }, gateGain { 0.0f };
    std::atomic<bool> gateOpenFlag { true };
    std::atomic<float> inPeaks[2] {}, outPeaks[2] {};
    std::atomic<int> numChannels { 2 };
    std::atomic<double> sampleRateUsed { 48000.0 };

    // link offsets (message thread)
    struct LinkPair
    {
        const char* link;
        const char* a;
        const char* b;
        float lastA = 0.0f, lastB = 0.0f;
    };
    std::array<LinkPair, 3> links { { { ids::linkThreshold, ids::compThreshold, ids::gateOpen },
                                       { ids::linkAttack, ids::compAttack, ids::gateAttack },
                                       { ids::linkRelease, ids::compRelease, ids::gateRelease } } };
    std::atomic<bool> applyingLink { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicsProcessor)
};

} // namespace craft::dynamics
