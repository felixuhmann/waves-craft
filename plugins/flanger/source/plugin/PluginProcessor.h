// Craft Flanger - AudioProcessor shared by the three variants (spec r1 section 1).
#pragma once

#include <array>
#include <atomic>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/FlangerEngine.h"
#include "dsp/Link.h"

namespace craft::flanger
{

class FlangerProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener,
                         private juce::AsyncUpdater
{
public:
    explicit FlangerProcessor (dsp::Variant variant);
    ~FlangerProcessor() override;

    dsp::Variant getVariant() const noexcept { return variant; }
    int getNumMeterChannels() const noexcept { return dsp::numOutputChannels (variant); }

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
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parameters / GUI support
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    dsp::Settings readSettings() const;
    bool isTempoSync() const noexcept { return isOn (tempoSync); }

    // Peak since the last call per output channel (GUI side, thread-safe).
    std::array<float, 2> takePeaks() noexcept;

    // LFO position for the modulation indicator: phase in cycles per channel, and the LFO value in [-1, 1].
    struct LfoView
    {
        float phase[2] {};
        float value[2] {};
        float delayMs[2] {};
    };
    LfoView getLfoView() const noexcept;

private:
    static BusesProperties busesFor (dsp::Variant v);

    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void setPhysical (juce::RangedAudioParameter* p, double value);

    float normalised (juce::RangedAudioParameter* p) const noexcept { return p != nullptr ? p->getValue() : 0.0f; }
    bool isOn (juce::RangedAudioParameter* p) const noexcept { return p != nullptr && p->getValue() >= 0.5f; }

    const dsp::Variant variant;
    std::shared_ptr<std::atomic<bool>> tempoSyncShown = std::make_shared<std::atomic<bool>> (false);
    juce::AudioProcessorValueTreeState state;
    dsp::FlangerEngine engine;

    juce::RangedAudioParameter *delay = nullptr, *depth = nullptr, *rate = nullptr, *mix = nullptr, *feedback = nullptr,
                               *gain = nullptr, *stereo = nullptr, *filterFreq = nullptr, *filterType = nullptr,
                               *filterOn = nullptr, *waveform = nullptr, *wetInvert = nullptr, *feedbackInvert = nullptr,
                               *tape = nullptr, *stop = nullptr, *link = nullptr, *tempoSync = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    // transport / tempo (audio thread)
    bool wasPlaying = false;
    double lastBpm = 120.0;

    // Rate <-> Depth link (message thread)
    dsp::RateDepthLink linkState;
    std::atomic<bool> pendingRate { false }, pendingDepth { false }, pendingCapture { true }, applyingLink { false };

    std::atomic<float> peaks[2] {};
    std::atomic<float> viewPhase[2] {}, viewValue[2] {}, viewDelay[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangerProcessor)
};

} // namespace craft::flanger
