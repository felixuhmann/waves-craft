// Craft Multiband - editor (spec r2 section 8; all artwork drawn in code).
#pragma once

#include <array>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "FrequencyDisplay.h"
#include "Theme.h"
#include "plugin/PluginProcessor.h"

namespace craft::multiband
{

class MultibandEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MultibandEditor (MultibandProcessor&);
    ~MultibandEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct BandColumn
    {
        std::unique_ptr<ui::ThresholdMeter> meter;
        std::array<std::unique_ptr<ui::ValueBox>, 5> boxes; // Threshold, Gain, Range, Attack, Release
        std::unique_ptr<ui::PillToggle> solo, bypass;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAtt, bypassAtt;
        juce::Rectangle<int> area;
    };

    MultibandProcessor& proc;
    ui::MultibandLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    ui::FrequencyDisplay display;
    std::array<BandColumn, numBands> columns;
    std::array<std::unique_ptr<ui::OffsetBox>, 5> offsets; // Threshold, Gain, Range, Attack, Release
    ui::ValueBox knee, q;
    ui::SegmentSwitch releaseMode, behavior;
    ui::OutputSection output;

    juce::Rectangle<int> displayArea, masterArea, outputArea;
    std::array<juce::Rectangle<int>, 5> offsetLabels;
    juce::Rectangle<int> kneeLabel, qLabel, releaseLabel, behaviorLabel;
    std::array<float, numBands> lastGains {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandEditor)
};

} // namespace craft::multiband
