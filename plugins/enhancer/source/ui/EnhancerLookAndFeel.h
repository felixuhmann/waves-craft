// Craft Enhancer - original look and feel, drawn entirely with vector graphics (Craft Audio family style).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::enhancer::ui
{

namespace colours
{
    const juce::Colour background { 0xff14171b };
    const juce::Colour panel      { 0xff1d2126 };
    const juce::Colour panelEdge  { 0xff2c3239 };
    const juce::Colour well       { 0xff0d0f12 };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff7f8993 };
    const juce::Colour accent     { 0xffe3a447 };
    const juce::Colour solo       { 0xffe6cf5a };
    const juce::Colour bypass     { 0xffe0604a };
    const juce::Colour grid       { 0xff262b31 };
} // namespace colours

juce::Colour bandColour (int band);
juce::FontOptions uiFont (float height, bool bold = false);

class EnhancerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EnhancerLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float startAngle,
                           float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
};

void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour tick);

} // namespace craft::enhancer::ui
