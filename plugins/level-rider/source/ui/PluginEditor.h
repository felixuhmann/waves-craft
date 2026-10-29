// Craft Level Rider - editor (spec r1 section 13; all artwork drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "Faders.h"
#include "LookAndFeel.h"
#include "plugin/PluginProcessor.h"

namespace craft::levelrider
{

class LevelRiderEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LevelRiderEditor (LevelRiderProcessor&);
    ~LevelRiderEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void riderOneShot (double db);

    LevelRiderProcessor& proc;
    ui::CraftLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // top section
    ui::TargetBar targetBar;
    ui::ValueBox targetBox;
    ui::SegmentSwitch attackSwitch;
    ui::Knob vocalKnob, musicKnob;
    std::unique_ptr<ui::Knob> spillKnob;

    // main section
    ui::RangeFader rangeFader;
    ui::ValueBox maxBox, minBox, idleBox;
    ui::RiderFader riderFader;
    ui::ValueBox riderBox;
    ui::OutputFader outputFader;
    ui::ValueBox outputBox;

    // bottom
    ui::SegmentSwitch automationSwitch;

    juce::Rectangle<int> topLeft, topRight, rangeBlock, riderBlock, outputBlock, bottomBlock, rideLed, idleLed, musicLed;
    LevelRiderProcessor::Display display;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelRiderEditor)
};

} // namespace craft::levelrider
