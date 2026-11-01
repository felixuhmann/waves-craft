#include "OptoLookAndFeel.h"

namespace craft::opto_leveler::ui
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
        g.drawText (title.toUpperCase(), r.reduced (12.0f, 6.0f).removeFromTop (16.0f), juce::Justification::centredLeft);
    }
}

OptoLookAndFeel::OptoLookAndFeel()
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

void OptoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    const float size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float radius = size * 0.5f - 2.0f;
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float trackW = std::max (3.0f, radius * 0.085f);
    const float trackR = radius - trackW * 0.5f;

    // track and value arc (bipolar controls start in the centre)
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f && slider.isEnabled())
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (accent);
        g.strokePath (value, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body: a flat disc with a soft inner ring (the large knobs get a lit rim)
    const float bodyR = radius - trackW - std::max (4.0f, radius * 0.10f);
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (body.translated (0.0f, 2.5f).expanded (1.5f));
    juce::ColourGradient grad (juce::Colour (0xff353c44), centre.x, body.getY(), juce::Colour (0xff1a1e23), centre.x,
                               body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff4a525b));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    if (bodyR > 30.0f)
    {
        g.setColour (accent.withAlpha (0.10f));
        g.drawEllipse (body.reduced (bodyR * 0.16f), 2.0f);
    }

    // pointer: a short bar from the rim inwards plus a dot
    const float pIn = bodyR * 0.45f, pOut = bodyR * 0.90f;
    const juce::Point<float> a (centre.x + pIn * std::sin (angle), centre.y - pIn * std::cos (angle));
    const juce::Point<float> b (centre.x + pOut * std::sin (angle), centre.y - pOut * std::cos (angle));
    g.setColour (slider.isEnabled() ? accent : colours::textDim);
    g.drawLine ({ a, b }, std::max (2.0f, bodyR * 0.075f));
    g.fillEllipse (juce::Rectangle<float> (bodyR * 0.12f, bodyR * 0.12f).withCentre (centre));
}

juce::Label* OptoLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (juce::Font (uiFont (12.5f, true)));
    l->setColour (juce::Label::backgroundColourId, colours::well);
    l->setColour (juce::Label::outlineColourId, colours::panelEdge);
    l->setColour (juce::Label::textColourId, colours::text);
    l->setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    l->setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    l->setColour (juce::Label::textWhenEditingColourId, colours::text);
    return l;
}

void OptoLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = b.getToggleState() ? colours::accent : colours::well.brighter (over ? 0.12f : 0.0f);
    if (down)
        fill = fill.darker (0.2f);
    if (! b.isEnabled())
        fill = fill.withMultipliedAlpha (0.4f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (over ? colours::accent.withAlpha (0.7f) : colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void OptoLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    auto col = b.getToggleState() ? colours::background : colours::text;
    if (! b.isEnabled())
        col = col.withMultipliedAlpha (0.4f);
    g.setColour (col);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1, 0.8f);
}

juce::Font OptoLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (uiFont (std::min (12.0f, buttonHeight * 0.5f), true));
}

} // namespace craft::opto_leveler::ui
