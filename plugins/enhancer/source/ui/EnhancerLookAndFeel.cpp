#include "EnhancerLookAndFeel.h"

namespace craft::enhancer::ui
{

juce::Colour bandColour (int band)
{
    static const juce::Colour c[5] = { juce::Colour (0xffe07a5f), juce::Colour (0xffe3a447), juce::Colour (0xff8cc56b),
                                       juce::Colour (0xff58c4b6), juce::Colour (0xff7fb7ef) };
    return c[juce::jlimit (0, 4, band)];
}

juce::FontOptions uiFont (float height, bool bold) { return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain); }

void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour tick)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r.toFloat(), 7.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 7.0f, 1.0f);
    if (title.isNotEmpty())
    {
        auto head = r.reduced (10, 6).removeFromTop (14);
        g.setColour (tick);
        g.fillRoundedRectangle (head.removeFromLeft (3).toFloat(), 1.5f);
        head.removeFromLeft (6);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (11.0f, true)).withExtraKerningFactor (0.12f));
        g.drawText (title.toUpperCase(), head, juce::Justification::centredLeft);
    }
}

EnhancerLookAndFeel::EnhancerLookAndFeel()
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

void EnhancerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float a0, float a1, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height)).reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto c = bounds.getCentre();
    const float ring = juce::jmax (3.0f, radius * 0.16f);
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float angle = a0 + pos * (a1 - a0);

    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - ring / 2, radius - ring / 2, 0.0f, a0, a1, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = slider.getProperties().contains ("centreDetent") ? a0 + 0.5f * (a1 - a0) : a0;
    juce::Path value;
    value.addCentredArc (c.x, c.y, radius - ring / 2, radius - ring / 2, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (accent.withAlpha (slider.isEnabled() ? 0.95f : 0.35f));
    g.strokePath (value, juce::PathStrokeType (ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float capR = radius - ring - 3.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d444c), c.x, c.y - capR, juce::Colour (0xff1f2328), c.x, c.y + capR, false));
    g.fillEllipse (juce::Rectangle<float> (2 * capR, 2 * capR).withCentre (c));
    juce::Path p;
    const float pw = juce::jmax (2.0f, capR * 0.14f);
    p.addRoundedRectangle (-pw / 2, -capR + 2.0f, pw, capR * 0.55f, pw / 2);
    g.setColour (colours::text);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void EnhancerLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                                            juce::Slider::SliderStyle, juce::Slider& slider)
{
    const auto b = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float t = 5.0f;
    const auto groove = juce::Rectangle<float> (b.getCentreX() - t / 2, b.getY(), t, b.getHeight());
    g.setColour (colours::well);
    g.fillRoundedRectangle (groove.expanded (1.0f), 3.0f);
    g.setColour (accent.withAlpha (slider.isEnabled() ? 0.85f : 0.3f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (groove.getX(), sliderPos, groove.getRight(), b.getBottom()));
    g.setColour (colours::text);
    g.fillRoundedRectangle (juce::Rectangle<float> (18.0f, 8.0f).withCentre ({ b.getCentreX(), sliderPos }), 2.0f);
}

void EnhancerLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : colours::panel.brighter (over ? 0.18f : 0.07f);
    if (down)
        fill = fill.darker (0.2f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge.brighter (0.25f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

juce::Font EnhancerLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (uiFont (juce::jmin (11.5f, float (buttonHeight) * 0.58f), true));
}

juce::Font EnhancerLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

} // namespace craft::enhancer::ui
