// Craft Audio stereo imager - editor (spec r1 section 12; all artwork drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "CraftLookAndFeel.h"
#include "Meters.h"
#include "VectorDisplay.h"
#include "plugin/PluginProcessor.h"

namespace craft
{

class CraftEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CraftEditor (CraftProcessor&);
    ~CraftEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void resetImage();
    bool full() const noexcept { return proc.getVariant() == dsp::Variant::Full; }
    bool msOnly() const noexcept { return proc.getVariant() == dsp::Variant::MsMatrix; }

    CraftProcessor& proc;
    ui::CraftLookAndFeel lnf;

    std::vector<std::unique_ptr<ui::Fader>> faders;
    std::unique_ptr<ui::VectorDisplay> display;
    std::unique_ptr<ui::HSlider> asymmetry, rotation;
    std::unique_ptr<ui::ValueBox> bassTrim;
    std::unique_ptr<ui::MeterPanel> meters;

    ui::SegmentSwitch inputMode { "L-R", "M-S" };
    ui::PolarityButton polarity1, polarity2;
    ui::SwapButton swap;
    juce::TextButton resetButton { "Reset" };
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    juce::Rectangle<int> leftBlock, centreBlock, rightBlock, ioRow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CraftEditor)
};

} // namespace craft
