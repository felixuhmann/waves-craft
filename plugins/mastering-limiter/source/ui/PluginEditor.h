// Craft Mastering Limiter - editor (spec r1 section 14; all artwork drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "LimiterLookAndFeel.h"
#include "Meters.h"
#include "plugin/PluginProcessor.h"

namespace craft::mastering_limiter
{

class MasteringLimiterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MasteringLimiterEditor (MasteringLimiterProcessor&);
    ~MasteringLimiterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    float valueDb (juce::RangedAudioParameter* p) const;
    bool isLinked() const;
    // Sets `primary` to `db`; with the link on, moves the other marker by the same amount (offset kept).
    void applyLevel (juce::RangedAudioParameter* primary, float db);
    void beginLevelGesture (juce::RangedAudioParameter* primary);
    void endLevelGesture (juce::RangedAudioParameter* primary);
    juce::RangedAudioParameter* partnerOf (juce::RangedAudioParameter* p) const;

    MasteringLimiterProcessor& proc;
    ui::CraftLookAndFeel lnf;
    juce::RangedAudioParameter *threshold = nullptr, *ceiling = nullptr, *release = nullptr;

    ui::MarkerMeter inputMeter { ui::colours::accent, ui::MarkerMeter::ScaleSide::Left };
    ui::MarkerMeter outputMeter { ui::colours::text, ui::MarkerMeter::ScaleSide::Left };
    ui::GainReductionMeter grMeter;
    ui::PeakReadout attenReadout { true }, outPeak[2];

    std::unique_ptr<ui::ValueBox> thresholdBox, ceilingBox, releaseBox;
    juce::TextButton adaptiveButton { "ADAPTIVE" }, linkButton { "LINK" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> adaptiveAttachment;
    std::unique_ptr<ui::SegmentedChoice> wordLength, dither, shaping;
    juce::TooltipWindow tooltips { this, 600 };

    std::array<juce::Rectangle<int>, 4> panels;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> captions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasteringLimiterEditor)
};

} // namespace craft::mastering_limiter
