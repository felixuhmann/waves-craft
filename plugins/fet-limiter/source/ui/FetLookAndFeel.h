// Craft FET Limiter - original look and feel (Craft Audio family style), drawn entirely with vector graphics.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::fet_limiter::ui
{

namespace colours
{
    const juce::Colour background { 0xff14171b };
    const juce::Colour panel      { 0xff1d2126 };
    const juce::Colour panelEdge  { 0xff2c3239 };
    const juce::Colour well       { 0xff0d0f12 };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff7f8993 };
    const juce::Colour warn       { 0xffe0604a }; // over LED
    const juce::Colour themeA     { 0xffe3a447 }; // amber: revision A
    const juce::Colour themeB     { 0xff6fb6e8 }; // ice blue: revision B
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);

// The accent colour follows the Revision (two original colour themes, spec 12).
class FetLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FetLookAndFeel();

    void setAccent (juce::Colour c);
    juce::Colour getAccent() const noexcept { return accent; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour, bool isMouseOverButton,
                               bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;

private:
    juce::Colour accent = colours::themeA;
};

// A rounded panel with a small caps title in the top-left corner and an accent tick.
void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour tick);

} // namespace craft::fet_limiter::ui
