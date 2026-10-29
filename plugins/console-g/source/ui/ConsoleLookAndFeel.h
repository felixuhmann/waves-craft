// Craft Console G - original look and feel, drawn entirely with vector graphics (Craft Audio house style:
// graphite panels, coloured knob-group accents). No imitation of any hardware console.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::consoleg::ui
{

namespace colours
{
    const juce::Colour background { 0xff15181c };
    const juce::Colour panel      { 0xff1e2227 };
    const juce::Colour panelEdge  { 0xff2d333a };
    const juce::Colour well       { 0xff0e1013 };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff858f99 };
    // knob groups
    const juce::Colour comp       { 0xffe3a447 }; // amber
    const juce::Colour expander   { 0xff58c4b6 }; // teal
    const juce::Colour filters    { 0xffb48ee6 }; // lavender
    const juce::Colour hf         { 0xffe07a8f }; // rose
    const juce::Colour hmf        { 0xff8fcf6a }; // leaf
    const juce::Colour lmf        { 0xff6aa7e0 }; // sky
    const juce::Colour lf         { 0xffd9925a }; // copper
    const juce::Colour master     { 0xffc9d1d9 }; // silver
    const juce::Colour warn       { 0xffe0604a };
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent, const juce::String& title);

class ConsoleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ConsoleLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 10; }
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getLabelFont (juce::Label&) override;
};

} // namespace craft::consoleg::ui
