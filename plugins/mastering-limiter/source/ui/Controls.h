// Craft Mastering Limiter - parameter-bound controls (spec r1 section 14).
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::mastering_limiter::ui
{

// Numeric value box: vertical drag changes the value (Shift = fine), single click types a value,
// double-click or Alt-click resets to the default (spec 14).
class ValueBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ValueBox (juce::RangedAudioParameter& param, std::function<juce::String()> textFn);

    void refresh();
    juce::RangedAudioParameter& getParameter() noexcept { return param; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void enablementChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Optional hook: called instead of setting the parameter directly (used for the marker link).
    std::function<void (float newNormalised)> onUserValue;

private:
    void timerCallback() override;
    void setNormalised (float v);

    juce::RangedAudioParameter& param;
    std::function<juce::String()> textFn;
    juce::Label label;
    float dragStart = 0.0f;
    bool dragging = false, gesture = false, suppressClick = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBox)
};

// Row of buttons selecting one entry of a choice parameter (Word Length / Dither / Shaping).
class SegmentedChoice : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentedChoice (juce::RangedAudioParameter& param, const juce::StringArray& labels);
    ~SegmentedChoice() override;

    void paint (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    int indexAt (juce::Point<float> p) const;
    juce::Rectangle<float> segment (int i) const;

    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    juce::ParameterAttachment attachment;
    int selected = 0, hover = -1;
};

} // namespace craft::mastering_limiter::ui
