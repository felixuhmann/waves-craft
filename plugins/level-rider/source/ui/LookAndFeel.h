// Craft Level Rider - look and feel (shared Craft Audio palette), drawn entirely with vector graphics.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::levelrider::ui
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
    const juce::Colour ride       { 0xff6fd08c }; // green status
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);

class CraftLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CraftLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
};

// Shared helpers.
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f);
void drawLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float brightness);
void drawFaderCap (juce::Graphics& g, juce::Rectangle<float> cap, juce::Colour stripe, bool hot);

} // namespace craft::levelrider::ui
