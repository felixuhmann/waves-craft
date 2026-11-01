#include "LookAndFeel.h"

namespace craft::consolee::ui
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
        auto strip = r.removeFromTop (24.0f).reduced (10.0f, 0.0f);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (11.5f, true)));
        g.drawText (title, strip, juce::Justification::centredLeft);
        g.setColour (colours::panelEdge);
        g.fillRect (strip.withY (strip.getBottom() - 1.0f).withHeight (1.0f));
    }
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
    setColour (juce::TextButton::textColourOffId, colours::textDim);
    setColour (juce::TextButton::textColourOnId, colours::text);
    setColour (juce::Slider::thumbColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.3f));
}

void CraftLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float startAngle,
                                         float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height)).reduced (2.0f);
    const float size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float radius = size * 0.5f;
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const bool off = slider.getProperties().getWithDefault ("offAtZero", false) && slider.getValue() <= 0.0;

    // value track: background arc, then the active arc from the centre (bipolar) or the start
    const float trackR = radius - 2.0f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = slider.getProperties().getWithDefault ("bipolar", false);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float from = bipolar ? 0.5f * (startAngle + endAngle) : startAngle;
    if (! off && std::abs (angle - from) > 0.01f)
    {
        juce::Path active;
        active.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (accent);
        g.strokePath (active, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body: a plain disc with a soft rim, and a short pointer line
    const float bodyR = radius - 7.0f;
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4048), centre.x, body.getY(), juce::Colour (0xff23272c), centre.x,
                                             body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff0b0d0f));
    g.drawEllipse (body, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (body.reduced (2.0f), 1.0f);

    const juce::Point<float> tip (centre.x + (bodyR - 3.0f) * std::sin (angle), centre.y - (bodyR - 3.0f) * std::cos (angle));
    const juce::Point<float> base (centre.x + bodyR * 0.25f * std::sin (angle), centre.y - bodyR * 0.25f * std::cos (angle));
    g.setColour (off ? colours::textDim : colours::text);
    g.drawLine ({ base, tip }, 2.2f);
}

void CraftLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                                         juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const bool vertical = style == juce::Slider::LinearVertical;
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    const auto accent = slider.findColour (juce::Slider::thumbColourId);

    const float grooveThickness = 5.0f;
    juce::Rectangle<float> groove = vertical
        ? juce::Rectangle<float> (bounds.getCentreX() - grooveThickness / 2, bounds.getY(), grooveThickness, bounds.getHeight())
        : juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - grooveThickness / 2, bounds.getWidth(), grooveThickness);
    g.setColour (colours::well);
    g.fillRoundedRectangle (groove.expanded (1.5f), 3.0f);

    // the filled part starts at the default (unity) position
    const double def = slider.getDoubleClickReturnValue();
    const float defPos = float (slider.getPositionOfValue (def));
    g.setColour (accent.withAlpha (0.55f));
    if (vertical)
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (groove.getX(), std::min (defPos, sliderPos), groove.getRight(),
                                                                 std::max (defPos, sliderPos)));
    else
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (std::min (defPos, sliderPos), groove.getY(), std::max (defPos, sliderPos),
                                                                 groove.getBottom()));

    juce::Rectangle<float> cap = vertical ? juce::Rectangle<float> (34.0f, 18.0f).withCentre ({ bounds.getCentreX(), sliderPos })
                                          : juce::Rectangle<float> (14.0f, 26.0f).withCentre ({ sliderPos, bounds.getCentreY() });
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a525b), cap.getX(), cap.getY(), juce::Colour (0xff2a2f35), cap.getX(),
                                             cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colour (0xff0b0d0f));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.setColour (accent);
    if (vertical)
        g.fillRect (juce::Rectangle<float> (cap.getWidth() - 10.0f, 2.0f).withCentre (cap.getCentre()));
    else
        g.fillRect (juce::Rectangle<float> (2.0f, cap.getHeight() - 10.0f).withCentre (cap.getCentre()));
}

void CraftLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool lit = b.getToggleState();
    const auto litColour = b.getProperties().getWithDefault ("warn", false) ? colours::warn : colours::accent;
    auto fill = colours::panel.brighter (over ? 0.18f : 0.08f);
    if (down)
        fill = fill.darker (0.2f);
    g.setGradientFill (juce::ColourGradient (fill, r.getX(), r.getY(), fill.darker (0.25f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (lit ? litColour.withAlpha (0.8f) : colours::panelEdge.brighter (0.25f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    // indicator dot on the left
    const auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 10.0f, r.getCentreY() });
    if (lit)
    {
        g.setColour (litColour.withAlpha (0.3f));
        g.fillEllipse (led.expanded (2.5f));
    }
    g.setColour (lit ? litColour : colours::well);
    g.fillEllipse (led);
}

void CraftLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? colours::text : colours::textDim);
    g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (16).withTrimmedRight (2), juce::Justification::centred);
}

juce::Font CraftLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (uiFont (std::min (11.5f, buttonHeight * 0.55f), true));
}

juce::Font CraftLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::consolee::ui
