#include "Controls.h"

#include "LookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::deesser::ui
{

// ---------------------------------------------------------------------------------------------
ValueBox::ValueBox (juce::RangedAudioParameter& p, juce::String id, juce::String u)
    : param (p), paramId (std::move (id)), unit (std::move (u))
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (uiFont (13.0f, true)));
    label.setColour (juce::Label::textColourId, colours::text);
    label.setColour (juce::Label::textWhenEditingColourId, colours::text);
    label.setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    label.setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    label.setInterceptsMouseClicks (false, true);
    label.onTextChange = [this] {
        const float norm = param.getValueForText (label.getText());
        param.beginChangeGesture();
        param.setValueNotifyingHost (norm);
        param.endChangeGesture();
        refresh();
    };
    label.onEditorHide = [this] { refresh(); };
    addAndMakeVisible (label);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Drag or click to type; double-click resets");
    refresh();
}

juce::String ValueBox::currentText() const
{
    auto t = formatValue (paramId, param.convertFrom0to1 (param.getValue()));
    return unit.isEmpty() ? t : t + " " + unit;
}

void ValueBox::refresh()
{
    if (! label.isBeingEdited())
        label.setText (currentText(), juce::dontSendNotification);
}

void ValueBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (isMouseOverOrDragging() ? colours::accent.withAlpha (0.7f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void ValueBox::resized() { label.setBounds (getLocalBounds()); }

void ValueBox::resetToDefault()
{
    param.beginChangeGesture();
    param.setValueNotifyingHost (param.getDefaultValue());
    param.endChangeGesture();
    refresh();
}

void ValueBox::mouseDown (const juce::MouseEvent& e)
{
    stopTimer();
    dragging = false;
    suppressClick = false;
    dragStart = param.getValue();
    if (e.mods.isAltDown())
    {
        resetToDefault();
        suppressClick = true;
    }
}

void ValueBox::mouseDrag (const juce::MouseEvent& e)
{
    if (suppressClick)
        return;
    if (! dragging && e.getDistanceFromDragStart() < 3)
        return;
    if (! dragging)
    {
        dragging = true;
        gesture = true;
        param.beginChangeGesture();
    }
    const float sensitivity = e.mods.isShiftDown() ? 0.0005f : 0.005f; // 200 px for the full range
    const float v = juce::jlimit (0.0f, 1.0f, dragStart - float (e.getDistanceFromDragStartY()) * sensitivity);
    param.setValueNotifyingHost (v);
    refresh();
}

void ValueBox::mouseUp (const juce::MouseEvent&)
{
    if (gesture)
    {
        param.endChangeGesture();
        gesture = false;
    }
    if (! dragging && ! suppressClick)
        startTimer (juce::MouseEvent::getDoubleClickTimeout() + 20); // open the editor unless a double-click follows
    dragging = false;
}

void ValueBox::mouseDoubleClick (const juce::MouseEvent&)
{
    stopTimer();
    resetToDefault();
}

void ValueBox::timerCallback()
{
    stopTimer();
    label.setText (formatValue (paramId, param.convertFrom0to1 (param.getValue())), juce::dontSendNotification);
    label.showEditor();
    if (auto* ed = label.getCurrentTextEditor())
    {
        ed->setJustification (juce::Justification::centred);
        ed->selectAll();
    }
}

// ---------------------------------------------------------------------------------------------
namespace
{
void drawButtonFace (juce::Graphics& g, juce::Rectangle<float> r, bool lit, bool over, juce::Colour litColour)
{
    juce::ColourGradient grad (lit ? litColour.brighter (0.15f) : colours::panel.brighter (over ? 0.18f : 0.08f), r.getX(), r.getY(),
                               lit ? litColour.darker (0.25f) : colours::panel.darker (0.1f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (lit ? litColour.darker (0.5f) : colours::panelEdge.brighter (0.25f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

void setParamGesture (juce::ParameterAttachment& a, float value) { a.setValueAsCompleteGesture (value); }
} // namespace

ModeToggle::ModeToggle (juce::RangedAudioParameter& p, juce::String off, juce::String onT)
    : param (p), offText (std::move (off)), onText (std::move (onT)),
      attachment (p, [this] (float v) { on = v >= 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ModeToggle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    drawButtonFace (g, r, false, isMouseOver(), colours::accent);
    // small two-segment state indicator on the left
    auto ind = r.removeFromLeft (18.0f).reduced (5.0f, 7.0f);
    g.setColour (on ? colours::accent2 : colours::accent);
    g.fillRoundedRectangle (ind.withHeight (ind.getHeight() / 2.0f - 1.0f).withY (on ? ind.getCentreY() + 1.0f : ind.getY()), 1.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (ind.withHeight (ind.getHeight() / 2.0f - 1.0f).withY (on ? ind.getY() : ind.getCentreY() + 1.0f), 1.5f);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (13.0f, true)));
    g.drawText ((on ? onText : offText).toUpperCase(), r.withTrimmedRight (4.0f), juce::Justification::centred);
}

void ModeToggle::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()) && ! e.mods.isPopupMenu())
        setParamGesture (attachment, on ? 0.0f : 1.0f);
}

// ---------------------------------------------------------------------------------------------
FilterSelector::FilterSelector (juce::RangedAudioParameter& p)
    : param (p), attachment (p, [this] (float v) { highPass = v >= 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Sidechain filter: high-pass or band-pass around Freq");
}

void FilterSelector::drawCurveIcon (juce::Graphics& g, juce::Rectangle<float> r, bool bandPass, juce::Colour c)
{
    juce::Path p;
    const int steps = 40;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = float (i) / float (steps);
        // a stylised response: rise around 40 % of the width; band-pass falls again after 70 %
        float y = 1.0f / (1.0f + std::pow (0.4f / std::max (t, 0.001f), 6.0f));
        if (bandPass)
            y *= 1.0f / (1.0f + std::pow (t / 0.72f, 8.0f));
        const juce::Point<float> pt (r.getX() + t * r.getWidth(), r.getBottom() - y * r.getHeight());
        if (i == 0)
            p.startNewSubPath (pt);
        else
            p.lineTo (pt);
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void FilterSelector::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    drawButtonFace (g, r, false, isMouseOver(), colours::accent);
    auto inner = r.reduced (6.0f, 4.0f);
    auto icon = inner.removeFromLeft (30.0f).reduced (0.0f, 2.0f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (icon.expanded (2.0f, 1.0f), 2.0f);
    drawCurveIcon (g, icon.reduced (2.0f, 3.0f), ! highPass, colours::accent2);

    auto arrow = inner.removeFromRight (12.0f);
    juce::Path tri;
    tri.addTriangle (arrow.getCentreX() - 4.0f, arrow.getCentreY() - 2.0f, arrow.getCentreX() + 4.0f, arrow.getCentreY() - 2.0f,
                     arrow.getCentreX(), arrow.getCentreY() + 3.0f);
    g.setColour (colours::textDim);
    g.fillPath (tri);

    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (12.5f, true)));
    g.drawText (highPass ? "HIGH-PASS" : "BAND-PASS", inner, juce::Justification::centred);
}

void FilterSelector::mouseDown (const juce::MouseEvent&)
{
    juce::PopupMenu menu;
    menu.addItem (2, "High-Pass", true, highPass);
    menu.addItem (1, "Band-Pass", true, ! highPass);
    juce::Component::SafePointer<FilterSelector> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                        [safe] (int result) {
                            if (safe != nullptr && result > 0)
                                setParamGesture (safe->attachment, result == 2 ? 1.0f : 0.0f);
                        });
}

// ---------------------------------------------------------------------------------------------
MonitorSelector::MonitorSelector (juce::RangedAudioParameter& p)
    : param (p), attachment (p, [this] (float v) { sidechain = v >= 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Listen to the processed audio or to the filtered sidechain");
}

juce::Rectangle<float> MonitorSelector::buttonArea (int index) const
{
    auto r = getLocalBounds().toFloat();
    const float h = (r.getHeight() - 6.0f) / 2.0f;
    return index == 0 ? r.withHeight (h) : r.withTrimmedTop (h + 6.0f);
}

void MonitorSelector::paint (juce::Graphics& g)
{
    const char* names[2] = { "AUDIO", "SIDECHAIN" };
    for (int i = 0; i < 2; ++i)
    {
        const bool lit = (i == 1) == sidechain;
        const auto r = buttonArea (i).reduced (1.0f);
        drawButtonFace (g, r, lit, isMouseOver(), i == 1 ? colours::warn : colours::accent);
        g.setColour (lit ? colours::background : colours::textDim);
        g.setFont (juce::Font (uiFont (12.5f, true)));
        g.drawText (names[i], r, juce::Justification::centred);
    }
}

void MonitorSelector::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < 2; ++i)
        if (buttonArea (i).contains (e.position))
            setParamGesture (attachment, i == 1 ? 1.0f : 0.0f);
}

} // namespace craft::deesser::ui
