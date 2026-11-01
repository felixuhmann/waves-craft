// Craft Dynamic Distortion - original look and feel, drawn entirely with vector graphics (Craft Audio family style).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::ddp::ui
{

namespace colours
{
    const juce::Colour background { 0xff15161a };
    const juce::Colour panel      { 0xff1f2126 };
    const juce::Colour panelEdge  { 0xff30333a };
    const juce::Colour well       { 0xff0e0f12 };
    const juce::Colour text       { 0xffdcdfe4 };
    const juce::Colour textDim    { 0xff858b94 };
    const juce::Colour heat       { 0xffef6a3a }; // distortion section
    const juce::Colour heatHot    { 0xffffb347 };
    const juce::Colour tone       { 0xff6fc3a8 }; // tone section
    const juce::Colour level      { 0xffa9b4c2 }; // input / output
    const juce::Colour grid       { 0xff272a30 };
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);

class DistortionLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DistortionLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float startAngle,
                           float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour, bool isMouseOverButton,
                               bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
};

// A rounded panel with a small caps title and a coloured tick.
void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& subtitle,
                  juce::Colour tick);

} // namespace craft::ddp::ui
