// Craft Peak Limiter - editor (spec r1 section 12; all artwork drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "LimiterLookAndFeel.h"
#include "Meters.h"
#include "plugin/PluginProcessor.h"

namespace craft::limiter
{

class PeakLimiterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PeakLimiterEditor (PeakLimiterProcessor&);
    ~PeakLimiterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PeakLimiterProcessor& proc;
    ui::LimiterLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    ui::MeterFader threshold, ceiling;
    ui::ReleaseKnob release;
    ui::LampSwitch autoRelease { "AUTO" };
    ui::GainReductionMeter grMeter;
    juce::AudioProcessorValueTreeState::ButtonAttachment autoAttachment;

    juce::Rectangle<int> thresholdBlock, ceilingBlock, releaseBlock, grBlock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PeakLimiterEditor)
};

} // namespace craft::limiter
