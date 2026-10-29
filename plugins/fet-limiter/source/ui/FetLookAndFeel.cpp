#include "FetLookAndFeel.h"

namespace craft::fet_limiter::ui
{

juce::FontOptions uiFont (float height, bool bold)
{
    return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
}

FetLookAndFeel::FetLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextEditor::backgroundColourId, colours::well);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::panel);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setColour (juce::TooltipWindow::backgroundColourId, colours::well);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::panelEdge);
    setAccent (colours::themeA);
}

void FetLookAndFeel::setAccent (juce::Colour c)
{
    accent = c;
    setColour (juce::TextEditor::highlightColourId, c.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId, c);
    setColour (juce::TextEditor::focusedOutlineColourId, c);
    setColour (juce::CaretComponent::caretColourId, c);
}

void FetLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = b.getToggleState() ? accent : colours::well.brighter (over ? 0.12f : 0.0f);
    if (down)
        fill = fill.darker (0.2f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (over ? accent.withAlpha (0.7f) : colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void FetLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? colours::background : colours::text);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1, 0.8f);
}

juce::Font FetLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (uiFont (std::min (12.5f, float (buttonHeight) * 0.55f), true));
}

juce::Font FetLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour tick)
{
    const auto rf = r.toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (rf, 7.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (rf.reduced (0.5f), 7.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (tick);
        g.fillRoundedRectangle (rf.getX() + 12.0f, rf.getY() + 12.0f, 3.0f, 11.0f, 1.5f);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (11.5f, true)));
        g.drawText (title.toUpperCase(), r.getX() + 21, r.getY() + 9, r.getWidth() - 30, 17, juce::Justification::centredLeft);
    }
}

} // namespace craft::fet_limiter::ui
