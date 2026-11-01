#include "Controls.h"

#include "LimiterLookAndFeel.h"

namespace craft::mastering_limiter::ui
{

ValueBox::ValueBox (juce::RangedAudioParameter& p, std::function<juce::String()> fn) : param (p), textFn (std::move (fn))
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
        setNormalised (norm);
        param.endChangeGesture();
        refresh();
    };
    label.onEditorHide = [this] { refresh(); };
    addAndMakeVisible (label);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    refresh();
}

void ValueBox::setNormalised (float v)
{
    if (onUserValue)
        onUserValue (v);
    else
        param.setValueNotifyingHost (v);
}

void ValueBox::refresh()
{
    if (! label.isBeingEdited())
    {
        const auto t = textFn();
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
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (isEnabled() && isMouseOverOrDragging() ? colours::accent.withAlpha (0.7f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void ValueBox::resized() { label.setBounds (getLocalBounds()); }

void ValueBox::mouseDown (const juce::MouseEvent& e)
{
    stopTimer();
    dragging = false;
    suppressClick = false;
    dragStart = param.getValue();
    if (e.mods.isAltDown())
    {
        param.beginChangeGesture();
        setNormalised (param.getDefaultValue());
        param.endChangeGesture();
        suppressClick = true;
        refresh();
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
    const float sensitivity = e.mods.isShiftDown() ? 0.0005f : 0.004f;
    setNormalised (juce::jlimit (0.0f, 1.0f, dragStart - float (e.getDistanceFromDragStartY()) * sensitivity));
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
    param.beginChangeGesture();
    setNormalised (param.getDefaultValue());
    param.endChangeGesture();
    refresh();
}

void ValueBox::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float delta = (w.isReversed ? -w.deltaY : w.deltaY) * 0.03f;
    param.beginChangeGesture();
    setNormalised (juce::jlimit (0.0f, 1.0f, param.getValue() + delta));
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

// ---------------------------------------------------------------------------------------------
SegmentedChoice::SegmentedChoice (juce::RangedAudioParameter& p, const juce::StringArray& l)
    : param (p), labels (l), attachment (p, [this] (float v) {
          selected = juce::roundToInt (v);
          repaint();
      })
{
    attachment.sendInitialUpdate();
}

SegmentedChoice::~SegmentedChoice() = default;

juce::Rectangle<float> SegmentedChoice::segment (int i) const
{
    const auto r = getLocalBounds().toFloat();
    const float w = r.getWidth() / float (labels.size());
    return { r.getX() + w * float (i), r.getY(), w, r.getHeight() };
}

int SegmentedChoice::indexAt (juce::Point<float> p) const
{
    for (int i = 0; i < labels.size(); ++i)
        if (segment (i).contains (p))
            return i;
    return -1;
}

void SegmentedChoice::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    for (int i = 0; i < labels.size(); ++i)
    {
        auto s = segment (i).reduced (2.0f);
        if (i == selected)
        {
            g.setColour (colours::accent);
            g.fillRoundedRectangle (s, 3.0f);
        }
        else if (i == hover)
        {
            g.setColour (colours::panel.brighter (0.15f));
            g.fillRoundedRectangle (s, 3.0f);
        }
        g.setColour (i == selected ? colours::background : colours::text);
        g.setFont (juce::Font (uiFont (11.5f, true)));
        g.drawFittedText (labels[i], s.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        if (i > 0 && i != selected && i - 1 != selected)
        {
            g.setColour (colours::panelEdge);
            const float x = segment (i).getX();
            g.drawVerticalLine (int (x), r.getY() + 5.0f, r.getBottom() - 5.0f);
        }
    }
    g.setColour (colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void SegmentedChoice::mouseDown (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i >= 0 && i != selected)
        attachment.setValueAsCompleteGesture (float (i));
}

void SegmentedChoice::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void SegmentedChoice::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i != hover)
    {
        hover = i;
        repaint();
    }
}

void SegmentedChoice::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

} // namespace craft::mastering_limiter::ui
