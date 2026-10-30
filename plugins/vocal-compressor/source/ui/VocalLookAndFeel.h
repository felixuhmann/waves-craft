// Craft Vocal Compressor - original look and feel, drawn entirely with vector graphics (Craft Audio house style:
// graphite panels; this product's accents are coral for the compressor and teal / violet for gate and output).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::vocal::ui
{

namespace colours
{
    const juce::Colour background { 0xff15181c };
    const juce::Colour panel      { 0xff1e2227 };
    const juce::Colour panelEdge  { 0xff2d333a };
    const juce::Colour well       { 0xff0e1013 };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff858f99 };
    const juce::Colour comp       { 0xffe9785f }; // coral: compression, gain reduction
    const juce::Colour gate       { 0xff8f8ae6 }; // violet: gate threshold
    const juce::Colour signal     { 0xff58c4b6 }; // teal: signal meters, output gain
    const juce::Colour warn       { 0xffe0604a };
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f);
void drawTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text,
                juce::Justification j = juce::Justification::centred);
// A small framed numeric readout.
void drawReadout (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour edge);

class VocalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VocalLookAndFeel();

    // Vertical sliders draw only their handle (they sit over a meter): a wide cap with a centre line.
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 9; }
    juce::Font getLabelFont (juce::Label&) override;
};

} // namespace craft::vocal::ui
