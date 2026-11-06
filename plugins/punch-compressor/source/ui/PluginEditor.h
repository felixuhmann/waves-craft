// Craft Punch Compressor - editor (spec r1 section 14; all artwork drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PunchLookAndFeel.h"
#include "Widgets.h"
#include "plugin/PluginProcessor.h"

namespace craft::punch
{

class PunchCompressorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PunchCompressorEditor (PunchCompressorProcessor&);
    ~PunchCompressorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int editorWidth = 880, editorHeight = 440;

private:
    void timerCallback() override;
    void updateSyncDependentControls();

    PunchCompressorProcessor& proc;
    ui::PunchLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    ui::VuMeter vu;
    ui::Selector meterSelect, syncSelect, analogSelect, stageSelect;
    ui::Knob threshold, ratio, attack, releaseMs, releaseNote, punch, mix, output;
    ui::ValueBox tempoBox;
    juce::Label hostTempo;

    juce::Rectangle<int> leftPanel, centrePanel, rightPanel, tempoRow;
    int lastSync = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchCompressorEditor)
};

} // namespace craft::punch
