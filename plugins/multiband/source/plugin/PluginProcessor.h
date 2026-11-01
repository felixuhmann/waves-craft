// Craft Multiband - AudioProcessor (stereo, or mono through the bus layout).
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/MultibandEngine.h"

namespace craft::multiband
{

// Levels collected on the audio thread and consumed by the editor.
struct MeterSnapshot
{
    std::array<float, 2> input {}, output {};
    std::array<float, numBands> bandLevel {}; // peak sine-equivalent band amplitude (linear)
    std::array<float, numBands> bandGainDb {}; // current band gain (dB)
    int channels = 2;
};

class MultibandProcessor : public juce::AudioProcessor
{
public:
    MultibandProcessor();
    ~MultibandProcessor() override = default;

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

    const juce::String getName() const override { return "Craft Multiband"; }
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
    Settings readSettings() const;

    // Thread-safe (GUI side): peaks since the previous call, current band gains.
    MeterSnapshot takeMeters() noexcept;
    double getCurrentSampleRate() const noexcept { return sampleRateForUi.load(); }

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer, bool forceBypass);

    juce::AudioProcessorValueTreeState state;
    MultibandEngine engine;

    juce::RangedAudioParameter *outputGain = nullptr, *q = nullptr, *knee = nullptr, *autoRelease = nullptr, *opto = nullptr;
    std::array<juce::RangedAudioParameter*, numCrossovers> xover {};
    struct BandParams
    {
        juce::RangedAudioParameter *gain = nullptr, *range = nullptr, *threshold = nullptr, *attack = nullptr,
                                   *release = nullptr, *solo = nullptr, *bypass = nullptr;
    };
    std::array<BandParams, numBands> bandParams {};
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<float> inPeak[2] {}, outPeak[2] {};
    std::array<std::atomic<float>, numBands> bandLevel {}, bandGain {};
    std::atomic<int> meterChannels { 2 };
    std::atomic<double> sampleRateForUi { 48000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandProcessor)
};

} // namespace craft::multiband
