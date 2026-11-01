// Craft Band Limiter - AudioProcessor shared by all four products (CRAFT_VARIANT). Stereo, mono via bus layout.
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/BandLimiterEngine.h"

#ifndef CRAFT_VARIANT
 #define CRAFT_VARIANT 0
#endif

namespace craft::band_limiter
{

class BandLimiterProcessor : public juce::AudioProcessor
{
public:
    BandLimiterProcessor();
    ~BandLimiterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return CRAFT_PRODUCT_NAME; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    static constexpr dsp::Variant variant = dsp::Variant (CRAFT_VARIANT);

    struct MeterData
    {
        float input = 0.0f, output = 0.0f, minGain = 1.0f;
        std::array<float, dsp::numBands> bandGain { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        std::array<float, dsp::numBands> bandPeak {};
    };
    MeterData takeMeters() noexcept; // GUI thread

private:
    dsp::Settings readSettings() const;
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);

    juce::AudioProcessorValueTreeState state;
    dsp::BandLimiterEngine engine { variant };

    juce::RangedAudioParameter *threshold = nullptr, *ceiling = nullptr, *separation = nullptr, *behaviour = nullptr,
                               *multiplier = nullptr, *profile = nullptr, *quantize = nullptr, *dither = nullptr, *shaping = nullptr;
    std::array<juce::RangedAudioParameter*, dsp::numBands> gain {}, priority {}, release {}, solo {};
    std::array<juce::RangedAudioParameter*, dsp::numCrossovers> crossover {};
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, minGain { 1.0f };
    std::array<std::atomic<float>, dsp::numBands> bandGainMeter, bandPeakMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandLimiterProcessor)
};

} // namespace craft::band_limiter
