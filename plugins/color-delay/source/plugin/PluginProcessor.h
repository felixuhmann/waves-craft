// Craft Colour Delay - AudioProcessor (stereo, and mono-in / stereo-out).
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/ColourDelayEngine.h"

namespace craft
{

class ColourDelayProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::AsyncUpdater
{
public:
    ColourDelayProcessor();
    ~ColourDelayProcessor() override;

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

    const juce::String getName() const override { return "Craft Colour Delay"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    cdl::Settings readSettings() const;

    // GUI metering: peak since the previous call (input after the Input gain, output), per channel
    std::array<float, 2> takeInputPeaks() noexcept { return { inPeak[0].exchange (0.0f), inPeak[1].exchange (0.0f) }; }
    std::array<float, 2> takeOutputPeaks() noexcept { return { outPeak[0].exchange (0.0f), outPeak[1].exchange (0.0f) }; }
    float getGainReductionDb() const noexcept { return gainReduction.load(); }
    double getTempo() const noexcept { return tempo.load(); }

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void setPhysical (const char* id, double value);
    double physical (const char* id) const;

    juce::AudioProcessorValueTreeState state;
    cdl::ColourDelayEngine engine;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<float> inPeak[2] {}, outPeak[2] {}, gainReduction { 0.0f };
    std::atomic<double> tempo { 120.0 };

    // Link handling (message thread): which side of each linked pair moved last
    enum Pending : int { none = 0, delayFromL = 1, delayFromR = 2, hpMoved = 4, lpMoved = 8, delayLinkOn = 16, filterLinkOn = 32 };
    std::atomic<int> pending { none };
    bool applyingLink = false;
    double filterProduct = 16.0 * 21357.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColourDelayProcessor)
};

} // namespace craft
