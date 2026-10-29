// Craft De-Esser - editor (spec r2 section 12; all artwork drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "LookAndFeel.h"
#include "Meters.h"
#include "plugin/PluginProcessor.h"

namespace craft::deesser
{

class DeEsserEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DeEsserEditor (DeEsserProcessor&);
    ~DeEsserEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    DeEsserProcessor& proc;
    ui::CraftLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // left column
    ui::ModeToggle audioMode;
    ui::ValueBox frequencyBox;
    ui::FilterSelector filterSelector;
    ui::MonitorSelector monitorSelector;

    // centre
    juce::Slider thresholdSlider;
    juce::AudioProcessorValueTreeState::SliderAttachment thresholdAttachment;
    ui::ValueBox thresholdBox;
    ui::BarMeter sidechainMeter;
    ui::PeakReadout sidechainReadout;
    ui::BarMeter attenMeter;
    ui::PeakReadout attenReadout { true };

    // right
    ui::BarMeter outputMeter;
    ui::PeakReadout outputReadout[2];

    juce::Rectangle<int> audioBlock, freqBlock, monitorBlock, centreBlock, rightBlock, monitorLed;
    bool monitoringSidechain = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeEsserEditor)
};

} // namespace craft::deesser
