// Craft Level Rider - small parameter-bound controls (spec r1 section 13).
#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::levelrider::ui
{

// Numeric value box. Single click: type a value; vertical drag: change it; double-click or Alt-click: default.
// Works on a host parameter, or on custom callbacks (the Rider fader value, section 9).
class ValueBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    struct Callbacks
    {
        std::function<juce::String()> text;            // display text (with unit)
        std::function<juce::String()> editText;        // text shown when the editor opens
        std::function<void (const juce::String&)> set; // typed text
        std::function<float()> getNorm;                // 0..1
        std::function<void (float)> setNorm;
        std::function<void()> beginDrag, endDrag, reset;
    };

    ValueBox (juce::RangedAudioParameter& param, juce::String paramId);
    explicit ValueBox (Callbacks callbacks);

    void refresh();
    void setFontHeight (float h);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void init();
    void timerCallback() override;

    Callbacks cb;
    juce::Label label;
    float dragStart = 0.0f;
    bool dragging = false, gesture = false, suppressClick = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBox)
};

// Two-state parameter shown as a segmented switch (e.g. Attack Slow | Fast); a click selects a state.
class SegmentSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    // Bound to a parameter (index = round(norm * (n - 1))).
    SegmentSwitch (juce::RangedAudioParameter& param, juce::StringArray names);
    // Not bound to a parameter: index getter / setter callbacks.
    SegmentSwitch (juce::StringArray names, std::function<int()> get, std::function<void (int)> set);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
    void setLitColours (std::vector<juce::Colour> c) { lit = std::move (c); }

private:
    juce::Rectangle<float> segment (int index) const;

    juce::StringArray names;
    std::function<int()> getIndex;
    std::function<void (int)> setIndex;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int index = 0;
    std::vector<juce::Colour> lit;
};

// Rotary knob bound to a parameter, with a caption and a typing value box underneath.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, juce::String caption, bool bipolar);

    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() { box.refresh(); }

private:
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    ValueBox box;
    juce::String caption;
};

} // namespace craft::levelrider::ui
