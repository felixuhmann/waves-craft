// Craft Vocal Compressor - editor (spec r1 section 12; all artwork drawn in code).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "VocalLookAndFeel.h"
#include "plugin/PluginProcessor.h"

namespace craft::vocal
{

class VocalCompressorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocalCompressorEditor (VocalCompressorProcessor&);
    ~VocalCompressorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    VocalCompressorProcessor& proc;
    ui::VocalLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    ui::FaderStrip gate, compression, gain;
    juce::Rectangle<int> gateBlock, compBlock, gainBlock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalCompressorEditor)
};

} // namespace craft::vocal
