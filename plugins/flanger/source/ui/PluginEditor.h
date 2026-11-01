// Craft Flanger - editor (spec r1 section 12; all artwork drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "FlangerLookAndFeel.h"
#include "LfoDisplay.h"
#include "Meters.h"
#include "plugin/PluginProcessor.h"

namespace craft::flanger
{

class FlangerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FlangerEditor (FlangerProcessor&);
    ~FlangerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    ui::ValueBox& makeBox (const char* id, int quantity, const juce::String& tooltip);
    void attach (const char* id, juce::Button& b, const juce::String& tooltip);
    bool hasStereo() const noexcept { return proc.getVariant() != dsp::Variant::Mono; }

    FlangerProcessor& proc;
    ui::CraftLookAndFeel lnf;

    std::vector<std::unique_ptr<ui::ValueBox>> boxes;
    ui::ValueBox *mixBox = nullptr, *feedbackBox = nullptr, *filterBox = nullptr, *delayBox = nullptr, *rateBox = nullptr,
                 *depthBox = nullptr, *stereoBox = nullptr;
    std::unique_ptr<ui::Fader> gainFader;

    ui::PolarityButton wetPolarity, feedbackPolarity;
    ui::IconToggle filterType { ui::IconToggle::Icon::FilterType }, filterOn { ui::IconToggle::Icon::Power },
        tape { ui::IconToggle::Icon::Tape, "TAPE" }, sync { ui::IconToggle::Icon::Sync, "SYNC" },
        link { ui::IconToggle::Icon::Link }, stop { ui::IconToggle::Icon::Stop, "STOP" };
    ui::WaveformSwitch waveform;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    ui::LfoDisplay lfoDisplay;
    ui::StereoPhaseIndicator phaseIndicator;
    std::unique_ptr<ui::MeterPanel> meters;
    juce::TooltipWindow tooltips { this, 600 };

    std::array<juce::Rectangle<int>, 4> panels;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> captions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangerEditor)
};

} // namespace craft::flanger
