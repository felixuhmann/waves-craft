#include "LookAndFeel.h"

namespace craft::levelrider::ui
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

void drawLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float brightness)
{
    brightness = juce::jlimit (0.0f, 1.0f, brightness);
    if (brightness > 0.02f)
    {
        g.setColour (colour.withAlpha (0.35f * brightness));
        g.fillEllipse (r.expanded (3.0f));
    }
    g.setColour (colours::well.interpolatedWith (colour, brightness));
    g.fillEllipse (r);
    g.setColour (colours::panelEdge.brighter (0.3f));
    g.drawEllipse (r, 1.0f);
}

void drawFaderCap (juce::Graphics& g, juce::Rectangle<float> cap, juce::Colour stripe, bool hot)
{
    juce::ColourGradient grad (juce::Colour (hot ? 0xff59626c : 0xff4a525b), cap.getX(), cap.getY(), juce::Colour (0xff2a2f35),
                               cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colour (0xff0b0d0f));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.setColour (stripe);
    g.fillRect (juce::Rectangle<float> (cap.getWidth() - 8.0f, 2.0f).withCentre (cap.getCentre()));
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
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.3f));
}

void CraftLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float a0, float a1,
                                         juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height)).reduced (4.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float arcR = radius - 2.0f;
    const auto accent = slider.findColour (juce::Slider::thumbColourId);

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc from the centre detent (bipolar) or from the start
    const float angle = a0 + pos * (a1 - a0);
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0 && slider.getProperties()["bipolar"];
    const float from = bipolar ? (a0 + a1) / 2.0f : a0;
    if (slider.isEnabled() && std::abs (angle - from) > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body
    const float bodyR = arcR - 7.0f;
    juce::ColourGradient grad (juce::Colour (0xff4a525b), centre.x, centre.y - bodyR, juce::Colour (0xff23272c), centre.x,
                               centre.y + bodyR, false);
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (2 * bodyR, 2 * bodyR).withCentre (centre));
    g.setColour (juce::Colour (0xff0b0d0f));
    g.drawEllipse (juce::Rectangle<float> (2 * bodyR, 2 * bodyR).withCentre (centre), 1.0f);

    // pointer
    const juce::Point<float> tip (centre.x + (bodyR - 3.0f) * std::sin (angle), centre.y - (bodyR - 3.0f) * std::cos (angle));
    const juce::Point<float> base (centre.x + bodyR * 0.35f * std::sin (angle), centre.y - bodyR * 0.35f * std::cos (angle));
    g.setColour (slider.isEnabled() ? colours::text : colours::textDim);
    g.drawLine ({ base, tip }, 2.5f);
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

} // namespace craft::levelrider::ui
