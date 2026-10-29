#include "ConsoleLookAndFeel.h"

namespace craft::consoleg::ui
{

juce::FontOptions uiFont (float height, bool bold)
{
    return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent, const juce::String& title)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    // accent strip + title
    auto head = r.removeFromTop (22.0f).reduced (8.0f, 0.0f);
    g.setColour (accent.withAlpha (0.85f));
    g.fillRoundedRectangle (juce::Rectangle<float> (head.getX(), head.getBottom() - 2.0f, head.getWidth(), 2.0f), 1.0f);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.5f, true)));
    g.drawText (title.toUpperCase(), head, juce::Justification::centredLeft);
}

ConsoleLookAndFeel::ConsoleLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextEditor::backgroundColourId, colours::well);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::highlightColourId, colours::comp.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId, colours::panelEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::comp);
    setColour (juce::CaretComponent::caretColourId, colours::comp);
    setColour (juce::Slider::thumbColourId, colours::comp);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxBackgroundColourId, colours::well);
    setColour (juce::Slider::textBoxOutlineColourId, colours::panelEdge);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::panelEdge);
}

void ConsoleLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float start,
                                           float end, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height)).reduced (3.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float angle = start + pos * (end - start);
    const bool bipolar = slider.getProperties().getWithDefault ("bipolar", false);

    // value arc on a recessed track
    const float arcR = radius - 3.0f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, std::min (from, angle), std::max (from, angle), true);
    g.setColour (slider.isEnabled() ? accent : colours::textDim);
    g.strokePath (value, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body: flat disc with a soft rim, a coloured cap ring and a pointer line
    const float bodyR = arcR - 6.0f;
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    juce::ColourGradient grad (juce::Colour (0xff3b424a), centre.x, body.getY(), juce::Colour (0xff1a1d21), centre.x,
                               body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff0a0c0e));
    g.drawEllipse (body, 1.2f);
    g.setColour (accent.withAlpha (slider.isMouseOverOrDragging() ? 0.9f : 0.55f));
    g.drawEllipse (body.reduced (2.5f), 1.4f);

    const float p0 = bodyR * 0.25f, p1 = bodyR - 4.0f;
    g.setColour (colours::text);
    g.drawLine (centre.x + p0 * std::sin (angle), centre.y - p0 * std::cos (angle), centre.x + p1 * std::sin (angle),
                centre.y - p1 * std::cos (angle), 2.2f);
}

void ConsoleLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                                           float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const auto groove = juce::Rectangle<float> (5.0f, bounds.getHeight()).withCentre (bounds.getCentre());
    g.setColour (colours::well);
    g.fillRoundedRectangle (groove.expanded (1.5f), 3.0f);
    // scale ticks
    g.setColour (colours::panelEdge.brighter (0.3f));
    for (int i = 0; i <= 12; ++i)
    {
        const float ty = bounds.getY() + bounds.getHeight() * float (i) / 12.0f;
        g.drawHorizontalLine (int (ty), groove.getX() - 9.0f, groove.getX() - 3.0f);
        g.drawHorizontalLine (int (ty), groove.getRight() + 3.0f, groove.getRight() + 9.0f);
    }
    // cap
    const auto cap = juce::Rectangle<float> (30.0f, 16.0f).withCentre ({ bounds.getCentreX(), sliderPos });
    juce::ColourGradient grad (juce::Colour (0xff4a525b), cap.getX(), cap.getY(), juce::Colour (0xff252a30), cap.getX(),
                               cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colour (0xff0b0d0f));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.setColour (accent);
    g.fillRect (juce::Rectangle<float> (cap.getWidth() - 8.0f, 2.0f).withCentre (cap.getCentre()));
}

void ConsoleLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    auto face = on ? accent.withAlpha (0.9f) : colours::panel.brighter (over ? 0.18f : 0.08f);
    if (down)
        face = face.darker (0.2f);
    g.setColour (face);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? accent.darker (0.5f) : colours::panelEdge.brighter (0.25f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    // status dot
    const auto dot = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + 9.0f, r.getCentreY() });
    g.setColour (on ? colours::background : colours::well);
    g.fillEllipse (dot);
    g.setColour (on ? colours::background : colours::text);
    g.setFont (juce::Font (uiFont (11.0f, true)));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (14.0f).toNearestInt(), juce::Justification::centred, 1);
}

juce::Font ConsoleLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::consoleg::ui
