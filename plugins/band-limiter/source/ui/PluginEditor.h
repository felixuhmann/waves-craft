// Craft Band Limiter - editor (spec r1 section 13): Multi and Profile layouts, original artwork drawn in code.
#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "BandDisplay.h"
#include "Controls.h"
#include "LookAndFeel.h"
#include "plugin/PluginProcessor.h"

namespace craft::band_limiter
{

class BandLimiterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BandLimiterEditor (BandLimiterProcessor&);
    ~BandLimiterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct BandStrip
    {
        std::unique_ptr<juce::TextButton> solo, link;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;
        std::unique_ptr<ui::ValueBox> gain, priority, release;
        juce::Rectangle<int> area, meterArea;
        float peak = 0.0f, gainValue = 1.0f;
    };

    void timerCallback() override;
    std::unique_ptr<ui::ValueBox> makeBox (juce::RangedAudioParameter* p, const juce::String& tip);
    void moveLevel (juce::RangedAudioParameter* moved, float db);
    void layoutSelectors (juce::Rectangle<int> area);

    BandLimiterProcessor& proc;
    ui::CraftLookAndFeel lnf;
    const bool profileLayout;

    juce::RangedAudioParameter* threshold = nullptr;
    juce::RangedAudioParameter* ceiling = nullptr;
    std::unique_ptr<ui::LevelMeter> inputMeter, outputMeter;
    ui::AttenuationMeter attenuation;
    std::unique_ptr<ui::ValueBox> thresholdBox, ceilingBox;
    juce::TextButton levelLink { "Link" };
    float dragOffsetDb = 0.0f;

    std::unique_ptr<ui::BandDisplay> display;
    std::array<BandStrip, dsp::numBands> strips;
    std::array<std::unique_ptr<ui::ValueBox>, dsp::numCrossovers> crossoverBoxes;
    std::unique_ptr<ui::ValueBox> separationBox, multiplierBox;
    std::unique_ptr<ui::SegmentedChoice> behaviour, profile, quantize, dither, shaping;
    juce::Rectangle<int> behaviourLabel, profileLabel, quantizeLabel, ditherLabel, shapingLabel, levelPanel, xoPanel, outPanel;
    float activeLed = 0.0f;
    juce::Rectangle<int> ledArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandLimiterEditor)
};

} // namespace craft::band_limiter
