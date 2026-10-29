#include "LimiterLookAndFeel.h"

namespace craft::limiter::ui
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

void drawTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, juce::Justification j)
{
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (12.0f, true)));
    g.drawFittedText (text.toUpperCase(), r, j, 1);
}

LimiterLookAndFeel::LimiterLookAndFeel()
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

void LimiterLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                                           float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    const auto accent = slider.findColour (juce::Slider::thumbColourId);

    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    if (! slider.getProperties().getWithDefault (overlayProperty, false))
    {
        // plain groove
        const auto groove = juce::Rectangle<float> (4.0f, bounds.getHeight()).withCentre (bounds.getCentre());
        g.setColour (colours::well);
        g.fillRoundedRectangle (groove.expanded (1.5f), 3.0f);
    }

    // Handle: a horizontal level line across the meter, with a grip tab on the left that carries a notch.
    const float full = float (slider.getWidth());
    const float lineY = sliderPos;
    g.setColour (accent.withAlpha (0.85f));
    g.fillRect (juce::Rectangle<float> (0.0f, lineY - 1.0f, full, 2.0f));

    const auto tab = juce::Rectangle<float> (0.0f, lineY - 7.0f, 22.0f, 14.0f);
    juce::Path p;
    p.startNewSubPath (tab.getX() + 3.0f, tab.getY());
    p.lineTo (tab.getRight() - 4.0f, tab.getY());
    p.lineTo (tab.getRight() + 3.0f, lineY);
    p.lineTo (tab.getRight() - 4.0f, tab.getBottom());
    p.lineTo (tab.getX() + 3.0f, tab.getBottom());
    p.quadraticTo (tab.getX(), tab.getBottom(), tab.getX(), tab.getBottom() - 3.0f);
    p.lineTo (tab.getX(), tab.getY() + 3.0f);
    p.quadraticTo (tab.getX(), tab.getY(), tab.getX() + 3.0f, tab.getY());
    p.closeSubPath();
    juce::ColourGradient grad (juce::Colour (0xff4a525b), 0.0f, tab.getY(), juce::Colour (0xff2a2f35), 0.0f, tab.getBottom(),
                               false);
    g.setGradientFill (grad);
    g.fillPath (p);
    g.setColour (slider.isMouseOverOrDragging() ? accent : juce::Colour (0xff0b0d0f));
    g.strokePath (p, juce::PathStrokeType (1.0f));
    g.setColour (accent);
    g.fillRect (juce::Rectangle<float> (12.0f, 2.0f).withCentre ({ tab.getX() + 10.0f, lineY }));
}

void LimiterLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float start,
                                           float end, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height)).reduced (4.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float angle = start + pos * (end - start);

    // tick ring
    for (int i = 0; i <= 20; ++i)
    {
        const float a = start + (end - start) * float (i) / 20.0f;
        const bool major = i % 4 == 0;
        const float r0 = radius - (major ? 7.0f : 5.0f), r1 = radius - 1.0f;
        g.setColour (major ? colours::textDim : colours::panelEdge.brighter (0.4f));
        g.drawLine (centre.x + r0 * std::sin (a), centre.y - r0 * std::cos (a), centre.x + r1 * std::sin (a),
                    centre.y - r1 * std::cos (a), major ? 1.5f : 1.0f);
    }

    // track + value arc
    const float arcR = radius - 12.0f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, angle, true);
    g.setColour (slider.isEnabled() ? accent : colours::textDim);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body
    const float bodyR = arcR - 8.0f;
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    juce::ColourGradient grad (juce::Colour (0xff3a4048), centre.x, body.getY(), juce::Colour (0xff1b1f24), centre.x,
                               body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff0b0d0f));
    g.drawEllipse (body, 1.2f);
    g.setColour (colours::panelEdge.brighter (0.25f));
    g.drawEllipse (body.reduced (3.0f), 0.8f);

    // pointer: a short bar on the body rim
    const float p0 = bodyR - 9.0f, p1 = bodyR - 2.0f;
    g.setColour (accent);
    g.drawLine (centre.x + p0 * std::sin (angle), centre.y - p0 * std::cos (angle), centre.x + p1 * std::sin (angle),
                centre.y - p1 * std::cos (angle), 3.0f);
}

juce::Font LimiterLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::limiter::ui
