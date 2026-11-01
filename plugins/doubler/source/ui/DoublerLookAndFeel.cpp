#include "DoublerLookAndFeel.h"

namespace craft::doubler::ui
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

void drawCaption (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, juce::Justification j, float size)
{
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (size, true)));
    g.drawFittedText (text.toUpperCase(), r, j, 1);
}

DoublerLookAndFeel::DoublerLookAndFeel()
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
    setColour (juce::TooltipWindow::backgroundColourId, colours::well);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::panelEdge);
}

void DoublerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float a0,
                                           float a1, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height)).reduced (2.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float angle = a0 + pos * (a1 - a0);

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - 1.5f, radius - 1.5f, 0.0f, a0, a1, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc from the centre detent (bipolar ranges) or from the start
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0 && slider.getMaximum() + slider.getMinimum() == 0.0;
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, radius - 1.5f, radius - 1.5f, 0.0f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (accent.withAlpha (slider.isEnabled() ? 0.9f : 0.35f));
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // cap
    const float capR = radius - 5.0f;
    juce::ColourGradient grad (juce::Colour (0xff4a525b), c.x, c.y - capR, juce::Colour (0xff262a30), c.x, c.y + capR, false);
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (2 * capR, 2 * capR).withCentre (c));
    g.setColour (juce::Colour (0xff0b0d0f));
    g.drawEllipse (juce::Rectangle<float> (2 * capR, 2 * capR).withCentre (c), 1.0f);

    // pointer
    const juce::Point<float> tip (c.x + (capR - 1.5f) * std::sin (angle), c.y - (capR - 1.5f) * std::cos (angle));
    const juce::Point<float> base (c.x + capR * 0.3f * std::sin (angle), c.y - capR * 0.3f * std::cos (angle));
    g.setColour (slider.isMouseOverOrDragging() ? accent.brighter (0.3f) : colours::text);
    g.drawLine ({ base, tip }, 2.0f);
}

void DoublerLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                                           float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const bool vertical = style == juce::Slider::LinearVertical;
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    const auto accent = slider.findColour (juce::Slider::thumbColourId);

    const float t = 4.0f;
    const auto groove = vertical ? juce::Rectangle<float> (bounds.getCentreX() - t / 2, bounds.getY(), t, bounds.getHeight())
                                 : juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - t / 2, bounds.getWidth(), t);
    g.setColour (colours::well);
    g.fillRoundedRectangle (groove.expanded (1.5f), 3.0f);
    g.setColour (accent.withAlpha (0.55f));
    if (vertical)
        g.fillRect (groove.withTop (sliderPos));
    else
        g.fillRect (groove.withRight (sliderPos));

    const auto cap = vertical ? juce::Rectangle<float> (28.0f, 14.0f).withCentre ({ bounds.getCentreX(), sliderPos })
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

void DoublerLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
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

juce::Font DoublerLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (uiFont (std::min (12.5f, buttonHeight * 0.6f), true));
}

juce::Font DoublerLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::doubler::ui
