#include "Controls.h"

#include "DynamicsLookAndFeel.h"

namespace craft::dynamics::ui
{

// ---------------------------------------------------------------------------------------------
ValueBox::ValueBox (juce::RangedAudioParameter& p, juce::Colour a) : param (p), accent (a)
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (uiFont (12.5f, true)));
    label.setColour (juce::Label::textColourId, colours::text);
    label.setColour (juce::Label::textWhenEditingColourId, colours::text);
    label.setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    label.setColour (juce::Label::outlineWhenEditingColourId, accent);
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
    refresh();
}

void ValueBox::refresh()
{
    if (! label.isBeingEdited())
    {
        const auto t = param.getCurrentValueAsText();
        if (t != label.getText())
            label.setText (t, juce::dontSendNotification);
    }
}

void ValueBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (isMouseOverOrDragging() ? accent.withAlpha (0.8f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    // a thin bar along the bottom shows the normalised position
    const float v = param.getValue();
    g.setColour (accent.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float> (r.getX() + 2.0f, r.getBottom() - 3.0f, (r.getWidth() - 4.0f) * v, 1.5f));
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
    if (suppressClick || (! dragging && e.getDistanceFromDragStart() < 3))
        return;
    if (! dragging)
    {
        dragging = true;
        gesture = true;
        param.beginChangeGesture();
    }
    const float sensitivity = e.mods.isShiftDown() ? 0.0005f : 0.004f;
    param.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, dragStart - float (e.getDistanceFromDragStartY()) * sensitivity));
    refresh();
    repaint();
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

void ValueBox::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float delta = (w.isReversed ? -w.deltaY : w.deltaY) * 0.04f;
    param.beginChangeGesture();
    param.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, param.getValue() + delta));
    param.endChangeGesture();
    refresh();
    repaint();
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
SegmentedChoice::SegmentedChoice (juce::RangedAudioParameter& p, juce::StringArray l, juce::Colour a)
    : param (p), labels (std::move (l)), accent (a), attachment (p, [this] (float) { repaint(); })
{
    attachment.sendInitialUpdate();
}

int SegmentedChoice::current() const
{
    const int n = labels.size();
    return juce::jlimit (0, n - 1, juce::roundToInt (param.getValue() * float (n - 1)));
}

int SegmentedChoice::indexAt (juce::Point<float> p) const
{
    const int n = labels.size();
    return juce::jlimit (0, n - 1, int (p.x / float (getWidth()) * float (n)));
}

void SegmentedChoice::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    const int n = labels.size(), sel = current();
    const float w = r.getWidth() / float (n);
    g.setFont (juce::Font (uiFont (11.0f, true)));
    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float> (r.getX() + w * float (i), r.getY(), w, r.getHeight()).reduced (2.0f);
        if (i == sel)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (seg, 3.0f);
        }
        else if (i == hover)
        {
            g.setColour (colours::panel.brighter (0.15f));
            g.fillRoundedRectangle (seg, 3.0f);
        }
        g.setColour (i == sel ? colours::background : colours::text);
        g.drawFittedText (labels[i], seg.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }
}

void SegmentedChoice::mouseDown (const juce::MouseEvent& e)
{
    const int n = labels.size();
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (float (indexAt (e.position)) / float (juce::jmax (1, n - 1))));
}

void SegmentedChoice::mouseMove (const juce::MouseEvent& e)
{
    const int h = indexAt (e.position);
    if (h != hover)
    {
        hover = h;
        repaint();
    }
}

void SegmentedChoice::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

// ---------------------------------------------------------------------------------------------
ParamToggle::ParamToggle (juce::RangedAudioParameter& p, juce::String t, juce::Colour c, Icon i)
    : param (p), text (std::move (t)), onColour (c), icon (i), attachment (p, [this] (float) { repaint(); })
{
    attachment.sendInitialUpdate();
}

void ParamToggle::mouseDown (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (isOn() ? 0.0f : 1.0f);
}

void ParamToggle::paint (juce::Graphics& g)
{
    const bool on = isOn();
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (on ? onColour : colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (isMouseOver() ? onColour : colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    const auto ink = on ? colours::background : colours::text;
    g.setColour (ink);

    auto content = r.reduced (4.0f);
    if (icon != Icon::None)
    {
        auto ic = content.removeFromLeft (content.getHeight()).withSizeKeepingCentre (content.getHeight(), content.getHeight());
        const float s = ic.getWidth();
        const auto c = ic.getCentre();
        if (icon == Icon::Link)
        {
            const auto a = juce::Rectangle<float> (s * 0.62f, s * 0.34f);
            const auto rot = juce::AffineTransform::rotation (-0.75f, c.x, c.y);
            juce::Path l1, l2;
            l1.addRoundedRectangle (a.withCentre ({ c.x - s * 0.16f, c.y }), s * 0.17f);
            l2.addRoundedRectangle (a.withCentre ({ c.x + s * 0.16f, c.y }), s * 0.17f);
            g.strokePath (l1, juce::PathStrokeType (1.4f), rot);
            g.strokePath (l2, juce::PathStrokeType (1.4f), rot);
        }
        else
        {
            const float rad = s * 0.34f;
            juce::Path p;
            p.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
            g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.drawLine (c.x, c.y - rad - 1.5f, c.x, c.y - 0.5f, 1.5f);
        }
        content.removeFromLeft (3.0f);
    }
    if (text.isNotEmpty())
    {
        g.setFont (juce::Font (uiFont (11.0f, true)));
        g.drawFittedText (text, content.toNearestInt(), icon == Icon::None ? juce::Justification::centred : juce::Justification::centredLeft,
                          1, 0.8f);
    }
}

// ---------------------------------------------------------------------------------------------
Fader::Fader (juce::AudioProcessorValueTreeState& state, const juce::String& id, std::vector<std::pair<double, juce::String>> sc)
    : box (*state.getParameter (id), colours::accent), scale (std::move (sc)), attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setSliderSnapsToMousePosition (false);
    slider.setMouseDragSensitivity (220);
    auto* p = state.getParameter (id);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
    addAndMakeVisible (slider);
    addAndMakeVisible (box);
}

void Fader::resized()
{
    auto r = getLocalBounds();
    box.setBounds (r.removeFromBottom (22));
    r.removeFromBottom (6);
    slider.setBounds (r.withTrimmedLeft (r.getWidth() / 3));
}

void Fader::paint (juce::Graphics& g)
{
    const auto sb = slider.getBounds();
    g.setFont (juce::Font (uiFont (10.0f)));
    for (const auto& [value, text] : scale)
    {
        const float y = float (sb.getY()) + float (slider.getPositionOfValue (value));
        g.setColour (colours::panelEdge.brighter (0.4f));
        g.drawHorizontalLine (int (y), float (sb.getCentreX() - 18), float (sb.getCentreX() - 15));
        g.setColour (colours::textDim);
        g.drawText (text, juce::Rectangle<int> (0, int (y) - 7, sb.getX() + sb.getWidth() / 2 - 20, 14), juce::Justification::centredRight);
    }
}

} // namespace craft::dynamics::ui
