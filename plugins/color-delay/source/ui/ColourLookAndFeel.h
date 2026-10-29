// Craft Colour Delay - original look and feel, drawn entirely with vector graphics (no images, no bundled fonts).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::ui
{

namespace colours
{
    const juce::Colour background { 0xff14171b };
    const juce::Colour panel      { 0xff1d2126 };
    const juce::Colour panelEdge  { 0xff2d333a };
    const juce::Colour well       { 0xff0d0f12 };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff858f99 };
    const juce::Colour accent     { 0xffe3a447 }; // amber (delay section)
    const juce::Colour accent2    { 0xff58c4b6 }; // teal
    const juce::Colour warn       { 0xffe0604a };

    // one colour per colour effect
    const juce::Colour reverb     { 0xff5fa8d3 };
    const juce::Colour distortion { 0xffe0704a };
    const juce::Colour doubler    { 0xffa98be0 };
    const juce::Colour phaser     { 0xff7cc576 };
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);

class ColourLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ColourLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY, int buttonW,
                       int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
};

// Recessed rounded panel with an optional coloured top rule.
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour rule = juce::Colours::transparentBlack,
                float corner = 8.0f);

// Small caps-style section title.
void drawTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text,
                juce::Justification j = juce::Justification::centred, juce::Colour c = colours::textDim, float size = 11.5f);

} // namespace craft::ui
