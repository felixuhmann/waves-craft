#include "VocalLookAndFeel.h"

namespace craft::vocal::ui
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

void drawReadout (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour edge)
{
    r = r.reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (edge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (12.0f, true)));
    g.drawText (text, r, juce::Justification::centred);
}

VocalLookAndFeel::VocalLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextEditor::backgroundColourId, colours::well);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::highlightColourId, colours::comp.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId, colours::comp);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::comp);
    setColour (juce::CaretComponent::caretColourId, colours::comp);
    setColour (juce::Slider::thumbColourId, colours::comp);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::panelEdge);
}

void VocalLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                                         float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const float w = float (slider.getWidth());
    const float lineY = sliderPos;

    // level line across the whole meter
    g.setColour (accent.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float> (0.0f, lineY - 0.75f, w, 1.5f));

    // cap: a rounded pill with two grip grooves and an accent centre line
    const float capW = std::min (w - 4.0f, 46.0f), capH = 16.0f;
    const auto cap = juce::Rectangle<float> (capW, capH).withCentre ({ w * 0.5f, lineY });
    juce::ColourGradient grad (juce::Colour (0xff505862), 0.0f, cap.getY(), juce::Colour (0xff262b31), 0.0f,
                               cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (slider.isMouseOverOrDragging() ? accent : juce::Colour (0xff0b0d0f));
    g.drawRoundedRectangle (cap, 4.0f, 1.0f);
    g.setColour (juce::Colour (0x30ffffff));
    g.drawHorizontalLine (int (cap.getY() + 3.0f), cap.getX() + 5.0f, cap.getRight() - 5.0f);
    g.drawHorizontalLine (int (cap.getBottom() - 4.0f), cap.getX() + 5.0f, cap.getRight() - 5.0f);
    g.setColour (accent);
    g.fillRect (juce::Rectangle<float> (cap.getWidth() - 10.0f, 2.0f).withCentre (cap.getCentre()));
}

juce::Font VocalLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::vocal::ui
