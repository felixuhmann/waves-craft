#include "HDelayLookAndFeel.h"

namespace craft::hdelay::ui
{

juce::FontOptions uiFont (float height, bool bold)
{
    return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, float corner)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, corner);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (11.0f, true)));
        g.drawText (title.toUpperCase(), r.reduced (10.0f, 4.0f).removeFromTop (16.0f), juce::Justification::centredLeft);
    }
}

HDelayLookAndFeel::HDelayLookAndFeel()
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
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxBackgroundColourId, colours::well);
    setColour (juce::Slider::textBoxOutlineColourId, colours::panelEdge);
    setColour (juce::Slider::textBoxHighlightColourId, colours::accent.withAlpha (0.4f));
    setColour (juce::TooltipWindow::backgroundColourId, colours::well);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::panelEdge);
}

void HDelayLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    const float size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float radius = size * 0.5f - 2.0f;
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float trackW = std::max (3.0f, radius * 0.10f);

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - trackW * 0.5f, radius - trackW * 0.5f, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc: from the start, or from the centre for bipolar parameters
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const bool fromEnd = slider.getProperties().getWithDefault ("arcFromEnd", false);
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : (fromEnd ? endAngle : startAngle);
    if (std::abs (angle - from) > 0.001f && slider.isEnabled())
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius - trackW * 0.5f, radius - trackW * 0.5f, 0.0f,
                             std::min (from, angle), std::max (from, angle), true);
        g.setColour (accent);
        g.strokePath (value, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body
    const float bodyR = radius - trackW - std::max (3.0f, radius * 0.08f);
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (body.translated (0.0f, 2.0f).expanded (1.0f));
    juce::ColourGradient grad (juce::Colour (0xff3b424a), body.getX(), body.getY(), juce::Colour (0xff1b1f24), body.getX(),
                               body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff4d555e));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    // fine grip ring
    g.setColour (juce::Colour (0xff0b0d0f).withAlpha (0.6f));
    g.drawEllipse (body.reduced (bodyR * 0.18f), 1.0f);

    // pointer
    const float pIn = bodyR * 0.30f, pOut = bodyR * 0.88f;
    const juce::Point<float> a (centre.x + pIn * std::sin (angle), centre.y - pIn * std::cos (angle));
    const juce::Point<float> b (centre.x + pOut * std::sin (angle), centre.y - pOut * std::cos (angle));
    g.setColour (slider.isEnabled() ? accent : colours::textDim);
    g.drawLine ({ a, b }, std::max (2.0f, bodyR * 0.09f));
}

juce::Label* HDelayLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (juce::Font (uiFont (12.5f, true)));
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, colours::text);
    l->setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    l->setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    l->setColour (juce::Label::textWhenEditingColourId, colours::text);
    return l;
}

void HDelayLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const auto onColour = b.isColourSpecified (juce::TextButton::buttonOnColourId)
                              ? b.findColour (juce::TextButton::buttonOnColourId)
                              : colours::accent;
    auto fill = b.getToggleState() ? onColour : colours::well.brighter (over ? 0.25f : 0.12f);
    if (down)
        fill = fill.darker (0.2f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (b.getToggleState() ? onColour.brighter (0.2f) : colours::panelEdge.brighter (over ? 0.5f : 0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

juce::Font HDelayLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (uiFont (std::min (11.5f, buttonHeight * 0.5f), true));
}

} // namespace craft::hdelay::ui
