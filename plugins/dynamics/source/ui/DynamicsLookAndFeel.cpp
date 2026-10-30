#include "DynamicsLookAndFeel.h"

namespace craft::dynamics::ui
{

juce::FontOptions uiFont (float height, bool bold)
{
    return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, corner);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, juce::Colour colour, juce::Justification j)
{
    // small coloured tab before the title identifies the module colour
    g.setColour (colour);
    g.fillRoundedRectangle (juce::Rectangle<float> (float (r.getX()), float (r.getCentreY()) - 5.0f, 3.0f, 10.0f), 1.5f);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.5f, true)));
    g.drawFittedText (text.toUpperCase(), r.withTrimmedLeft (9), j, 1);
}

CraftLookAndFeel::CraftLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextEditor::backgroundColourId, colours::well);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::highlightColourId, colours::accent.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId, colours::accent);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    setColour (juce::CaretComponent::caretColourId, colours::accent);
    setColour (juce::Slider::thumbColourId, colours::accent);
    setColour (juce::TooltipWindow::backgroundColourId, colours::well);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::panelEdge);
}

void CraftLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                                         juce::Slider::SliderStyle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    const auto accent = slider.findColour (juce::Slider::thumbColourId);

    const float grooveThickness = 4.0f;
    const auto groove = juce::Rectangle<float> (bounds.getCentreX() - grooveThickness / 2, bounds.getY(), grooveThickness, bounds.getHeight());
    g.setColour (colours::well);
    g.fillRoundedRectangle (groove.expanded (1.5f), 3.0f);

    // filled from the 0 dB position
    const float zero = float (slider.getPositionOfValue (0.0));
    g.setColour (accent.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (groove.getX(), std::min (zero, sliderPos), groove.getRight(),
                                                             std::max (zero, sliderPos)));

    const auto cap = juce::Rectangle<float> (26.0f, 14.0f).withCentre ({ bounds.getCentreX(), sliderPos });
    juce::ColourGradient grad (juce::Colour (0xff4a525b), cap.getX(), cap.getY(), juce::Colour (0xff2a2f35), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colour (0xff0b0d0f));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.setColour (accent);
    g.fillRect (juce::Rectangle<float> (cap.getWidth() - 8.0f, 2.0f).withCentre (cap.getCentre()));
}

juce::Font CraftLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::dynamics::ui
