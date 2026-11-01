// Craft Enhancer - parameter-bound controls (spec r1 section 9).
#pragma once

#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::enhancer::ui
{

using APVTS = juce::AudioProcessorValueTreeState;

// Numeric value box: vertical drag changes the value (Shift = fine), click types a value, double-click resets.
class ValueBox : public juce::Component, private juce::Timer
{
public:
    explicit ValueBox (juce::RangedAudioParameter& param);

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    void enablementChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // Optional limit of the normalised value (crossover neighbours).
    std::function<float (float)> limit;

private:
    void timerCallback() override;
    void setNormalised (float v);

    juce::RangedAudioParameter& param;
    juce::Label label;
    float dragStart = 0.0f;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBox)
};

// Slider (rotary or vertical fader) attached to a parameter; double-click resets.
class BoundSlider : public juce::Slider
{
public:
    BoundSlider (APVTS&, const juce::String& paramId, juce::Slider::SliderStyle, juce::Colour accent);

private:
    APVTS::SliderAttachment attachment;
};

// Toggle attached to a bool parameter.
class BoundToggle : public juce::TextButton
{
public:
    BoundToggle (APVTS&, const juce::String& paramId, const juce::String& text, juce::Colour onColour);

private:
    APVTS::ButtonAttachment attachment;
};

} // namespace craft::enhancer::ui
