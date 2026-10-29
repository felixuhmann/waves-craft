#include "DistortionLookAndFeel.h"

namespace craft::ddp::ui
{

juce::FontOptions uiFont (float height, bool bold)
{
    return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
}

void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& subtitle,
                  juce::Colour tick)
{
    auto rf = r.toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (rf, 8.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (rf.reduced (0.5f), 8.0f, 1.0f);
    auto head = r.reduced (12, 8).removeFromTop (14);
    g.setColour (tick);
    g.fillRoundedRectangle (head.removeFromLeft (3).toFloat(), 1.5f);
    head.removeFromLeft (7);
    g.setColour (colours::text.withAlpha (0.85f));
    g.setFont (juce::Font (uiFont (11.0f, true)).withExtraKerningFactor (0.14f));
    const auto t = title.toUpperCase();
    g.drawText (t, head, juce::Justification::centredLeft);
    if (subtitle.isNotEmpty())
    {
        const int w = juce::GlyphArrangement::getStringWidthInt (juce::Font (uiFont (11.0f, true)).withExtraKerningFactor (0.14f), t);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (10.5f)));
        g.drawText (subtitle, head.withTrimmedLeft (w + 10), juce::Justification::centredLeft);
    }
}

DistortionLookAndFeel::DistortionLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::Label::textWhenEditingColourId, colours::text);
    setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    setColour (juce::Label::outlineWhenEditingColourId, colours::heat);
    setColour (juce::TextEditor::backgroundColourId, colours::well);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::highlightColourId, colours::heat.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId, colours::heat);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::heat);
    setColour (juce::CaretComponent::caretColourId, colours::heat);
    setColour (juce::TextButton::buttonColourId, colours::panel);
    setColour (juce::TextButton::buttonOnColourId, colours::heat);
    setColour (juce::TextButton::textColourOffId, colours::textDim);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setColour (juce::Slider::thumbColourId, colours::heat);
    setColour (juce::TooltipWindow::backgroundColourId, colours::well);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::panelEdge);
}

void DistortionLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float a0,
                                              float a1, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height)).reduced (3.0f);
    const float size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float radius = size / 2.0f;
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float angle = a0 + pos * (a1 - a0);
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;

    // tick ring: short radial marks, lit up to the value
    const int ticks = 31;
    const float from = bipolar ? a0 + float (slider.valueToProportionOfLength (0.0)) * (a1 - a0) : a0;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = a0 + (a1 - a0) * float (i) / float (ticks - 1);
        const bool lit = a >= std::min (from, angle) - 1.0e-3f && a <= std::max (from, angle) + 1.0e-3f;
        const float r0 = radius - (i % 5 == 0 ? 6.0f : 4.5f), r1 = radius;
        g.setColour (lit ? accent.withAlpha (slider.isEnabled() ? 1.0f : 0.4f) : colours::grid.brighter (0.25f));
        const auto p0 = c.getPointOnCircumference (r0, a), p1 = c.getPointOnCircumference (r1, a);
        g.drawLine ({ p0, p1 }, lit ? 2.0f : 1.4f);
    }

    // body: faceted cap
    const float capR = radius - 9.0f;
    const auto cap = juce::Rectangle<float> (2 * capR, 2 * capR).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (cap.translated (0.0f, 2.0f).expanded (1.0f));
    juce::ColourGradient grad (juce::Colour (0xff3a3d44), c.x - capR * 0.5f, c.y - capR, juce::Colour (0xff1a1c20), c.x + capR * 0.4f,
                               c.y + capR, false);
    g.setGradientFill (grad);
    g.fillEllipse (cap);
    g.setColour (juce::Colour (0xff4a4e57));
    g.drawEllipse (cap.reduced (0.5f), 1.0f);
    g.setColour (colours::well.withAlpha (0.7f));
    g.drawEllipse (cap.reduced (capR * 0.28f), 1.0f);

    // pointer: glowing dot near the rim of the cap
    const auto dot = c.getPointOnCircumference (capR * 0.72f, angle);
    const float dr = std::max (2.2f, capR * 0.11f);
    g.setColour (accent.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (dr * 4.0f, dr * 4.0f).withCentre (dot));
    g.setColour (accent.brighter (0.3f));
    g.fillEllipse (juce::Rectangle<float> (dr * 2.0f, dr * 2.0f).withCentre (dot));
}

void DistortionLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : colours::panel.brighter (over ? 0.18f : 0.07f);
    if (down)
        fill = fill.darker (0.2f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge.brighter (0.25f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

juce::Font DistortionLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (uiFont (std::min (12.0f, float (buttonHeight) * 0.55f), true));
}

juce::Font DistortionLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::ddp::ui
