// Craft Console E - parameter-bound controls (spec r1 section 13).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::consolee::ui
{

// Rotary control with a caption above and the value (section 2 display resolution) below.
//  - drag: change   - double-click the knob or Alt-click: default   - double-click the value: type a value
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, juce::String caption, bool bipolar = false,
          bool offAtZero = false);

    void resized() override;
    void refreshValue();

private:
    juce::RangedAudioParameter& param;
    const juce::String paramId;
    juce::Slider slider;
    juce::Label caption, value;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

// Latching switch with an indicator dot.
class Switch : public juce::TextButton
{
public:
    Switch (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& text, const juce::String& tooltip,
            bool warn = false);

private:
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

// Two-way selector shown as two adjacent segments (e.g. Expander | Gate, In | Out).
class Selector : public juce::Component, public juce::SettableTooltipClient
{
public:
    Selector (juce::AudioProcessorValueTreeState& state, const char* paramId, juce::String offText, juce::String onText);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::String offText, onText;
    bool on = false;
    juce::ParameterAttachment attachment;
};

// Vertical output fader with its value underneath.
class Fader : public juce::Component
{
public:
    Fader (juce::AudioProcessorValueTreeState& state, const char* paramId);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refreshValue();
    juce::Rectangle<int> getTrackArea() const;

private:
    const juce::String paramId;
    juce::Slider slider;
    juce::Label value;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

} // namespace craft::consolee::ui
