// Craft Peak Limiter - original look and feel, drawn entirely with vector graphics (Craft Audio house style:
// graphite panels, amber / teal accents).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::limiter::ui
{

namespace colours
{
    const juce::Colour background { 0xff15181c };
    const juce::Colour panel      { 0xff1e2227 };
    const juce::Colour panelEdge  { 0xff2d333a };
    const juce::Colour well       { 0xff0e1013 };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff858f99 };
    const juce::Colour accent     { 0xffe3a447 }; // amber: threshold, knob
    const juce::Colour accent2    { 0xff58c4b6 }; // teal: ceiling, signal
    const juce::Colour warn       { 0xffe0604a }; // gain reduction
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);

// Draws a recessed rounded panel.
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f);

// Draws a block title in the house style.
void drawTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text,
                juce::Justification j = juce::Justification::centred);

class LimiterLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Slider property: when set, a vertical slider draws only its handle (it sits over a meter).
    static constexpr const char* overlayProperty = "craftOverlay";

    LimiterLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 8; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    juce::Font getLabelFont (juce::Label&) override;
};

} // namespace craft::limiter::ui
