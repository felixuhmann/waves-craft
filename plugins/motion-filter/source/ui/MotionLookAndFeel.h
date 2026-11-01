// Craft Motion Filter - original look and feel, drawn entirely with vector graphics (Craft Audio family style).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::mfp::ui
{

namespace colours
{
    const juce::Colour background { 0xff14171b };
    const juce::Colour panel      { 0xff1d2126 };
    const juce::Colour panelEdge  { 0xff2c3239 };
    const juce::Colour well       { 0xff0d0f12 };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff7f8993 };
    const juce::Colour accent     { 0xffe3a447 }; // amber: filter
    const juce::Colour lfo        { 0xff58c4b6 }; // teal: LFO
    const juce::Colour seq        { 0xffb48cf0 }; // violet: sequencer
    const juce::Colour follower   { 0xff7fb7ef }; // blue: envelope follower
    const juce::Colour delay      { 0xffe07a5f }; // coral: delay
    const juce::Colour grid       { 0xff262b31 };
} // namespace colours

// Colour of modulation source 0..2 (LFO, SEQ, FOLLOW).
juce::Colour sourceColour (int source);

juce::FontOptions uiFont (float height, bool bold = false);

class MotionLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MotionLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float startAngle,
                           float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour, bool isMouseOverButton,
                               bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
};

// A rounded panel with a small caps title in the top-left corner and a coloured tick.
void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour tick);

} // namespace craft::mfp::ui
