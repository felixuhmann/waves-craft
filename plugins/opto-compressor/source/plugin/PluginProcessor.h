// Craft Opto Compressor - AudioProcessor (spec r1).
#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/OptoEngine.h"

namespace craft::opto
{

namespace ids
{
inline constexpr const char* gain = "gain";
inline constexpr const char* peakReduction = "peakReduction";
inline constexpr const char* mode = "mode";
inline constexpr const char* hiFreq = "hiFreq";
inline constexpr const char* analog = "analog";
inline constexpr const char* mix = "mix";
inline constexpr const char* trim = "trim";
inline constexpr const char* autoMakeup = "autoMakeup";
inline constexpr const char* vuDisplay = "vuDisplay";
inline constexpr const char* bypass = "bypass";
} // namespace ids

class OptoProcessor : public juce::AudioProcessor
{
public:
    OptoProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) override { processBlock (b, m); }
    void processBlockBypassed (juce::AudioBuffer<double>& b, juce::MidiBuffer& m) override { processBlock (b, m); }
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Craft Opto Compressor"; }
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

    struct MeterView { float inPeak = 0, outPeak = 0, reductionDb = 0; };
    MeterView takeMeters() noexcept; // peaks are "since the last call"

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);
    dsp::Settings readSettings() const noexcept;
    double physical (const char* id) const noexcept;

    juce::AudioProcessorValueTreeState state;
    juce::RangedAudioParameter* bypassParam = nullptr;
    dsp::OptoEngine engine;
    std::atomic<float> inPeak { 0 }, outPeak { 0 }, reduction { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoProcessor)
};

} // namespace craft::opto
