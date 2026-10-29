// Craft Audio stereo imager - original look and feel, drawn entirely with vector graphics.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::ui
{

namespace colours
{
    const juce::Colour background { 0xff15181c };
    const juce::Colour panel      { 0xff1e2227 };
    const juce::Colour panelEdge  { 0xff2d333a };
    const juce::Colour well       { 0xff0e1013 };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff858f99 };
    const juce::Colour accent     { 0xffe3a447 }; // amber
    const juce::Colour accent2    { 0xff58c4b6 }; // teal
    const juce::Colour warn       { 0xffe0604a };
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);

class CraftLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CraftLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 9; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
};

// Shared helper: a recessed rounded panel.
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f);

} // namespace craft::ui
