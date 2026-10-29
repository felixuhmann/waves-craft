// Craft Audio stereo imager - shared AudioProcessor, parameterised by variant.
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/ImagerEngine.h"
#include "ParameterLayout.h"

namespace craft
{

class CraftProcessor : public juce::AudioProcessor
{
public:
    explicit CraftProcessor (dsp::Variant variant);
    ~CraftProcessor() override = default;

    dsp::Variant getVariant() const noexcept { return variant; }

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

    // Parameters / GUI support
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    // Current (quantised) settings as set by the parameters - used by the vector display.
    dsp::Settings readSettings() const;

    // Peak since last call, per meter channel; mode 0 = L/R, 1 = M/S. Thread-safe (GUI side).
    std::array<float, 2> takePeaks (int meterMode) noexcept;

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    float normalised (juce::RangedAudioParameter* p) const noexcept { return p != nullptr ? p->getValue() : 0.0f; }
    bool isOn (juce::RangedAudioParameter* p) const noexcept { return p != nullptr && p->getValue() >= 0.5f; }

    const dsp::Variant variant;
    juce::AudioProcessorValueTreeState state;
    dsp::ImagerEngine engine;

    juce::RangedAudioParameter *gain = nullptr, *width = nullptr, *rotation = nullptr, *asymmetry = nullptr,
                               *shuffle = nullptr, *frequency = nullptr, *bassTrim = nullptr, *swap = nullptr,
                               *polarity1 = nullptr, *polarity2 = nullptr, *inputMode = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<float> peaks[2][2] {}; // [mode][channel]

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CraftProcessor)
};

} // namespace craft
