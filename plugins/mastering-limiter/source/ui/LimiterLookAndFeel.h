// Craft Mastering Limiter - original look and feel (shared Craft palette), drawn entirely with vector graphics.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::mastering_limiter::ui
{

namespace colours
{
    const juce::Colour background { 0xff15181c };
    const juce::Colour panel      { 0xff1e2227 };
    const juce::Colour panelEdge  { 0xff2d333a };
    const juce::Colour well       { 0xff0e1013 };
    const juce::Colour text       { 0xffd8dde3 };
    const juce::Colour textDim    { 0xff858f99 };
    const juce::Colour accent     { 0xffe3a447 }; // amber: threshold, active controls
    const juce::Colour accent2    { 0xff58c4b6 }; // teal: levels, ceiling
    const juce::Colour warn       { 0xffe0604a }; // gain reduction
} // namespace colours

juce::FontOptions uiFont (float height, bool bold = false);

class CraftLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CraftLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
};

// Shared helpers
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f);
void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text,
                       juce::Justification j = juce::Justification::centredLeft);

// dB scale used by all meters: 0 dB at the top, -30 dB at the bottom (spec 14).
inline constexpr float meterFloorDb = -30.0f;
inline float dbToY (float db, juce::Rectangle<float> r)
{
    const float t = juce::jlimit (0.0f, 1.0f, db / meterFloorDb);
    return r.getY() + t * r.getHeight();
}
inline float yToDb (float y, juce::Rectangle<float> r)
{
    return juce::jlimit (meterFloorDb, 0.0f, (y - r.getY()) / r.getHeight() * meterFloorDb);
}
inline float linToDb (float lin) { return lin > 1.0e-10f ? 20.0f * std::log10 (lin) : -200.0f; }

} // namespace craft::mastering_limiter::ui
