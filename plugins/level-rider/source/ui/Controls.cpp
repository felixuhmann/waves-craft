#include "Controls.h"

#include "LookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::levelrider::ui
{

// ---------------------------------------------------------------------------------------------
ValueBox::ValueBox (juce::RangedAudioParameter& p, juce::String id)
{
    auto* param = &p;
    cb.text = [param, id] { return formatValue (id, param->convertFrom0to1 (param->getValue()), true); };
    cb.editText = [param, id] { return formatValue (id, param->convertFrom0to1 (param->getValue()), false); };
    cb.set = [param] (const juce::String& t) {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValueForText (t));
        param->endChangeGesture();
    };
    cb.getNorm = [param] { return param->getValue(); };
    cb.setNorm = [param] (float v) { param->setValueNotifyingHost (v); };
    cb.beginDrag = [param] { param->beginChangeGesture(); };
    cb.endDrag = [param] { param->endChangeGesture(); };
    cb.reset = [param] {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getDefaultValue());
        param->endChangeGesture();
    };
    init();
}

ValueBox::ValueBox (Callbacks c) : cb (std::move (c))
{
    init();
}

void ValueBox::init()
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (uiFont (12.5f, true)));
    label.setColour (juce::Label::textColourId, colours::text);
    label.setColour (juce::Label::textWhenEditingColourId, colours::text);
    label.setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    label.setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    label.setInterceptsMouseClicks (false, true);
    label.onTextChange = [this] {
        cb.set (label.getText());
        refresh();
    };
    label.onEditorHide = [this] { refresh(); };
    addAndMakeVisible (label);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Drag or click to type; double-click resets");
    refresh();
}

void ValueBox::setFontHeight (float h) { label.setFont (juce::Font (uiFont (h, true))); }

void ValueBox::refresh()
{
    if (! label.isBeingEdited())
        label.setText (cb.text(), juce::dontSendNotification);
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

void ValueBox::mouseDown (const juce::MouseEvent& e)
{
    stopTimer();
    dragging = false;
    suppressClick = false;
    dragStart = cb.getNorm();
    if (e.mods.isAltDown())
    {
        cb.reset();
        refresh();
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
        cb.beginDrag();
    }
    const float sensitivity = e.mods.isShiftDown() ? 0.0005f : 0.004f;
    cb.setNorm (juce::jlimit (0.0f, 1.0f, dragStart - float (e.getDistanceFromDragStartY()) * sensitivity));
    refresh();
}

void ValueBox::mouseUp (const juce::MouseEvent&)
{
    if (gesture)
    {
        cb.endDrag();
        gesture = false;
    }
    if (! dragging && ! suppressClick)
        startTimer (juce::MouseEvent::getDoubleClickTimeout() + 20); // open the editor unless a double-click follows
    dragging = false;
}

void ValueBox::mouseDoubleClick (const juce::MouseEvent&)
{
    stopTimer();
    cb.reset();
    refresh();
}

void ValueBox::timerCallback()
{
    stopTimer();
    label.setText (cb.editText(), juce::dontSendNotification);
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
} // namespace

SegmentSwitch::SegmentSwitch (juce::RangedAudioParameter& p, juce::StringArray n) : names (std::move (n))
{
    const int count = names.size();
    attachment = std::make_unique<juce::ParameterAttachment> (p, [this, count] (float v) {
        index = juce::jlimit (0, count - 1, int (std::lround (v))); // physical value of a choice = index
        repaint();
    });
    // the attachment reports physical values: for a choice parameter that is the index
    setIndex = [this] (int i) { attachment->setValueAsCompleteGesture (float (i)); };
    getIndex = [this] { return index; };
    attachment->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

SegmentSwitch::SegmentSwitch (juce::StringArray n, std::function<int()> get, std::function<void (int)> set)
    : names (std::move (n)), getIndex (std::move (get)), setIndex (std::move (set))
{
    index = getIndex();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SegmentSwitch::refresh()
{
    const int i = getIndex();
    if (i != index)
    {
        index = i;
        repaint();
    }
}

juce::Rectangle<float> SegmentSwitch::segment (int i) const
{
    const auto r = getLocalBounds().toFloat();
    const float w = r.getWidth() / float (names.size());
    return { r.getX() + w * float (i), r.getY(), w, r.getHeight() };
}

void SegmentSwitch::paint (juce::Graphics& g)
{
    for (int i = 0; i < names.size(); ++i)
    {
        const bool on = i == index;
        const auto c = i < int (lit.size()) ? lit[size_t (i)] : colours::accent;
        const auto r = segment (i).reduced (1.5f, 1.0f);
        drawButtonFace (g, r, on, isMouseOver(), c);
        g.setColour (on ? colours::background : colours::textDim);
        g.setFont (juce::Font (uiFont (12.0f, true)));
        g.drawText (names[i].toUpperCase(), r, juce::Justification::centred);
    }
}

void SegmentSwitch::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    for (int i = 0; i < names.size(); ++i)
        if (segment (i).contains (e.position) && i != index)
        {
            setIndex (i);
            index = i;
            repaint();
        }
}

// ---------------------------------------------------------------------------------------------
Knob::Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, juce::String cap, bool bipolar)
    : attachment (state, paramId, slider), box (*state.getParameter (paramId), paramId), caption (std::move (cap))
{
    auto* p = state.getParameter (paramId);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (200);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setTooltip (caption + " - drag; double-click resets");
    slider.onValueChange = [this] { box.refresh(); };
    addAndMakeVisible (slider);
    addAndMakeVisible (box);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
    box.setBounds (r.removeFromBottom (22).withSizeKeepingCentre (std::min (r.getWidth(), 70), 22));
    r.removeFromBottom (2);
    slider.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (16), juce::Justification::centred);
}

} // namespace craft::levelrider::ui
