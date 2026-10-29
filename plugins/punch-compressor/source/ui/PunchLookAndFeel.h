// Craft Punch Compressor - original look and feel, drawn entirely with vector graphics (Craft Audio house style:
// graphite panels, amber / teal accents; this product adds a copper "punch" accent for gain reduction).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::punch::ui
{

namespace colours
{
    const juce::Colour background { 0xff14171b };
    const juce::Colour panel      { 0xff1d2126 };
    const juce::Colour panelEdge  { 0xff2c3239 };
    const juce::Colour well       { 0xff0d0f12 };
    const juce::Colour text       { 0xffd9dee4 };
    const juce::Colour textDim    { 0xff86909a };
    const juce::Colour accent     { 0xffe3a447 }; // amber: controls
    const juce::Colour accent2    { 0xff58c4b6 }; // teal: signal level
    const juce::Colour punch      { 0xffd8693e }; // copper: gain reduction, punch
    const juce::Colour dial       { 0xffe9e1cf }; // meter face
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f);
void drawTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text,
                juce::Justification j = juce::Justification::centred);

class PunchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PunchLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;
};

} // namespace craft::punch::ui
