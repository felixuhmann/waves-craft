// Craft Console G - editor (spec section 11 functional layout; all artwork is original and drawn in code).
#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ConsoleLookAndFeel.h"
#include "Controls.h"
#include "Meters.h"
#include "plugin/PluginProcessor.h"

namespace craft::consoleg
{

class ConsoleGEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ConsoleGEditor (ConsoleGProcessor&);
    ~ConsoleGEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    ui::Knob& knob (const char* id, const char* name, juce::Colour c, bool bipolar = false);
    ui::Switch& toggle (const char* id, const char* text, juce::Colour c, const char* tip = "");

    ConsoleGProcessor& proc;
    ui::ConsoleLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    std::vector<std::unique_ptr<ui::Knob>> knobs;
    std::vector<std::unique_ptr<ui::Switch>> switches;

    // dynamics
    ui::Knob *compRatio, *compThreshold, *compRelease, *expRange, *expThreshold, *expRelease;
    ui::Switch *compFast, *expFast, *expGate, *dynBypass, *dynPostEq, *filtersSc, *filtersSplit;
    ui::BarMeter grMeter { true };
    // filters
    ui::Knob *hp, *lp;
    // EQ
    ui::Knob *hfGain, *hfFreq, *hmfGain, *hmfFreq, *hmfQ, *lmfGain, *lmfFreq, *lmfQ, *lfGain, *lfFreq;
    ui::Switch *hmfX3, *lmfDiv3, *eqBypass;
    // master
    ui::Knob* trim;
    std::unique_ptr<ui::Fader> output;
    ui::Switch *analog, *polarity;
    juce::TextButton meterIn { "IN" }, meterOut { "OUT" };
    ui::BarMeter ioMeter { false };

    juce::Rectangle<int> dynPanel, compArea, expArea, routeArea, filterPanel, eqPanel, masterPanel;
    std::array<juce::Rectangle<int>, 4> bandAreas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConsoleGEditor)
};

} // namespace craft::consoleg
