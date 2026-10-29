#include "CraftLookAndFeel.h"

namespace craft::ui
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
    setColour (juce::TextButton::buttonColourId, colours::panel);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setColour (juce::Slider::thumbColourId, colours::accent);
}

void CraftLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                                         float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const bool vertical = style == juce::Slider::LinearVertical;
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    const auto accent = slider.findColour (juce::Slider::thumbColourId);

    // groove
    const float grooveThickness = 4.0f;
    juce::Rectangle<float> groove = vertical
        ? juce::Rectangle<float> (bounds.getCentreX() - grooveThickness / 2, bounds.getY(), grooveThickness, bounds.getHeight())
        : juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - grooveThickness / 2, bounds.getWidth(), grooveThickness);
    g.setColour (colours::well);
    g.fillRoundedRectangle (groove.expanded (1.5f), 3.0f);

    // filled part: from the default value (centre-anchored sliders) or from the bottom
    const double def = slider.getDoubleClickReturnValue();
    const float defPos = float (vertical ? slider.getPositionOfValue (def) : slider.getPositionOfValue (def));
    const bool fromDefault = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    g.setColour (accent.withAlpha (0.55f));
    if (vertical)
    {
        const float from = fromDefault ? defPos : bounds.getBottom();
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (groove.getX(), std::min (from, sliderPos), groove.getRight(),
                                                                 std::max (from, sliderPos)));
    }
    else
    {
        const float from = fromDefault ? defPos : bounds.getX();
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (std::min (from, sliderPos), groove.getY(), std::max (from, sliderPos),
                                                                 groove.getBottom()));
    }

    // cap
    juce::Rectangle<float> cap = vertical ? juce::Rectangle<float> (26.0f, 14.0f).withCentre ({ bounds.getCentreX(), sliderPos })
                                          : juce::Rectangle<float> (12.0f, 22.0f).withCentre ({ sliderPos, bounds.getCentreY() });
    juce::ColourGradient grad (juce::Colour (0xff4a525b), cap.getX(), cap.getY(), juce::Colour (0xff2a2f35), cap.getX(),
                               cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colour (0xff0b0d0f));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.setColour (accent);
    if (vertical)
        g.fillRect (juce::Rectangle<float> (cap.getWidth() - 8.0f, 2.0f).withCentre (cap.getCentre()));
    else
        g.fillRect (juce::Rectangle<float> (2.0f, cap.getHeight() - 8.0f).withCentre (cap.getCentre()));
}

void CraftLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = b.getToggleState() ? colours::accent : colours::panel.brighter (over ? 0.15f : 0.05f);
    if (down)
        fill = fill.darker (0.2f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

juce::Font CraftLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (uiFont (std::min (13.0f, buttonHeight * 0.6f), true));
}

juce::Font CraftLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::ui
