// Craft Colour Delay - parameter-bound controls (spec r1 section 12).
#pragma once

#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::ui
{

// Rotary control with title. Small/medium knobs show the value instead of the title while hovered or dragged;
// large knobs always show the value in their centre. Double-click or Alt-click resets to the default.
class Knob : public juce::Component, private juce::Slider::Listener
{
public:
    enum class Style { Large, Medium, Small };

    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::String title, Style, juce::Colour accent);
    ~Knob() override;

    void bindTo (const juce::String& paramId); // re-attach (Delay knobs switch between ms and note)
    const juce::String& getParamId() const noexcept { return paramId; }
    void setDimmed (bool d);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent&) override { repaint(); }

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    juce::String valueText() const;
    juce::Rectangle<int> knobArea() const;

    juce::AudioProcessorValueTreeState& state;
    juce::String paramId, title;
    Style style;
    juce::Colour accent;
    bool dimmed = false;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Toggle with an indicator light (effect on/off, Link buttons).
class LedButton : public juce::Button
{
public:
    LedButton (const juce::String& text, juce::Colour led);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Colour led;
};

// Segmented selector bound to a choice or bool parameter (Mode ms/Host, Monitor L/ST/R).
class SegmentedChoice : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentedChoice (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::StringArray labels, juce::Colour accent);

    int getIndex() const noexcept { return index; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    juce::Colour accent;
    int index = 0;
    juce::ParameterAttachment attachment;
};

// Vertical two-channel peak meter (display only): -48 ... +6 dBFS with falling bars and peak hold.
class Meter : public juce::Component
{
public:
    explicit Meter (juce::String title) : title (std::move (title)) {}
    void push (float peakL, float peakR);
    void paint (juce::Graphics&) override;

private:
    juce::String title;
    float level[2] { -100.0f, -100.0f }, hold[2] { -100.0f, -100.0f };
    int holdAge[2] {};
};

} // namespace craft::ui
