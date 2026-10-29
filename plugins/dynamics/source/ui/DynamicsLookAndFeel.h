// Craft Dynamics - original look and feel (shared Craft palette), drawn entirely with vector graphics.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::dynamics::ui
{

namespace colours
{
    const juce::Colour background { 0xff15181c };
    const juce::Colour panel      { 0xff1e2227 };
    const juce::Colour panelEdge  { 0xff2d333a };
    const juce::Colour well       { 0xff0e1013 };
    const juce::Colour grid       { 0xff242a31 };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff858f99 };
    const juce::Colour accent     { 0xffe3a447 }; // amber: compressor, active band
    const juce::Colour accent2    { 0xff58c4b6 }; // teal: gate / expander
    const juce::Colour passive    { 0xff9c8ce0 }; // lavender: passive band
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
    juce::Font getLabelFont (juce::Label&) override;
};

// A recessed rounded panel.
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f);
void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, juce::Colour colour,
                       juce::Justification j = juce::Justification::centredLeft);

} // namespace craft::dynamics::ui
