#include "LimiterLookAndFeel.h"

namespace craft::mastering_limiter::ui
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

void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, juce::Justification j)
{
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.5f, true)));
    g.drawFittedText (text.toUpperCase(), r, j, 1);
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
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setColour (juce::TooltipWindow::backgroundColourId, colours::well);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::panelEdge);
}

void CraftLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
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

void CraftLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    auto col = b.getToggleState() ? colours::background : colours::text;
    if (! b.isEnabled())
        col = col.withMultipliedAlpha (0.4f);
    g.setColour (col);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1, 0.8f);
}

juce::Font CraftLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (uiFont (std::min (12.5f, buttonHeight * 0.55f), true));
}

juce::Font CraftLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace craft::mastering_limiter::ui
