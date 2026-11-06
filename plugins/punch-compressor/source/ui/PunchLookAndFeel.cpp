#include "PunchLookAndFeel.h"

namespace craft::punch::ui
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
    g.setFont (juce::Font (uiFont (11.5f, true)));
    g.drawFittedText (text.toUpperCase(), r, j, 1);
}

PunchLookAndFeel::PunchLookAndFeel()
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

// Knob: a dotted scale, a recessed track with a value arc, a faceted body with a slot pointer.
void PunchLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float start,
                                         float end, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height)).reduced (3.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float angle = start + pos * (end - start);
    const bool large = radius > 60.0f;

    // dotted scale
    const int dots = large ? 25 : 11;
    for (int i = 0; i < dots; ++i)
    {
        const float a = start + (end - start) * float (i) / float (dots - 1);
        const float r = radius - 2.5f;
        const bool lit = float (i) / float (dots - 1) <= pos + 1.0e-4f;
        g.setColour (lit ? accent.withAlpha (0.9f) : colours::panelEdge.brighter (0.5f));
        const float d = (i == 0 || i == dots - 1) ? 3.2f : 2.2f;
        g.fillEllipse (juce::Rectangle<float> (d, d).withCentre ({ centre.x + r * std::sin (a), centre.y - r * std::cos (a) }));
    }

    // track + value arc
    const float arcR = radius - (large ? 11.0f : 9.0f);
    const float trackW = large ? 6.0f : 4.5f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, angle, true);
    g.setColour (slider.isEnabled() ? accent : colours::textDim);
    g.strokePath (value, juce::PathStrokeType (trackW - 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body: twelve-sided cap with a soft top light
    const float bodyR = arcR - (large ? 10.0f : 7.0f);
    juce::Path body;
    for (int i = 0; i < 12; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * float (i) / 12.0f + angle;
        const auto pt = juce::Point<float> (centre.x + bodyR * std::sin (a), centre.y - bodyR * std::cos (a));
        if (i == 0)
            body.startNewSubPath (pt);
        else
            body.lineTo (pt);
    }
    body.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (body, juce::AffineTransform::translation (0.0f, 2.5f));
    juce::ColourGradient grad (juce::Colour (0xff434a53), centre.x, centre.y - bodyR, juce::Colour (0xff1a1e23), centre.x,
                               centre.y + bodyR, false);
    g.setGradientFill (grad);
    g.fillPath (body);
    g.setColour (juce::Colour (0xff0a0c0e));
    g.strokePath (body, juce::PathStrokeType (1.2f));
    const float capR = bodyR * 0.62f;
    juce::ColourGradient cap (juce::Colour (0xff2b3138), centre.x, centre.y - capR, juce::Colour (0xff3a414a), centre.x,
                              centre.y + capR, false);
    g.setGradientFill (cap);
    g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre));

    // pointer slot
    const float p0 = bodyR * 0.30f, p1 = bodyR - 3.0f;
    juce::Path ptr;
    ptr.startNewSubPath (centre.x + p0 * std::sin (angle), centre.y - p0 * std::cos (angle));
    ptr.lineTo (centre.x + p1 * std::sin (angle), centre.y - p1 * std::cos (angle));
    g.setColour (slider.isEnabled() ? accent : colours::textDim);
    g.strokePath (ptr, juce::PathStrokeType (large ? 3.5f : 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font PunchLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::punch::ui
