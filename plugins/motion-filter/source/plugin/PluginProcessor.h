// Craft Motion Filter - JUCE AudioProcessor around the host-independent DSP core.
#pragma once

#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/MotionFilter.h"

namespace craft::mfp
{

class MotionFilterProcessor : public juce::AudioProcessor
{
public:
    MotionFilterProcessor();
    ~MotionFilterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Craft Motion Filter"; }
    bool acceptsMidi() const override { return true; }
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

    // The physical settings the parameters currently describe (also used by the editor's display).
    mf::Settings readSettings() const;

    // Live values for the editor (written by the audio thread after each block).
    struct Live
    {
        std::atomic<float> cutoff[2] { { 18000.0f }, { 18000.0f } };
        std::atomic<float> combMs[2] { { 28.0f }, { 28.0f } };
        std::atomic<float> resonance[2] { { 0.0f }, { 0.0f } };
        std::atomic<float> lfo { 0.0f };
        std::atomic<int> step { 0 };
        std::atomic<float> follower { 0.0f };
        std::atomic<float> peakIn { 0.0f }, peakOut { 0.0f };
    };
    Live live;

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer, juce::MidiBuffer& midi);

    float value (juce::RangedAudioParameter* p) const noexcept
    {
        return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
    }
    bool isOn (juce::RangedAudioParameter* p) const noexcept { return p != nullptr && p->getValue() >= 0.5f; }
    int index (juce::RangedAudioParameter* p) const noexcept { return p != nullptr ? juce::roundToInt (value (p)) : 0; }

    juce::AudioProcessorValueTreeState state;
    mf::MotionFilter dsp;
    int currentNote = -1;
    std::vector<float> scratchF;
    std::vector<double> scratchD;

    juce::RangedAudioParameter *type {}, *slope {}, *freq {}, *res {}, *spread {}, *smooth {}, *drive {}, *crush {},
        *input {}, *output {}, *mix {}, *lfoShape {}, *lfoRate {}, *lfoSync {}, *lfoSyncRate {}, *seqLast {}, *seqRate {},
        *seqSync {}, *seqSyncRate {}, *swing {}, *dlyTime {}, *dlySync {}, *dlySyncRate {}, *dlyRatio {}, *dlyFeedback {},
        *dlyMix {}, *dlyMode {}, *dlyRoute {}, *folSource {}, *kbd {};
    juce::RangedAudioParameter* steps[16] {};
    juce::RangedAudioParameter* mods[3][3] {};
    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotionFilterProcessor)
};

} // namespace craft::mfp
