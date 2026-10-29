#include "ColourLookAndFeel.h"

namespace craft::ui
{

juce::FontOptions uiFont (float height, bool bold)
{
    return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour rule, float corner)
{
    juce::ColourGradient grad (colours::panel.brighter (0.04f), r.getX(), r.getY(), colours::panel.darker (0.12f), r.getX(),
                               r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, corner);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    if (! rule.isTransparent())
    {
        g.setColour (rule);
        g.fillRoundedRectangle (r.withHeight (3.0f).reduced (corner, 0.0f), 1.5f);
    }
}

void drawTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, juce::Justification j, juce::Colour c,
                float size)
{
    g.setColour (c);
    g.setFont (juce::Font (uiFont (size, true)).withExtraKerningFactor (0.08f));
    g.drawFittedText (text.toUpperCase(), r, j, 1);
}

ColourLookAndFeel::ColourLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::ComboBox::backgroundColourId, colours::well);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, colours::panelEdge);
    setColour (juce::ComboBox::arrowColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::background);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::panelEdge);
}

void ColourLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float a0,
                                          float a1, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const bool enabled = s.isEnabled();
    const float track = std::max (2.5f, radius * 0.09f);
    const float arcR = radius - track * 0.5f;
    const float angle = a0 + pos * (a1 - a0);

    // track
    juce::Path bg;
    bg.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (colours::well);
    g.strokePath (bg, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc: from the start, or from the centre for bipolar controls
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? a0 + (float) s.valueToProportionOfLength (0.0) * (a1 - a0) : a0;
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, std::min (from, angle), std::max (from, angle), true);
    g.setColour (enabled ? accent : accent.withSaturation (0.1f).withAlpha (0.5f));
    g.strokePath (arc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // tick marks
    g.setColour (colours::panelEdge.brighter (0.3f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / 10.0f;
        const auto p1 = c.getPointOnCircumference (radius + 1.5f, a), p2 = c.getPointOnCircumference (radius + (i % 5 == 0 ? 5.0f : 3.5f), a);
        if (radius > 30.0f)
            g.drawLine ({ p1, p2 }, 1.0f);
    }

    // body
    const float bodyR = radius - track - std::max (3.0f, radius * 0.1f);
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (body.translated (0.0f, 2.0f));
    juce::ColourGradient grad (juce::Colour (0xff3a4048), c.x, body.getY(), juce::Colour (0xff1c2025), c.x, body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff4a525b));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    g.setColour (juce::Colour (0xff23282e));
    g.drawEllipse (body.reduced (bodyR * 0.22f), 1.0f);

    // pointer
    const auto p0 = c.getPointOnCircumference (bodyR * (radius > 60.0f ? 0.66f : 0.35f), angle), p1 = c.getPointOnCircumference (bodyR * 0.9f, angle);
    g.setColour (enabled ? colours::text : colours::textDim);
    g.drawLine ({ p0, p1 }, std::max (2.0f, bodyR * 0.09f));
    g.setColour (enabled ? accent : colours::textDim);
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (c.getPointOnCircumference (bodyR * 0.9f, angle)));
}

void ColourLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const bool enabled = box.isEnabled();
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (enabled && box.isMouseOver (true) ? colours::accent.withAlpha (0.7f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    // arrow
    const float ax = r.getRight() - 12.0f, ay = r.getCentreY();
    juce::Path p;
    p.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (enabled ? box.findColour (juce::ComboBox::arrowColourId) : colours::panelEdge.brighter (0.2f));
    g.fillPath (p);
}

juce::Font ColourLookAndFeel::getComboBoxFont (juce::ComboBox&) { return juce::Font (uiFont (13.0f, true)); }

void ColourLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

juce::Font ColourLookAndFeel::getPopupMenuFont() { return juce::Font (uiFont (14.0f)); }

void ColourLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
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

} // namespace craft::ui
