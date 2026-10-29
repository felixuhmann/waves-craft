// Craft Motion Filter - editor (spec r1 section 13; all artwork drawn in code).
#pragma once

#include <array>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "MotionLookAndFeel.h"
#include "ResponseDisplay.h"
#include "StepEditor.h"
#include "plugin/PluginProcessor.h"

namespace craft::mfp
{

class MotionFilterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MotionFilterEditor (MotionFilterProcessor&);
    ~MotionFilterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MotionFilterProcessor& proc;
    ui::MotionLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // filter
    std::unique_ptr<ui::ChoiceStrip> typeStrip;
    std::unique_ptr<ui::Knob> freqKnob, resKnob;
    std::array<std::unique_ptr<ui::ModSlider>, 3> freqMods, resMods, timeMods;
    std::unique_ptr<ui::SegmentToggle> slope, kbd;
    // global / mixer / follower
    std::unique_ptr<ui::Knob> spread, smooth, drive, crush, mix, input, output;
    std::unique_ptr<ui::SegmentToggle> folSource;
    // display
    std::unique_ptr<ui::ResponseDisplay> display;
    // LFO
    std::unique_ptr<ui::Knob> lfoRate;
    std::unique_ptr<ui::SegmentToggle> lfoSync;
    std::unique_ptr<ui::ChoiceStrip> lfoShape;
    // sequencer
    std::unique_ptr<ui::StepEditor> steps;
    std::unique_ptr<ui::DragValue> seqLast;
    std::unique_ptr<ui::Knob> swing, seqRate;
    std::unique_ptr<ui::SegmentToggle> seqSync, drawMode;
    // delay
    std::unique_ptr<ui::Knob> dlyTime, dlyFeedback, dlyMix;
    std::unique_ptr<ui::ChoiceStrip> dlyRatio;
    std::unique_ptr<ui::SegmentToggle> dlySync, dlyMode, dlyRoute;
    // header
    std::unique_ptr<ui::SegmentToggle> bypass;

    std::vector<ui::Knob*> knobs;
    std::vector<ui::ModSlider*> modSliders;

    juce::Rectangle<int> filterArea, displayArea, globalArea, mixerArea, followerArea, lfoArea, seqArea, delayArea;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> captions; // small labels above toggles

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotionFilterEditor)
};

} // namespace craft::mfp
