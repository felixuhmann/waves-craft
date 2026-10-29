// Craft Hybrid Delay - original look and feel, drawn entirely with vector graphics.
// Palette shared with the other Craft Audio plug-ins (dark panels, amber and teal accents).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::hdelay::ui
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
    const juce::Colour lcd        { 0xff0a1514 }; // display background (teal-tinted black)
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);

// Recessed rounded panel with an optional section title.
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {}, float corner = 7.0f);

class HDelayLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HDelayLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

} // namespace craft::hdelay::ui
