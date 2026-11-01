// Craft Dynamic Distortion - JUCE AudioProcessor around the host-independent DSP core.
#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/DynamicDistortion.h"

namespace craft::ddp
{

class DynamicDistortionProcessor : public juce::AudioProcessor
{
public:
    DynamicDistortionProcessor();
    ~DynamicDistortionProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Craft Dynamic Distortion"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    // The physical settings the parameters currently describe (also used by the editor's response curve).
    dd::Settings readSettings() const;

    // Live values for the editor's meters (peak levels since the editor last read them, linear).
    struct Live
    {
        std::atomic<float> inPeak[2] { { 0.0f }, { 0.0f } };
        std::atomic<float> outPeak[2] { { 0.0f }, { 0.0f } };
        std::atomic<float> expansionDb { 0.0f };
    };
    Live live;

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    float value (juce::RangedAudioParameter* p) const noexcept { return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f; }

    juce::AudioProcessorValueTreeState state;
    dd::DynamicDistortion dsp;

    juce::RangedAudioParameter *input {}, *direct {}, *drive {}, *distLevel {}, *attack {}, *release {}, *bass {}, *mid {},
        *midFreq {}, *treble {}, *lowPass {}, *output {};
    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicDistortionProcessor)
};

} // namespace craft::ddp
