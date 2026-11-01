// Craft Enhancer - editor (spec r1 section 9; all artwork drawn in code).
#pragma once

#include <array>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "BandDisplay.h"
#include "Controls.h"
#include "EnhancerLookAndFeel.h"
#include "Meters.h"
#include "plugin/PluginProcessor.h"

namespace craft::enhancer
{

class EnhancerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EnhancerEditor (EnhancerProcessor&);
    ~EnhancerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Strip
    {
        std::unique_ptr<ui::BoundToggle> solo, bypass;
        std::unique_ptr<ui::BoundSlider> fader, widthKnob;
        std::unique_ptr<ui::ValueBox> gainBox, widthBox;
    };

    void timerCallback() override;
    ui::ValueBox* box (std::unique_ptr<ui::ValueBox>& slot, const juce::String& id);

    EnhancerProcessor& proc;
    ui::EnhancerLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    ui::BandDisplay display;
    std::array<std::unique_ptr<ui::ValueBox>, 4> crossoverBoxes;
    std::unique_ptr<ui::BoundSlider> inputKnob, outputKnob, directFader;
    std::unique_ptr<ui::ValueBox> inputBox, outputBox, directBox, punchBox;
    ui::StereoMeter inputMeter, outputMeter;
    std::array<Strip, 5> strips;
    juce::Label trimValue;
    juce::Rectangle<int> displayArea, xoverRow, leftArea, centreArea, rightArea;
    std::array<juce::Rectangle<int>, 5> stripAreas;
    juce::Rectangle<int> directArea;
    bool stereoShown = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnhancerEditor)
};

} // namespace craft::enhancer
