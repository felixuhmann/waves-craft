// Craft Audio stereo imager - parameter-bound controls (spec r1 section 12).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::ui
{

// Numeric value box: shows the parameter value in its spec format.
//  - single click: type a value       - vertical drag: change the value
//  - double-click or Alt-click: reset to default
class ValueBox : public juce::Component, private juce::Timer
{
public:
    ValueBox (juce::RangedAudioParameter& param, juce::String paramId);

    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void resetToDefault();
    juce::String currentText() const;

    juce::RangedAudioParameter& param;
    const juce::String paramId;
    juce::Label label;
    float dragStart = 0.0f;
    bool dragging = false, gesture = false, suppressClick = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBox)
};

struct ScaleMark
{
    double value;
    juce::String text;
};

// Vertical fader: title on top, fader with scale, value box underneath.
class Fader : public juce::Component
{
public:
    Fader (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title,
           std::vector<ScaleMark> scale);

    void refresh() { box.refresh(); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    ValueBox box;
    juce::String title;
    std::vector<ScaleMark> scale;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// Horizontal centre-detented slider with title and value box.
class HSlider : public juce::Component
{
public:
    HSlider (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title,
             std::vector<ScaleMark> scale);

    void refresh() { box.refresh(); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    ValueBox box;
    juce::String title;
    std::vector<ScaleMark> scale;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// Two-position switch drawn as a segmented control ("L-R" | "M-S"); toggle state = right segment.
class SegmentSwitch : public juce::Button
{
public:
    SegmentSwitch (juce::String left, juce::String right);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::String leftText, rightText;
};

// Polarity toggle: shows "+" (normal) or "-" (inverted).
class PolarityButton : public juce::Button
{
public:
    PolarityButton() : juce::Button ("polarity") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// Channel swap toggle: double-headed arrow.
class SwapButton : public juce::Button
{
public:
    SwapButton() : juce::Button ("swap") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

} // namespace craft::ui
