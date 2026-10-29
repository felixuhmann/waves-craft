// Craft FET Limiter - editor (spec r1 section 12: one horizontal panel, original visuals).
#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "FetLookAndFeel.h"
#include "NeedleMeter.h"
#include "plugin/PluginProcessor.h"

namespace craft::fet_limiter
{

class FetLimiterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FetLimiterEditor (FetLimiterProcessor&);
    ~FetLimiterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyTheme();
    double paramValue (const char* id) const;

    FetLimiterProcessor& processor;
    ui::FetLookAndFeel lnf;

    ui::Knob input, output, attack, release, mix, trim;
    ui::ButtonGroup ratio, meterMode, analog, revision;
    ui::ToggleSwitch compOff, autoMakeup;
    ui::NeedleMeter meter;
    juce::TooltipWindow tooltips { this, 600 };

    // Auto Makeup gesture state (spec 8)
    bool linking = false;
    double linkInputStart = 0.0, linkOutputStart = 0.0;

    juce::Rectangle<int> gainArea, dynArea, meterArea, bottomArea, headerArea;
    int themeRevision = -1;
    double lastTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FetLimiterEditor)
};

} // namespace craft::fet_limiter
