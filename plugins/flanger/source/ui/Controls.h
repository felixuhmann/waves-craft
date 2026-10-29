// Craft Flanger - parameter-bound controls (spec r1 section 12).
#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::flanger::ui
{

// Numeric value box (spec 12):
//  - vertical drag: change the value (Shift = fine)   - single click: type a value
//  - double-click or Alt-click: reset to default
class ValueBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ValueBox (juce::RangedAudioParameter& param, std::function<juce::String()> textFn);

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
    std::function<juce::String()> textFn;
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

// Vertical fader with scale and value box underneath.
class Fader : public juce::Component
{
public:
    Fader (juce::AudioProcessorValueTreeState&, const juce::String& paramId, std::function<juce::String()> textFn,
           std::vector<ScaleMark> scale);

    void refresh() { box.refresh(); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    ValueBox box;
    std::vector<ScaleMark> scale;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// Polarity toggle: "+" (normal) or "-" (inverted).
class PolarityButton : public juce::Button
{
public:
    PolarityButton() : juce::Button ("polarity") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// Small toggle with a vector icon and optional caption.
class IconToggle : public juce::Button
{
public:
    enum class Icon
    {
        Tape,       // two reels
        Sync,       // note + clock tick
        Link,       // chain links
        Power,      // on/off
        Stop,       // square
        FilterType  // selector: low-pass (off) / high-pass (on) slope, never highlighted
    };

    explicit IconToggle (Icon i, juce::String caption = {});
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Icon icon;
    juce::String caption;
};

// Sine / Triangle selector with small waveform pictures (toggle state = Triangle).
class WaveformSwitch : public juce::Button
{
public:
    WaveformSwitch() : juce::Button ("waveform") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text,
                       juce::Justification j = juce::Justification::centredLeft);

} // namespace craft::flanger::ui
