// Craft Console E - editor (spec r1 section 13; all artwork drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "LookAndFeel.h"
#include "Meters.h"
#include "plugin/PluginProcessor.h"

namespace craft::consolee
{

class ConsoleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ConsoleEditor (ConsoleProcessor&);
    ~ConsoleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    ConsoleProcessor& proc;
    ui::CraftLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // dynamics
    ui::Knob compRatio, compThreshold, compRelease;
    ui::Switch compFast;
    ui::ReductionMeter compMeter { "GAIN REDUCTION", 20.0f, { 20.0f, 14.0f, 10.0f, 6.0f, 3.0f, 0.0f } };
    ui::Knob expRange, expThreshold, expRelease;
    ui::Switch expFast;
    ui::Selector expMode;
    ui::ReductionMeter expMeter { "EXP / GATE", 40.0f, { 40.0f, 30.0f, 20.0f, 10.0f, 0.0f } };
    ui::Switch dynBypass, channelOut;

    // equaliser
    ui::Knob hp, lp;
    ui::Switch split;
    ui::Knob hfGain, hfFreq;
    ui::Switch hfBell;
    ui::Knob hmfGain, hmfFreq, hmfQ;
    ui::Knob lmfGain, lmfFreq, lmfQ;
    ui::Knob lfGain, lfFreq;
    ui::Switch lfBell;
    ui::Switch eqBypass, eqToSidechain;

    // I/O
    ui::Knob inputTrim;
    ui::Switch polarity, analogue;
    ui::Selector meterSelect;
    ui::LevelMeter levelMeter;
    ui::Fader outputFader;

    juce::Rectangle<int> dynPanel, compBlock, expBlock, eqPanel, ioPanel;
    std::array<juce::Rectangle<int>, 6> eqRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConsoleEditor)
};

} // namespace craft::consolee
