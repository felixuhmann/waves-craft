#include "Controls.h"

#include "FetLookAndFeel.h"

namespace craft::fet_limiter::ui
{

namespace
{
constexpr float startAngle = -2.35619449f; // -135 degrees from 12 o'clock
constexpr float endAngle = 2.35619449f;
} // namespace

// ---------------------------------------------------------------------------------------------
Knob::Knob (juce::RangedAudioParameter& p, const juce::String& t, bool isLarge, std::vector<ScaleLabel> s, juce::String cap)
    : param (p), attachment (p, [this] (float) { refreshText(); repaint(); }), title (t), caption (std::move (cap)),
      large (isLarge), scale (std::move (s)), accent (colours::themeA)
{
    readout.setJustificationType (juce::Justification::centred);
    readout.setFont (juce::Font (uiFont (large ? 13.5f : 12.0f, true)));
    readout.setColour (juce::Label::textColourId, colours::text);
    readout.setColour (juce::Label::textWhenEditingColourId, colours::text);
    readout.setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    readout.setInterceptsMouseClicks (false, true);
    readout.onTextChange = [this] {
        const float norm = param.getValueForText (readout.getText());
        beginGesture();
        setNormalised (norm);
        endGesture();
        refreshText();
    };
    readout.onEditorHide = [this] { refreshText(); };
    addAndMakeVisible (readout);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    attachment.sendInitialUpdate();
    refreshText();
}

Knob::~Knob() = default;

void Knob::setAccent (juce::Colour c)
{
    accent = c;
    readout.setColour (juce::Label::outlineWhenEditingColourId, c);
    repaint();
}

void Knob::refreshText()
{
    if (! readout.isBeingEdited())
        readout.setText (param.getCurrentValueAsText(), juce::dontSendNotification);
}

juce::Rectangle<float> Knob::dialArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (caption.isNotEmpty() ? 30.0f : 18.0f); // title (+ caption)
    r.removeFromBottom (22.0f); // readout
    if (! scale.empty())
        r = r.reduced (large ? 22.0f : 16.0f, large ? 14.0f : 10.0f);
    const float size = std::min (r.getWidth(), r.getHeight());
    return juce::Rectangle<float> (size, size).withCentre (r.getCentre());
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int w = std::min (r.getWidth(), large ? 96 : 76);
    readout.setBounds (r.removeFromBottom (20).withSizeKeepingCentre (w, 20));
}

void Knob::paint (juce::Graphics& g)
{
    const auto area = dialArea();
    const auto c = area.getCentre();
    const float radius = area.getWidth() * 0.5f;
    const float norm = param.getValue();
    const float angle = startAngle + norm * (endAngle - startAngle);

    // title
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (large ? 12.5f : 11.5f, true)));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (16), juce::Justification::centred);

    // readout well
    g.setColour (colours::well);
    g.fillRoundedRectangle (readout.getBounds().toFloat(), 3.0f);

    // scale labels and ticks around the dial
    if (! scale.empty())
    {
        g.setFont (juce::Font (uiFont (10.5f)));
        for (const auto& s : scale)
        {
            const float a = startAngle + s.norm * (endAngle - startAngle);
            const auto dir = juce::Point<float> (std::sin (a), -std::cos (a));
            const auto p0 = c + dir * (radius + 2.0f), p1 = c + dir * (radius + 6.0f);
            g.setColour (colours::textDim.withAlpha (0.8f));
            g.drawLine ({ p0, p1 }, 1.2f);
            const auto tp = c + dir * (radius + (large ? 15.0f : 12.0f));
            g.setColour (colours::textDim);
            g.drawText (s.text, juce::Rectangle<float> (30.0f, 12.0f).withCentre (tp), juce::Justification::centred);
        }
    }

    // value ring
    const float ring = std::max (3.0f, radius * 0.11f);
    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - ring * 0.5f, radius - ring * 0.5f, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::well);
    g.strokePath (track, juce::PathStrokeType (ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path value;
    value.addCentredArc (c.x, c.y, radius - ring * 0.5f, radius - ring * 0.5f, 0.0f, startAngle, angle, true);
    g.setColour (accent.withAlpha (isEnabled() ? 0.95f : 0.35f));
    g.strokePath (value, juce::PathStrokeType (ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body: faceted cap (original design, flat grey with a soft top light)
    const float capR = radius - ring - 4.0f;
    juce::Path cap;
    const int facets = large ? 12 : 10;
    for (int i = 0; i < facets; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * float (i) / float (facets) + angle;
        const auto p = c + juce::Point<float> (std::sin (a), -std::cos (a)) * capR;
        if (i == 0)
            cap.startNewSubPath (p);
        else
            cap.lineTo (p);
    }
    cap.closeSubPath();
    const auto capBounds = juce::Rectangle<float> (2 * capR, 2 * capR).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (capBounds.translated (0.0f, 2.5f).expanded (1.0f));
    juce::ColourGradient grad (juce::Colour (0xff434b54), c.x, c.y - capR, juce::Colour (0xff1c2025), c.x, c.y + capR, false);
    g.setGradientFill (grad);
    g.fillPath (cap);
    g.setColour (juce::Colour (0xff0a0c0e));
    g.strokePath (cap, juce::PathStrokeType (1.0f));
    g.setColour (juce::Colour (0xff2a3037));
    g.fillEllipse (capBounds.reduced (capR * 0.32f));

    // pointer
    juce::Path ptr;
    const float pw = std::max (2.0f, capR * 0.12f);
    ptr.addRoundedRectangle (-pw * 0.5f, -capR + 3.0f, pw, capR * 0.5f, pw * 0.5f);
    g.setColour (colours::text);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));

    if (caption.isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (10.0f)));
        g.drawText (caption, getLocalBounds().withY (16).withHeight (12), juce::Justification::centred);
    }
}

void Knob::beginGesture()
{
    if (! inGesture)
    {
        inGesture = true;
        if (onGestureStart)
            onGestureStart();
        param.beginChangeGesture();
    }
}

void Knob::endGesture()
{
    if (inGesture)
    {
        param.endChangeGesture();
        inGesture = false;
        if (onGestureEnd)
            onGestureEnd();
    }
}

void Knob::setNormalised (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (onUserValue)
        onUserValue (v);
    else
        param.setValueNotifyingHost (v);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    stopTimer();
    dragging = false;
    suppressClick = false;
    clickOnReadout = readout.getBounds().contains (e.getPosition());
    dragStart = param.getValue();
    if (e.mods.isAltDown())
    {
        beginGesture();
        setNormalised (param.getDefaultValue());
        endGesture();
        suppressClick = true;
    }
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (suppressClick)
        return;
    if (! dragging && e.getDistanceFromDragStart() < 3)
        return;
    if (! dragging)
    {
        dragging = true;
        beginGesture();
    }
    const float sensitivity = e.mods.isShiftDown() ? 0.0008f : 0.005f;
    setNormalised (dragStart - float (e.getDistanceFromDragStartY()) * sensitivity);
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        endGesture();
    else if (! suppressClick && clickOnReadout)
        startTimer (juce::MouseEvent::getDoubleClickTimeout() + 20); // type a value unless a double-click follows
    dragging = false;
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    stopTimer();
    beginGesture();
    setNormalised (param.getDefaultValue());
    endGesture();
}

void Knob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float delta = (w.isReversed ? -w.deltaY : w.deltaY) * 0.04f;
    beginGesture();
    setNormalised (param.getValue() + delta);
    endGesture();
}

void Knob::timerCallback()
{
    stopTimer();
    readout.showEditor();
    if (auto* ed = readout.getCurrentTextEditor())
    {
        ed->setJustification (juce::Justification::centred);
        ed->selectAll();
    }
}

// ---------------------------------------------------------------------------------------------
ButtonGroup::ButtonGroup (juce::RangedAudioParameter& p, const juce::StringArray& l, bool v, std::vector<int> o)
    : param (p), labels (l), vertical (v), order (std::move (o)), attachment (p, [this] (float value) {
          selected = juce::roundToInt (value);
          repaint();
          if (onChange)
              onChange (selected);
      }),
      accent (colours::themeA)
{
    if (order.empty())
        for (int i = 0; i < labels.size(); ++i)
            order.push_back (i);
    attachment.sendInitialUpdate();
}

void ButtonGroup::setAccent (juce::Colour c)
{
    accent = c;
    repaint();
}

juce::Rectangle<float> ButtonGroup::slot (int i) const
{
    const auto r = getLocalBounds().toFloat();
    const int n = int (order.size());
    if (vertical)
    {
        const float h = r.getHeight() / float (n);
        return { r.getX(), r.getY() + h * float (i), r.getWidth(), h };
    }
    const float w = r.getWidth() / float (n);
    return { r.getX() + w * float (i), r.getY(), w, r.getHeight() };
}

int ButtonGroup::slotAt (juce::Point<float> p) const
{
    for (int i = 0; i < int (order.size()); ++i)
        if (slot (i).contains (p))
            return i;
    return -1;
}

void ButtonGroup::paint (juce::Graphics& g)
{
    for (int i = 0; i < int (order.size()); ++i)
    {
        const int index = order[size_t (i)];
        auto s = slot (i).reduced (vertical ? 0.0f : 2.0f, vertical ? 2.0f : 0.0f);
        const bool on = index == selected;
        g.setColour (on ? accent : (i == hover ? colours::well.brighter (0.15f) : colours::well));
        g.fillRoundedRectangle (s, 4.0f);
        g.setColour (on ? accent.brighter (0.3f) : colours::panelEdge.brighter (0.2f));
        g.drawRoundedRectangle (s.reduced (0.5f), 4.0f, 1.0f);
        // latch lamp
        const auto lamp = juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ s.getX() + 9.0f, s.getCentreY() });
        g.setColour (on ? colours::background.withAlpha (0.7f) : colours::panelEdge.brighter (0.3f));
        g.fillEllipse (lamp);
        g.setColour (on ? colours::background : colours::text);
        g.setFont (juce::Font (uiFont (12.0f, true)));
        g.drawFittedText (labels[index], s.withTrimmedLeft (12.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

void ButtonGroup::mouseDown (const juce::MouseEvent& e)
{
    const int i = slotAt (e.position);
    if (i < 0)
        return;
    const int index = order[size_t (i)];
    if (e.mods.isAltDown())
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
    else if (index != selected)
        attachment.setValueAsCompleteGesture (float (index));
}

void ButtonGroup::mouseMove (const juce::MouseEvent& e)
{
    const int i = slotAt (e.position);
    if (i != hover)
    {
        hover = i;
        repaint();
    }
}

void ButtonGroup::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

// ---------------------------------------------------------------------------------------------
ToggleSwitch::ToggleSwitch (juce::RangedAudioParameter& p, const juce::String& t)
    : param (p), text (t), attachment (p, [this] (float v) {
          on = v >= 0.5f;
          repaint();
      }),
      accent (colours::themeA)
{
    attachment.sendInitialUpdate();
}

void ToggleSwitch::setAccent (juce::Colour c)
{
    accent = c;
    repaint();
}

void ToggleSwitch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (isMouseOver() ? colours::well.brighter (0.12f) : colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? accent : colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    const auto lamp = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 12.0f, r.getCentreY() });
    if (on)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.fillEllipse (lamp.expanded (3.0f));
    }
    g.setColour (on ? accent : colours::panelEdge.brighter (0.4f));
    g.fillEllipse (lamp);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (11.5f, true)));
    g.drawFittedText (text, r.withTrimmedLeft (20.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void ToggleSwitch::mouseDown (const juce::MouseEvent& e)
{
    attachment.setValueAsCompleteGesture (e.mods.isAltDown() ? 0.0f : (on ? 0.0f : 1.0f));
}

} // namespace craft::fet_limiter::ui
