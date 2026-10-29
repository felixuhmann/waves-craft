// Craft Audio doubler - editor (spec r1 section 12; all artwork drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "Displays.h"
#include "DoublerLookAndFeel.h"
#include "Meters.h"
#include "plugin/PluginProcessor.h"

namespace craft::doubler
{

class DoublerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DoublerEditor (DoublerProcessor&);
    ~DoublerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    using Attachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    juce::Button& addToggle (std::unique_ptr<juce::Button> b, const juce::String& paramId, const juce::String& tooltip);
    ui::Knob& addKnob (const juce::String& paramId, const juce::String& unit, juce::Colour accent);
    ui::ValueBox& addBox (const juce::String& paramId, const juce::String& unit, juce::Colour accent, const juce::String& tooltip);

    struct VoiceRow
    {
        juce::Button* on = nullptr;
        ui::Knob *gain = nullptr, *pan = nullptr;
        ui::ValueBox *delay = nullptr, *feedback = nullptr, *detune = nullptr, *depth = nullptr, *rate = nullptr;
        juce::Button* octave = nullptr;
    };

    DoublerProcessor& proc;
    ui::DoublerLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<Attachment>> attachments;
    std::vector<ui::Knob*> knobs;
    std::vector<ui::ValueBox*> boxes;

    std::unique_ptr<ui::StageDisplay> stage;
    std::unique_ptr<ui::DelayPlaneDisplay> plane;
    std::unique_ptr<ui::EqDisplay> eq;
    std::unique_ptr<ui::OutputSection> output;
    std::unique_ptr<ui::ChoiceSegments> routingA, routingB;
    std::unique_ptr<ui::MomentaryButton> modReset;

    // middle strip
    juce::Button *directOn = nullptr, *align = nullptr, *range = nullptr, *eqLowOn = nullptr, *eqHighOn = nullptr;
    ui::Knob *directGain = nullptr, *directPan = nullptr, *eqLowGain = nullptr, *eqHighGain = nullptr;
    ui::ValueBox *fbHighPass = nullptr, *eqLowFreq = nullptr, *eqHighFreq = nullptr;
    std::vector<VoiceRow> rows;

    juce::Rectangle<int> stripPanel, voicePanel, routingPanel, outputPanel;
    struct Caption
    {
        juce::Rectangle<int> r;
        juce::String text;
        juce::Justification j = juce::Justification::centred;
    };
    std::vector<Caption> captions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoublerEditor)
};

} // namespace craft::doubler
