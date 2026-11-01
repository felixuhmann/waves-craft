// Craft Dynamic Distortion - editor (spec r1 section 12; all artwork drawn in code).
#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "DistortionLookAndFeel.h"
#include "ToneDisplay.h"
#include "plugin/PluginProcessor.h"

namespace craft::ddp
{

class DynamicDistortionEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DynamicDistortionEditor (DynamicDistortionProcessor&);
    ~DynamicDistortionEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    DynamicDistortionProcessor& proc;
    ui::DistortionLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<ui::Knob> input, drive, attack, release, direct, distLevel, output;
    std::unique_ptr<ui::Knob> bass, mid, midFreq, treble, lowPass;
    ui::LevelMeter inMeter, outMeter;
    ui::ToneDisplay toneDisplay;
    juce::TextButton bypass { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::vector<ui::Knob*> knobs;

    juce::Rectangle<int> distArea, toneArea, dynamicsBox, mixBox, expansionBar;
    float expansionDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicDistortionEditor)
};

} // namespace craft::ddp
