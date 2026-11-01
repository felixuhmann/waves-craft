// Craft Opto Compressor - editor (spec r1 section 11). Original flat vector artwork.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

namespace craft::opto
{

class OptoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OptoLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
};

// Knob that reports whether a change comes from a user gesture in the editor (Auto Makeup, spec 8).
class Knob : public juce::Slider
{
public:
    Knob();
    bool isUserGesture() const noexcept { return gesture; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    bool gesture = false;
};

// Segmented selector bound to a choice parameter.
class Segments : public juce::Component
{
public:
    Segments (juce::RangedAudioParameter& param, juce::StringArray labels);
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment;
};

class LevelMeter : public juce::Component
{
public:
    void setValues (float levelDb, float reductionDb, int mode);
    void paint (juce::Graphics&) override;

private:
    float level = -100.0f, reduction = 0.0f;
    int mode = 1;
};

class OptoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OptoEditor (OptoProcessor&);
    ~OptoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    struct Control
    {
        Knob knob;
        juce::Label readout;
        std::unique_ptr<SliderAttachment> attachment;
        juce::String title;
    };
    void addControl (Control&, const char* id, const juce::String& title);
    void timerCallback() override;
    void onPeakReductionMoved();
    void updateReadouts();

    OptoProcessor& processor;
    OptoLookAndFeel lnf;
    Control gain, peak, hiFreq, mix, trim;
    Segments modeSel, analogSel, vuSel;
    juce::TextButton autoMakeup { "Auto Makeup" };
    juce::AudioProcessorValueTreeState::ButtonAttachment autoAttachment;
    LevelMeter meter;
    double lastPr = 0;
    float heldIn = 0, heldOut = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoEditor)
};

} // namespace craft::opto
