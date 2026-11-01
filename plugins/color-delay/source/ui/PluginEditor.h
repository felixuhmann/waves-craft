// Craft Colour Delay - editor (spec r1 section 12; functional layout as specified, all artwork original and drawn in code).
#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ColourLookAndFeel.h"
#include "Widgets.h"
#include "plugin/PluginProcessor.h"

namespace craft
{

class ColourDelayEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ColourDelayEditor (ColourDelayProcessor&);
    ~ColourDelayEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateModeAndLink();
    void attachButton (const char* id, juce::Button& b, const juce::String& tip);
    void attachCombo (const char* id, juce::ComboBox& c);
    void layoutEffect (int index, juce::Rectangle<int> area);
    void drawBadge (juce::Graphics& g, juce::Rectangle<float> r);

    ColourDelayProcessor& proc;
    ui::ColourLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    using K = ui::Knob;
    K feedbackL, feedbackR, delayL, delayR, dryWet, hp, lp, input, output;
    K verbLevel, verbSize, distLevel, distSend, distWidth, dblLevel, dblDetune, phLevel, phRate;
    ui::LedButton delayLink { "Link", ui::colours::accent }, filterLink { "Link", ui::colours::accent };
    ui::LedButton verbOn { "Reverb", ui::colours::reverb }, distOn { "Distortion", ui::colours::distortion },
                  dblOn { "Doubler", ui::colours::doubler }, phOn { "Phaser", ui::colours::phaser };
    ui::SegmentedChoice mode, monitor;
    juce::ComboBox noteL, noteR;
    ui::Meter inMeter { "In" }, outMeter { "Out" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;

    juce::Rectangle<int> leftCol, rightCol, delayPanel, fxPanels[4], badge;
    bool hostMode = true, linked = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColourDelayEditor)
};

} // namespace craft
