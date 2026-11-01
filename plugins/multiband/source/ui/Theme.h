// Craft Multiband - original look and feel, drawn entirely with vector graphics (Craft Audio house style:
// graphite panels, one accent colour per band).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::multiband::ui
{

namespace colours
{
    const juce::Colour background { 0xff15181c };
    const juce::Colour panel      { 0xff1e2227 };
    const juce::Colour panelEdge  { 0xff2d333a };
    const juce::Colour well       { 0xff0e1013 };
    const juce::Colour grid       { 0xff23282e };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff858f99 };
    const juce::Colour accent     { 0xffe3a447 }; // amber: master / output
    const juce::Colour signal     { 0xff58c4b6 }; // teal: meters
    const juce::Colour warn       { 0xffe0604a };
    const juce::Colour solo       { 0xfff2cf4a };
    const juce::Colour bypass     { 0xff8a96a3 };

    // One hue per band, low to high.
    inline juce::Colour band (int b)
    {
        static const juce::Colour c[] = { juce::Colour (0xffe0734f), juce::Colour (0xffd9b44a), juce::Colour (0xff5fc48a),
                                          juce::Colour (0xff5d9fe0) };
        return c[juce::jlimit (0, 3, b)];
    }
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f);
void drawTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text,
                juce::Justification j = juce::Justification::centred, juce::Colour colour = colours::textDim);

class MultibandLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MultibandLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 8; }
    juce::Font getLabelFont (juce::Label&) override;
};

} // namespace craft::multiband::ui
