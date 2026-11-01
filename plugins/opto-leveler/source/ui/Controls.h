// Craft Opto Leveler - parameter-bound controls (spec r1 section 12).
#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::opto_leveler::ui
{

// Rotary slider: drag / wheel, double-click or Alt-click resets to the default, the value box below is editable.
class ResetSlider : public juce::Slider
{
public:
    ResetSlider() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow) {}
    void mouseDown (const juce::MouseEvent& e) override;
};

// Knob with caption, an optional scale (tick marks at display values, placed with the parameter's own taper) and
// optional end labels.
class Knob : public juce::Component
{
public:
    struct Tick
    {
        float value;
        juce::String label;
    };

    Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& caption,
          juce::Colour colour, std::vector<Tick> ticks = {});

    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }

private:
    juce::Rectangle<float> dialBounds() const;

    juce::RangedAudioParameter& param;
    juce::String caption;
    std::vector<Tick> ticks;
    ResetSlider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Row of buttons selecting one entry of a choice (or bool) parameter.
class SegmentedChoice : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentedChoice (juce::RangedAudioParameter& param, const juce::StringArray& labels, juce::Colour colour);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    int indexAt (juce::Point<float> p) const;
    juce::Rectangle<float> segment (int i) const;

    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    juce::Colour colour;
    juce::ParameterAttachment attachment;
    int selected = 0, hover = -1;
};

} // namespace craft::opto_leveler::ui
