// Craft Hybrid Delay - AudioProcessor (stereo and mono-to-stereo bus layouts).
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/HybridDelayEngine.h"

namespace craft::hdelay
{

class HDelayProcessor : public juce::AudioProcessor
{
public:
    HDelayProcessor();
    ~HDelayProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Craft Hybrid Delay"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- GUI support
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    Settings readSettings() const;                 // current parameter values (host tempo included)
    double getHostBpm() const noexcept { return hostBpm.load(); }
    bool isMonoInput() const noexcept { return monoInput.load(); }
    // Peak since last call per output channel. Thread-safe (GUI side).
    std::array<float, 2> takePeaks() noexcept { return { peaks[0].exchange (0.0f), peaks[1].exchange (0.0f) }; }

    // Sets a parameter from the GUI as one host gesture (physical value).
    void setParameterValue (const char* id, float physicalValue);

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    juce::AudioProcessorValueTreeState state;

    // every parameter read by readSettings(), resolved once in the constructor (pointer comparison in indexOf)
    static constexpr std::array<const char*, 20> paramIds {
        ids::delayMs, ids::delayNote, ids::delayRMs, ids::delayRNote, ids::dual, ids::sync, ids::bpm,
        ids::feedback, ids::pingPong, ids::pingPongStart, ids::phaseL, ids::phaseR, ids::mix, ids::output,
        ids::analog, ids::lofi, ids::modRate, ids::modDepth, ids::hiPass, ids::loPass
    };
    std::array<std::atomic<float>*, 20> rawValues {};
    std::array<juce::RangedAudioParameter*, 20> params {};
    static size_t indexOf (const char* id) noexcept;
    HybridDelayEngine engine;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> monoInput { false };
    std::atomic<float> peaks[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HDelayProcessor)
};

} // namespace craft::hdelay
