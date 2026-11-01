// Craft Audio doubler - original look and feel, drawn entirely with vector graphics
// (same family palette as the other Craft plug-ins).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::doubler::ui
{

namespace colours
{
    const juce::Colour background { 0xff15181c };
    const juce::Colour panel      { 0xff1e2227 };
    const juce::Colour panelEdge  { 0xff2d333a };
    const juce::Colour well       { 0xff0e1013 };
    const juce::Colour grid       { 0xff252a30 };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff858f99 };
    const juce::Colour accent     { 0xffe3a447 }; // amber
    const juce::Colour accent2    { 0xff58c4b6 }; // teal
    const juce::Colour warn       { 0xffe0604a };
    const juce::Colour direct     { 0xffcfd6de };

    // voice colour coding
    inline juce::Colour voice (int index0)
    {
        static const juce::Colour c[4] = { juce::Colour (0xffe3a447), juce::Colour (0xff58c4b6), juce::Colour (0xffe3739a),
                                           juce::Colour (0xff8f9cf0) };
        return c[index0 & 3];
    }
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);

class DoublerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DoublerLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 9; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
};

// Shared helpers
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f);
void drawCaption (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text,
                  juce::Justification j = juce::Justification::centred, float size = 10.5f);

} // namespace craft::doubler::ui
