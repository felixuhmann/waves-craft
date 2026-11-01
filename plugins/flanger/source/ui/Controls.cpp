#include "Controls.h"

#include "FlangerLookAndFeel.h"

namespace craft::flanger::ui
{

void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, juce::Justification j)
{
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.5f, true)));
    g.drawFittedText (text.toUpperCase(), r, j, 1);
}

// ---------------------------------------------------------------------------------------------
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
        const auto t = textFn();
        if (t != label.getText())
            label.setText (t, juce::dontSendNotification);
    }
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

void ValueBox::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float delta = (w.isReversed ? -w.deltaY : w.deltaY) * 0.05f;
    param.beginChangeGesture();
    param.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, param.getValue() + delta));
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
Fader::Fader (juce::AudioProcessorValueTreeState& state, const juce::String& id, std::function<juce::String()> textFn,
              std::vector<ScaleMark> sc)
    : box (*state.getParameter (id), std::move (textFn)), scale (std::move (sc)), attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setSliderSnapsToMousePosition (false);
    slider.setVelocityBasedMode (false);
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
    r.removeFromBottom (8);
    slider.setBounds (r.withTrimmedLeft (r.getWidth() / 3));
}

void Fader::paint (juce::Graphics& g)
{
    const auto sb = slider.getBounds();
    g.setFont (juce::Font (uiFont (10.5f)));
    for (const auto& m : scale)
    {
        const float y = float (sb.getY()) + float (slider.getPositionOfValue (m.value));
        g.setColour (colours::panelEdge.brighter (0.4f));
        g.drawHorizontalLine (int (y), float (sb.getCentreX() - 18), float (sb.getCentreX() - 15));
        g.drawHorizontalLine (int (y), float (sb.getCentreX() + 15), float (sb.getCentreX() + 18));
        g.setColour (colours::textDim);
        g.drawText (m.text, juce::Rectangle<int> (0, int (y) - 7, sb.getX() - 2, 14), juce::Justification::centredRight);
    }
}

// ---------------------------------------------------------------------------------------------
static void paintToggleBody (juce::Graphics& g, juce::Button& b, bool over, bool active)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (active ? colours::accent : colours::well);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (over ? colours::accent : colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void PolarityButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const bool inverted = getToggleState();
    paintToggleBody (g, *this, over, inverted);
    auto c = getLocalBounds().toFloat().getCentre();
    const float s = std::min (getWidth(), getHeight()) * 0.22f;
    g.setColour (inverted ? colours::background : colours::text);
    g.fillRect (juce::Rectangle<float> (2 * s, 2.2f).withCentre (c));
    if (! inverted)
        g.fillRect (juce::Rectangle<float> (2.2f, 2 * s).withCentre (c));
}

// ---------------------------------------------------------------------------------------------
IconToggle::IconToggle (Icon i, juce::String c) : juce::Button (c), icon (i), caption (std::move (c))
{
    setClickingTogglesState (true);
}

void IconToggle::paintButton (juce::Graphics& g, bool over, bool)
{
    const bool on = getToggleState();
    const bool selector = icon == Icon::FilterType;
    paintToggleBody (g, *this, over, on && ! selector);
    const auto ink = (on && ! selector) ? colours::background : colours::text;

    auto r = getLocalBounds().toFloat().reduced (4.0f);
    juce::Rectangle<float> iconArea = r;
    if (caption.isNotEmpty() || selector)
        iconArea = r.removeFromLeft (r.getHeight() * 1.25f);

    const auto ic = iconArea.withSizeKeepingCentre (std::min (iconArea.getWidth(), iconArea.getHeight()),
                                                     std::min (iconArea.getWidth(), iconArea.getHeight()));
    const float s = ic.getWidth();
    juce::Path p;
    g.setColour (ink);
    switch (icon)
    {
        case Icon::Tape:
        {
            const float rr = s * 0.24f;
            g.drawEllipse (juce::Rectangle<float> (2 * rr, 2 * rr).withCentre ({ ic.getX() + rr + 0.5f, ic.getCentreY() - 1 }), 1.4f);
            g.drawEllipse (juce::Rectangle<float> (2 * rr, 2 * rr).withCentre ({ ic.getRight() - rr - 0.5f, ic.getCentreY() - 1 }), 1.4f);
            g.drawLine (ic.getX() + rr, ic.getCentreY() - 1 + rr, ic.getRight() - rr, ic.getCentreY() - 1 + rr, 1.4f);
            break;
        }
        case Icon::Sync:
        {
            // eighth note
            const float x = ic.getCentreX() + s * 0.12f, top = ic.getY() + s * 0.1f, bottom = ic.getBottom() - s * 0.25f;
            g.drawLine (x, top, x, bottom, 1.5f);
            g.fillEllipse (juce::Rectangle<float> (s * 0.34f, s * 0.26f).withCentre ({ x - s * 0.15f, bottom }));
            p.startNewSubPath (x, top);
            p.quadraticTo (x + s * 0.32f, top + s * 0.18f, x + s * 0.22f, top + s * 0.42f);
            g.strokePath (p, juce::PathStrokeType (1.5f));
            break;
        }
        case Icon::Link:
        {
            const auto a = juce::Rectangle<float> (s * 0.62f, s * 0.34f);
            const auto c = ic.getCentre();
            juce::AffineTransform rot = juce::AffineTransform::rotation (-0.75f, c.x, c.y);
            juce::Path link1, link2;
            link1.addRoundedRectangle (a.withCentre ({ c.x - s * 0.16f, c.y }), s * 0.17f);
            link2.addRoundedRectangle (a.withCentre ({ c.x + s * 0.16f, c.y }), s * 0.17f);
            g.strokePath (link1, juce::PathStrokeType (1.5f), rot);
            g.strokePath (link2, juce::PathStrokeType (1.5f), rot);
            break;
        }
        case Icon::Power:
        {
            const auto c = ic.getCentre();
            const float rad = s * 0.36f;
            p.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.drawLine (c.x, c.y - rad - 1.5f, c.x, c.y - 0.5f, 1.6f);
            break;
        }
        case Icon::Stop:
            g.fillRoundedRectangle (ic.withSizeKeepingCentre (s * 0.5f, s * 0.5f), 1.5f);
            break;
        case Icon::FilterType:
        {
            // response sketch: flat then falling (LP) or rising then flat (HP)
            const auto b = ic.reduced (s * 0.08f, s * 0.2f);
            if (on)
            {
                p.startNewSubPath (b.getX(), b.getBottom());
                p.quadraticTo (b.getX() + b.getWidth() * 0.35f, b.getY(), b.getX() + b.getWidth() * 0.6f, b.getY());
                p.lineTo (b.getRight(), b.getY());
            }
            else
            {
                p.startNewSubPath (b.getX(), b.getY());
                p.lineTo (b.getX() + b.getWidth() * 0.4f, b.getY());
                p.quadraticTo (b.getX() + b.getWidth() * 0.65f, b.getY(), b.getRight(), b.getBottom());
            }
            g.setColour (colours::accent2);
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (ink);
            break;
        }
    }

    const juce::String shown = selector ? juce::String (on ? "HP" : "LP") : caption;
    if (shown.isNotEmpty())
    {
        g.setFont (juce::Font (uiFont (11.0f, true)));
        g.drawText (shown, r, juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------------------------
void WaveformSwitch::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (over ? colours::accent.withAlpha (0.6f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    const bool tri = getToggleState();
    auto left = r.withWidth (r.getWidth() / 2).reduced (2.0f);
    auto right = left.withX (r.getCentreX() + 2.0f).withWidth (r.getRight() - r.getCentreX() - 4.0f);
    g.setColour (colours::accent);
    g.fillRoundedRectangle (tri ? right : left, 3.0f);

    auto wave = [&] (juce::Rectangle<float> area, bool triangle, bool selected) {
        auto b = area.reduced (area.getWidth() * 0.18f, area.getHeight() * 0.25f);
        juce::Path p;
        const int steps = 40;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = float (i) / steps;
            float v;
            if (triangle)
                v = t < 0.25f ? 4 * t : (t < 0.75f ? 2 - 4 * t : 4 * t - 4);
            else
                v = std::sin (juce::MathConstants<float>::twoPi * t);
            const juce::Point<float> pt (b.getX() + t * b.getWidth(), b.getCentreY() - v * b.getHeight() / 2);
            if (i == 0)
                p.startNewSubPath (pt);
            else
                p.lineTo (pt);
        }
        g.setColour (selected ? colours::background : colours::textDim);
        g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    wave (left, false, ! tri);
    wave (right, true, tri);
}

} // namespace craft::flanger::ui
