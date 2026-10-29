#include "Controls.h"

#include "MotionLookAndFeel.h"

namespace craft::mfp::ui
{

namespace
{
void setupRotary (juce::Slider& s, juce::Colour accent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    s.setColour (juce::Slider::thumbColourId, accent);
    s.setVelocityBasedMode (false);
    s.setMouseDragSensitivity (220);
}
} // namespace

// ---------------------------------------------------------------------------------------------
Knob::Knob (APVTS& st, const juce::String& paramId, const juce::String& t, juce::Colour accent) : title (t)
{
    param = st.getParameter (paramId);
    setupRotary (slider, accent);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (st, paramId, slider);
    if (param != nullptr)
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    refresh();
}

void Knob::setAlternate (APVTS& st, const juce::String& altParamId, const juce::String& switchParamId)
{
    altParam = st.getParameter (altParamId);
    switchParam = st.getParameter (switchParamId);
    setupRotary (altSlider, slider.findColour (juce::Slider::thumbColourId));
    addChildComponent (altSlider);
    altAttachment = std::make_unique<APVTS::SliderAttachment> (st, altParamId, altSlider);
    if (altParam != nullptr)
        altSlider.setDoubleClickReturnValue (true, altParam->convertFrom0to1 (altParam->getDefaultValue()));
    resized();
    refresh();
}

void Knob::setTitle (const juce::String& t)
{
    if (t != title)
    {
        title = t;
        repaint();
    }
}

void Knob::refresh()
{
    const bool alt = switchParam != nullptr && switchParam->getValue() >= 0.5f;
    if (alt != showingAlt)
    {
        showingAlt = alt;
        slider.setVisible (! alt);
        altSlider.setVisible (alt);
    }
    auto* p = showingAlt ? altParam : param;
    const auto text = p != nullptr ? p->getCurrentValueAsText() : juce::String();
    if (text != valueText)
    {
        valueText = text;
        repaint();
    }
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f, true)).withExtraKerningFactor (0.08f));
    g.drawText (title.toUpperCase(), r.removeFromTop (14), juce::Justification::centred);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (11.5f)));
    g.drawText (valueText, r.removeFromBottom (15), juce::Justification::centred);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (14);
    r.removeFromBottom (15);
    const int s = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
    altSlider.setBounds (slider.getBounds());
}

// ---------------------------------------------------------------------------------------------
ModSlider::ModSlider (APVTS& st, const juce::String& paramId, const juce::String& l, juce::Colour c) : label (l), colour (c)
{
    param = st.getParameter (paramId);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::thumbColourId, colour);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (st, paramId, slider);
    slider.setTooltip (param != nullptr ? param->getName (64) : juce::String());
    refresh();
}

void ModSlider::refresh()
{
    const auto text = param != nullptr ? param->getCurrentValueAsText() : juce::String();
    if (text != valueText)
    {
        valueText = text;
        repaint();
    }
}

void ModSlider::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setFont (juce::Font (uiFont (9.5f, true)));
    g.setColour (colour.withAlpha (0.9f));
    g.drawText (label, r.removeFromLeft (38), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.drawText (valueText.upToFirstOccurrenceOf (" ", false, false), r.removeFromRight (30), juce::Justification::centredRight);
}

void ModSlider::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (38);
    r.removeFromRight (32);
    slider.setBounds (r);
}

// ---------------------------------------------------------------------------------------------
SegmentToggle::SegmentToggle (APVTS& st, const juce::String& paramId, juce::String off, juce::String on, juce::Colour a)
    : juce::Button (paramId), offText (std::move (off)), onText (std::move (on)), accent (a)
{
    setClickingTogglesState (true);
    if (paramId.isNotEmpty() && st.getParameter (paramId) != nullptr) // empty id: not bound to a parameter
    {
        attachment = std::make_unique<APVTS::ButtonAttachment> (st, paramId, *this);
        setTooltip (st.getParameter (paramId)->getName (64));
    }
}

void SegmentToggle::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge.brighter (over ? 0.4f : 0.15f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    auto left = r.withWidth (r.getWidth() / 2).reduced (2.0f);
    auto right = left.withX (r.getCentreX() + 0.0f).withWidth (r.getRight() - r.getCentreX() - 2.0f);
    const bool on = getToggleState();
    g.setColour (accent.withAlpha (0.9f));
    g.fillRoundedRectangle (on ? right : left, 3.0f);
    g.setFont (juce::Font (uiFont (std::min (11.0f, r.getHeight() * 0.55f), true)));
    g.setColour (on ? colours::textDim : colours::background);
    g.drawText (offText, left, juce::Justification::centred);
    g.setColour (on ? colours::background : colours::textDim);
    g.drawText (onText, right, juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------------
ChoiceStrip::ChoiceStrip (APVTS& st, const juce::String& paramId, juce::StringArray l, juce::Colour a, int cols)
    : labels (std::move (l)), accent (a), columns (cols > 0 ? cols : 0)
{
    param = st.getParameter (paramId);
    if (columns == 0)
        columns = labels.size();
    if (param != nullptr)
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) {
            selected = juce::roundToInt (v);
            repaint();
        });
        attachment->sendInitialUpdate();
        setTooltip (param->getName (64));
    }
}

juce::Rectangle<float> ChoiceStrip::cell (int i) const
{
    const int rows = (labels.size() + columns - 1) / columns;
    const float w = float (getWidth()) / float (columns), h = float (getHeight()) / float (rows);
    return { float (i % columns) * w, float (i / columns) * h, w, h };
}

int ChoiceStrip::indexAt (juce::Point<float> p) const
{
    for (int i = 0; i < labels.size(); ++i)
        if (cell (i).contains (p))
            return i;
    return -1;
}

void ChoiceStrip::paint (juce::Graphics& g)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto c = cell (i).reduced (1.5f);
        const bool sel = i == selected;
        g.setColour (sel ? accent : (i == hover ? colours::panel.brighter (0.25f) : colours::well));
        g.fillRoundedRectangle (c, 4.0f);
        g.setColour (colours::panelEdge.brighter (0.15f));
        g.drawRoundedRectangle (c, 4.0f, 1.0f);
        const auto ink = sel ? colours::background : colours::text.withAlpha (0.85f);
        if (icon)
            icon (g, c.reduced (4.0f), i, ink);
        else
        {
            g.setColour (ink);
            g.setFont (juce::Font (uiFont (std::min (11.5f, c.getHeight() * 0.5f), true)));
            g.drawText (labels[i], c, juce::Justification::centred);
        }
    }
}

void ChoiceStrip::mouseDown (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i >= 0 && attachment != nullptr)
        attachment->setValueAsCompleteGesture (float (i));
}

void ChoiceStrip::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i != hover)
    {
        hover = i;
        repaint();
    }
}

void ChoiceStrip::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

// ---------------------------------------------------------------------------------------------
DragValue::DragValue (APVTS& st, const juce::String& paramId, const juce::String& t, juce::Colour a) : title (t), accent (a)
{
    param = st.getParameter (paramId);
    if (param != nullptr)
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) {
            value = v;
            repaint();
        });
        attachment->sendInitialUpdate();
        setTooltip (param->getName (64) + " (drag or scroll)");
    }
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void DragValue::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f, true)).withExtraKerningFactor (0.08f));
    g.drawText (title.toUpperCase(), r.removeFromTop (14), juce::Justification::centred);
    auto box = r.reduced (2, 1).toFloat();
    g.setColour (colours::well);
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (accent.withAlpha (0.7f));
    g.drawRoundedRectangle (box, 4.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (14.0f, true)));
    g.drawText (juce::String (juce::roundToInt (value)), box, juce::Justification::centred);
}

void DragValue::mouseDown (const juce::MouseEvent&)
{
    dragStartValue = value;
    if (attachment != nullptr)
        attachment->beginGesture();
}

void DragValue::mouseDrag (const juce::MouseEvent& e)
{
    if (attachment == nullptr || param == nullptr)
        return;
    const auto range = param->getNormalisableRange();
    const float v = juce::jlimit (range.start, range.end, std::round (dragStartValue - float (e.getDistanceFromDragStartY()) / 8.0f));
    if (v != value)
        attachment->setValueAsPartOfGesture (v);
}

void DragValue::mouseUp (const juce::MouseEvent&)
{
    if (attachment != nullptr)
        attachment->endGesture();
}

void DragValue::mouseDoubleClick (const juce::MouseEvent&)
{
    if (attachment != nullptr && param != nullptr)
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
}

void DragValue::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (attachment == nullptr || param == nullptr)
        return;
    const auto range = param->getNormalisableRange();
    const float step = w.deltaY > 0 ? 1.0f : (w.deltaY < 0 ? -1.0f : 0.0f);
    attachment->setValueAsCompleteGesture (juce::jlimit (range.start, range.end, value + step));
}

} // namespace craft::mfp::ui
