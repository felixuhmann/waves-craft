#include "Controls.h"

#include "EnhancerLookAndFeel.h"

namespace craft::enhancer::ui
{

ValueBox::ValueBox (juce::RangedAudioParameter& p) : param (p)
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (uiFont (12.0f, true)));
    label.setColour (juce::Label::textWhenEditingColourId, colours::text);
    label.setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    label.setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    label.setMinimumHorizontalScale (0.7f);
    label.setInterceptsMouseClicks (false, true);
    label.onTextChange = [this] {
        param.beginChangeGesture();
        setNormalised (param.getValueForText (label.getText()));
        param.endChangeGesture();
        refresh();
    };
    label.onEditorHide = [this] { refresh(); };
    addAndMakeVisible (label);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    refresh();
}

void ValueBox::setNormalised (float v) { param.setValueNotifyingHost (limit ? limit (v) : v); }

void ValueBox::refresh()
{
    if (! label.isBeingEdited())
    {
        const auto t = param.getCurrentValueAsText();
        if (t != label.getText())
            label.setText (t, juce::dontSendNotification);
    }
}

void ValueBox::enablementChanged()
{
    label.setColour (juce::Label::textColourId, isEnabled() ? colours::text : colours::textDim.withAlpha (0.6f));
    repaint();
}

void ValueBox::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (isEnabled() && isMouseOverOrDragging() ? colours::accent.withAlpha (0.7f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void ValueBox::resized() { label.setBounds (getLocalBounds()); }

void ValueBox::mouseDown (const juce::MouseEvent&)
{
    stopTimer();
    dragging = false;
    dragStart = param.getValue();
}

void ValueBox::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() < 3)
        return;
    if (! dragging)
    {
        dragging = true;
        param.beginChangeGesture();
    }
    const float sensitivity = e.mods.isShiftDown() ? 0.0005f : 0.004f;
    setNormalised (juce::jlimit (0.0f, 1.0f, dragStart - float (e.getDistanceFromDragStartY()) * sensitivity));
    refresh();
}

void ValueBox::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        param.endChangeGesture();
    else
        startTimer (juce::MouseEvent::getDoubleClickTimeout() + 20);
    dragging = false;
}

void ValueBox::mouseDoubleClick (const juce::MouseEvent&)
{
    stopTimer();
    param.beginChangeGesture();
    setNormalised (param.getDefaultValue());
    param.endChangeGesture();
    refresh();
}

void ValueBox::timerCallback()
{
    stopTimer();
    label.showEditor();
    if (auto* ed = label.getCurrentTextEditor())
    {
        ed->setJustification (juce::Justification::centred);
        ed->selectAll();
    }
}

BoundSlider::BoundSlider (APVTS& st, const juce::String& id, juce::Slider::SliderStyle style, juce::Colour accent)
    : juce::Slider (style, juce::Slider::NoTextBox), attachment (st, id, *this)
{
    setColour (juce::Slider::thumbColourId, accent);
    if (style == juce::Slider::RotaryHorizontalVerticalDrag)
    {
        setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        setMouseDragSensitivity (220);
    }
    if (auto* p = st.getParameter (id))
        setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

BoundToggle::BoundToggle (APVTS& st, const juce::String& id, const juce::String& text, juce::Colour onColour)
    : juce::TextButton (text), attachment (st, id, *this)
{
    setClickingTogglesState (true);
    setColour (juce::TextButton::buttonOnColourId, onColour);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setTooltip (st.getParameter (id) != nullptr ? st.getParameter (id)->getName (64) : juce::String());
}

} // namespace craft::enhancer::ui
