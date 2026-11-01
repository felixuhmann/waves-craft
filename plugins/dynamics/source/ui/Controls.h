// Craft Dynamics - parameter-bound controls (spec r1 section 12).
#pragma once

#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::dynamics::ui
{

// Numeric value box: vertical drag changes the value (Shift = fine), single click types a value,
// double-click or Alt-click resets to the default, the wheel steps.
class ValueBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ValueBox (juce::RangedAudioParameter& param, juce::Colour accent);

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void resetToDefault();

    juce::RangedAudioParameter& param;
    juce::Colour accent;
    juce::Label label;
    float dragStart = 0.0f;
    bool dragging = false, gesture = false, suppressClick = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBox)
};

// Row of mutually exclusive segments bound to a choice (or bool) parameter.
class SegmentedChoice : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentedChoice (juce::RangedAudioParameter& param, juce::StringArray labels, juce::Colour accent);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int indexAt (juce::Point<float>) const;
    int current() const;

    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    juce::Colour accent;
    int hover = -1;
    juce::ParameterAttachment attachment;
};

// Single toggle bound to a bool parameter; lit in `onColour` when on.
class ParamToggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Icon { None, Link, Power };

    ParamToggle (juce::RangedAudioParameter& param, juce::String text, juce::Colour onColour, Icon icon = Icon::None);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    bool isOn() const { return param.getValue() >= 0.5f; }

private:
    juce::RangedAudioParameter& param;
    juce::String text;
    juce::Colour onColour;
    Icon icon;
    juce::ParameterAttachment attachment;
};

// Vertical fader with a dB scale and a value box underneath.
class Fader : public juce::Component
{
public:
    Fader (juce::AudioProcessorValueTreeState&, const juce::String& paramId, std::vector<std::pair<double, juce::String>> scale);

    void refresh() { box.refresh(); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    ValueBox box;
    std::vector<std::pair<double, juce::String>> scale;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

} // namespace craft::dynamics::ui
