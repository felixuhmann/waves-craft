#include "Theme.h"

namespace craft::multiband::ui
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

void drawTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, juce::Justification j, juce::Colour c)
{
    g.setColour (c);
    g.setFont (juce::Font (uiFont (11.5f, true)));
    g.drawFittedText (text.toUpperCase(), r, j, 1);
}

MultibandLookAndFeel::MultibandLookAndFeel()
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
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::panelEdge);
}

void MultibandLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                             float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    const auto accent = slider.findColour (juce::Slider::thumbColourId);

    // groove
    const auto groove = juce::Rectangle<float> (4.0f, bounds.getHeight()).withCentre (bounds.getCentre());
    g.setColour (colours::well);
    g.fillRoundedRectangle (groove.expanded (1.5f), 3.0f);

    // cap: a rounded bar with a centre line
    const auto cap = juce::Rectangle<float> (std::min (34.0f, bounds.getWidth() - 2.0f), 16.0f).withCentre ({ bounds.getCentreX(), sliderPos });
    juce::ColourGradient grad (juce::Colour (0xff4a525b), 0.0f, cap.getY(), juce::Colour (0xff2a2f35), 0.0f, cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (slider.isMouseOverOrDragging() ? accent : juce::Colour (0xff0b0d0f));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.setColour (accent);
    g.fillRect (juce::Rectangle<float> (cap.getWidth() - 10.0f, 2.0f).withCentre (cap.getCentre()));
}

juce::Font MultibandLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::multiband::ui
