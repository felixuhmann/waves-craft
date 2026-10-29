// Craft Console G - parameter-bound controls (spec section 11).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::consoleg::ui
{

// Rotary knob with its name above and the displayed value (spec section 2 format) below.
// Drag to change, double-click resets to the default, the value can be typed after a click on the value text.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, juce::String name, juce::Colour accent,
          bool bipolar = false);

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::RangedAudioParameter& param;
    juce::String name;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    juce::Label value;
};

// Vertical fader (output gain).
class Fader : public juce::Component
{
public:
    Fader (juce::AudioProcessorValueTreeState& state, const char* paramId, juce::String name, juce::Colour accent);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::RangedAudioParameter& param;
    juce::String name;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    juce::Label value;
};

// Latching switch bound to a two-state parameter.
class Switch : public juce::ToggleButton
{
public:
    Switch (juce::AudioProcessorValueTreeState& state, const char* paramId, juce::String text, juce::Colour accent,
            juce::String tooltip = {});

private:
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

} // namespace craft::consoleg::ui
