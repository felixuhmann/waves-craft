#include "MotionLookAndFeel.h"

namespace craft::mfp::ui
{

juce::Colour sourceColour (int source)
{
    switch (source)
    {
        case 0: return colours::lfo;
        case 1: return colours::seq;
        default: return colours::follower;
    }
}

juce::FontOptions uiFont (float height, bool bold)
{
    return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
}

void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour tick)
{
    auto rf = r.toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (rf, 7.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (rf.reduced (0.5f), 7.0f, 1.0f);
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

MotionLookAndFeel::MotionLookAndFeel()
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
    setColour (juce::Slider::textBoxOutlineColourId, colours::accent);
    setColour (juce::BubbleComponent::backgroundColourId, colours::well);
    setColour (juce::BubbleComponent::outlineColourId, colours::panelEdge);
    setColour (juce::TooltipWindow::backgroundColourId, colours::well);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::panelEdge);
}

void MotionLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float a0, float a1,
                                          juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (float (x), float (y), float (width), float (height)).reduced (2.0f);
    const float size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float radius = size / 2.0f;
    const float ring = std::max (3.0f, radius * 0.16f);
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float angle = a0 + pos * (a1 - a0);

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - ring / 2, radius - ring / 2, 0.0f, a0, a1, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc: from the centre for bipolar ranges, else from the start
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar ? a0 + float (slider.valueToProportionOfLength (0.0)) * (a1 - a0) : a0;
    juce::Path value;
    value.addCentredArc (c.x, c.y, radius - ring / 2, radius - ring / 2, 0.0f, std::min (from, angle), std::max (from, angle), true);
    g.setColour (accent.withAlpha (slider.isEnabled() ? 0.95f : 0.35f));
    g.strokePath (value, juce::PathStrokeType (ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // cap
    const float capR = radius - ring - 3.0f;
    juce::ColourGradient grad (juce::Colour (0xff3d444c), c.x, c.y - capR, juce::Colour (0xff1f2328), c.x, c.y + capR, false);
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (2 * capR, 2 * capR).withCentre (c));
    g.setColour (juce::Colour (0xff0a0c0e));
    g.drawEllipse (juce::Rectangle<float> (2 * capR, 2 * capR).withCentre (c), 1.0f);

    // pointer
    juce::Path p;
    const float pw = std::max (2.0f, capR * 0.14f);
    p.addRoundedRectangle (-pw / 2, -capR + 2.0f, pw, capR * 0.55f, pw / 2);
    g.setColour (colours::text);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void MotionLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                                          juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const bool vertical = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;
    const auto b = juce::Rectangle<float> (float (x), float (y), float (width), float (height));
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float t = 4.0f;
    auto groove = vertical ? juce::Rectangle<float> (b.getCentreX() - t / 2, b.getY(), t, b.getHeight())
                           : juce::Rectangle<float> (b.getX(), b.getCentreY() - t / 2, b.getWidth(), t);
    g.setColour (colours::well);
    g.fillRoundedRectangle (groove.expanded (1.0f), 2.5f);

    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zero = float (slider.getPositionOfValue (bipolar ? 0.0 : slider.getMinimum()));
    g.setColour (accent.withAlpha (0.85f));
    if (vertical)
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (groove.getX(), std::min (zero, sliderPos), groove.getRight(),
                                                                 std::max (zero, sliderPos)));
    else
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (std::min (zero, sliderPos), groove.getY(),
                                                                 std::max (zero, sliderPos), groove.getBottom()));
    if (bipolar)
    {
        g.setColour (colours::textDim.withAlpha (0.6f));
        if (vertical)
            g.fillRect (juce::Rectangle<float> (t + 6, 1.0f).withCentre ({ b.getCentreX(), zero }));
        else
            g.fillRect (juce::Rectangle<float> (1.0f, t + 6).withCentre ({ zero, b.getCentreY() }));
    }
    // thumb
    const auto thumb = vertical ? juce::Rectangle<float> (12.0f, 6.0f).withCentre ({ b.getCentreX(), sliderPos })
                                : juce::Rectangle<float> (6.0f, 12.0f).withCentre ({ sliderPos, b.getCentreY() });
    g.setColour (colours::text);
    g.fillRoundedRectangle (thumb, 2.0f);
}

void MotionLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto on = b.findColour (juce::TextButton::buttonOnColourId);
    auto fill = b.getToggleState() ? on : colours::panel.brighter (over ? 0.18f : 0.07f);
    if (down)
        fill = fill.darker (0.2f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge.brighter (0.25f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

juce::Font MotionLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (uiFont (std::min (12.0f, buttonHeight * 0.58f), true));
}

juce::Font MotionLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::mfp::ui
