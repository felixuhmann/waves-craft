// Craft Hybrid Delay - editor (spec r1 section 13; all artwork drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "HDelayLookAndFeel.h"
#include "Widgets.h"
#include "plugin/PluginProcessor.h"

namespace craft::hdelay
{

class HDelayEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HDelayEditor (HDelayProcessor&);
    ~HDelayEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int editorWidth = 1000, editorHeight = 440;

private:
    void timerCallback() override;
    void handleTap (double intervalMs);
    void updateSyncDependentControls();

    HDelayProcessor& proc;
    ui::HDelayLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    ui::TapPad tap;
    juce::TextButton lofi { "LOFI" };
    ui::Knob delayMsKnob, delayNoteKnob, feedbackKnob, mixKnob, outputKnob, analogKnob;
    ui::Knob depthKnob, rateKnob, hiPassKnob, loPassKnob;
    ui::LinkRing link;
    ui::DelayDisplay display;
    ui::OutputMeter meter;
    juce::TextButton phaseL { juce::String::fromUTF8 ("\xc3\x98 L") }, pingPong { "PING PONG" }, dual { "DUAL" },
        phaseR { juce::String::fromUTF8 ("\xc3\x98 R") };
    juce::TextButton syncBpm { "BPM" }, syncHost { "HOST" }, syncMs { "MS" };
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    juce::Rectangle<int> leftPanel, delayPanel, centrePanel, feedbackPanel, rightPanel, modPanel, filterPanel;
    int lastSync = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HDelayEditor)
};

} // namespace craft::hdelay
