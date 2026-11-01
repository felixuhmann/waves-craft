// Craft Enhancer - AudioProcessor wrapper around the host-independent engine.
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/EnhancerEngine.h"

namespace craft::enhancer
{

class EnhancerProcessor : public juce::AudioProcessor
{
public:
    EnhancerProcessor();
    ~EnhancerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Craft Enhancer"; }
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
    double getCurrentSampleRate() const noexcept { return currentRate.load(); }

    // Peak since the last call (GUI side): channel 0/1 of the input and the output.
    std::array<float, 2> takeInputPeaks() noexcept { return { inPeaks[0].exchange (0.0f), inPeaks[1].exchange (0.0f) }; }
    std::array<float, 2> takeOutputPeaks() noexcept { return { outPeaks[0].exchange (0.0f), outPeaks[1].exchange (0.0f) }; }

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    juce::AudioProcessorValueTreeState state;
    dsp::EnhancerEngine engine;
    std::atomic<double> currentRate { 48000.0 };
    std::atomic<float> inPeaks[2] {}, outPeaks[2] {};

    std::atomic<float>*input = nullptr, *output = nullptr, *direct = nullptr, *punch = nullptr;
    std::array<std::atomic<float>*, dsp::numBands> bandGain {}, width {}, solo {}, bypass {};
    std::array<std::atomic<float>*, dsp::numCrossovers> crossover {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnhancerProcessor)
};

} // namespace craft::enhancer
