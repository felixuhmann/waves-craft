// Craft Audio doubler - shared AudioProcessor, parameterised by the number of voices (4 or 2).
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/DoublerEngine.h"

namespace craft::doubler
{

class DoublerProcessor : public juce::AudioProcessor
{
public:
    explicit DoublerProcessor (int numVoices);
    ~DoublerProcessor() override = default;

    int getNumVoices() const noexcept { return numVoices; }

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
    double getTailLengthSeconds() const override { return 2.0; } // feedback echoes

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- GUI support (thread-safe) ---
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    Settings readSettings() const;
    // output peaks since the previous call
    std::array<float, 2> takePeaks() noexcept { return { peaks[0].exchange (0.0f), peaks[1].exchange (0.0f) }; }
    // current pitch offset of each voice in cents (detune + octave + LFO), for the delay/detune display
    double getVoiceCents (int voice) const noexcept { return voiceCents[size_t (voice)].load (std::memory_order_relaxed); }
    double getSampleRateForDisplay() const noexcept { return lastSampleRate.load(); }

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    const int numVoices;
    juce::AudioProcessorValueTreeState state;
    DoublerEngine engine;

    struct VoiceParams
    {
        juce::RangedAudioParameter *on = nullptr, *gain = nullptr, *pan = nullptr, *delay = nullptr, *feedback = nullptr,
                                   *octave = nullptr, *detune = nullptr, *depth = nullptr, *rate = nullptr;
    };
    juce::RangedAudioParameter *master = nullptr, *fbHighPass = nullptr, *modReset = nullptr, *range = nullptr,
                               *align = nullptr, *directOn = nullptr, *directGain = nullptr, *directPan = nullptr,
                               *eqLowOn = nullptr, *eqLowGain = nullptr, *eqLowFreq = nullptr, *eqHighOn = nullptr,
                               *eqHighGain = nullptr, *eqHighFreq = nullptr, *routingA = nullptr, *routingB = nullptr;
    std::array<VoiceParams, maxVoices> voiceParams {};
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<float> peaks[2] {};
    std::array<std::atomic<double>, maxVoices> voiceCents {};
    std::atomic<double> lastSampleRate { 48000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoublerProcessor)
};

} // namespace craft::doubler
