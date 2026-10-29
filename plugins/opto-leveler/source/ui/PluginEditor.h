// Craft Opto Leveler - editor (spec r1 section 12; all artwork original and drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "LevelMeter.h"
#include "OptoLookAndFeel.h"
#include "plugin/PluginProcessor.h"

namespace craft::opto_leveler
{

class OptoLevelerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OptoLevelerEditor (OptoLevelerProcessor&);
    ~OptoLevelerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    OptoLevelerProcessor& proc;
    ui::OptoLookAndFeel lnf;

    std::unique_ptr<ui::Knob> gainKnob, prKnob, hiFreqKnob, mixKnob, trimKnob;
    std::unique_ptr<ui::SegmentedChoice> modeChoice, vuChoice, analogChoice;
    juce::TextButton autoMakeupButton { "AUTO MAKEUP" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoMakeupAttachment;
    ui::LevelMeter meter;
    juce::RangedAudioParameter* vuParam = nullptr;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Rectangle<int> leftPanel, centrePanel, rightPanel, bottomPanel;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> captions;
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoLevelerEditor)
};

} // namespace craft::opto_leveler
