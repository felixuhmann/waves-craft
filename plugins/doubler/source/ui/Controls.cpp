#include "Controls.h"

#include "DoublerLookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::doubler::ui
{

// ---------------------------------------------------------------------------------------------
ValueBox::ValueBox (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::String u)
    : param (state, id), paramId (id), unit (std::move (u)), accent (colours::accent)
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (uiFont (12.0f, true)));
    label.setColour (juce::Label::textColourId, colours::text);
    label.setColour (juce::Label::textWhenEditingColourId, colours::text);
    label.setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    label.setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    label.setInterceptsMouseClicks (false, true);
    label.onTextChange = [this] {
        if (param.p == nullptr)
            return;
        const float norm = param.p->getValueForText (label.getText());
        param.begin();
        param.setNormalised (norm);
        param.end();
        refresh();
    };
    label.onEditorHide = [this] { refresh(); };
    addAndMakeVisible (label);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    refresh();
}

void ValueBox::setFontHeight (float h) { label.setFont (juce::Font (uiFont (h, true))); }

juce::String ValueBox::currentText() const
{
    auto t = formatValue (paramId, param.get());
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
    g.setColour (isMouseOverOrDragging() ? accent.withAlpha (0.8f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void ValueBox::resized() { label.setBounds (getLocalBounds()); }

void ValueBox::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    suppress = false;
    dragStart = param.p != nullptr ? param.p->getValue() : 0.0f;
    if (e.mods.isAltDown())
    {
        param.resetToDefault();
        refresh();
        suppress = true;
    }
}

void ValueBox::mouseDrag (const juce::MouseEvent& e)
{
    if (suppress || param.p == nullptr)
        return;
    if (! dragging && e.getDistanceFromDragStart() < 3)
        return;
    if (! dragging)
    {
        dragging = true;
        gesture = true;
        param.begin();
    }
    const float sensitivity = e.mods.isShiftDown() ? 0.0005f : 0.005f; // 200 px for the full range
    param.setNormalised (dragStart - float (e.getDistanceFromDragStartY()) * sensitivity);
    refresh();
}

void ValueBox::mouseUp (const juce::MouseEvent&)
{
    if (gesture)
    {
        param.end();
        gesture = false;
    }
    dragging = false;
}

void ValueBox::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isAltDown())
    {
        param.resetToDefault();
        refresh();
        return;
    }
    label.showEditor();
    if (auto* ed = label.getCurrentTextEditor())
    {
        ed->setJustification (juce::Justification::centred);
        ed->setText (formatValue (paramId, param.get()), false);
        ed->selectAll();
    }
}

// ---------------------------------------------------------------------------------------------
namespace
{
void setupSlider (juce::Slider& s, juce::Slider::SliderStyle style, juce::RangedAudioParameter* p)
{
    s.setSliderStyle (style);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setSliderSnapsToMousePosition (false);
    s.setVelocityBasedMode (false);
    s.setMouseDragSensitivity (200);
    s.setScrollWheelEnabled (true);
    if (p != nullptr)
        s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
}
} // namespace

Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::String unit, Layout l)
    : box (state, id, std::move (unit)), layout (l), attachment (state, id, slider)
{
    setupSlider (slider, juce::Slider::RotaryVerticalDrag, state.getParameter (id));
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    // the attachment sets a linear range on the slider; keep the display bipolar for centred parameters
    addAndMakeVisible (slider);
    addAndMakeVisible (box);
    box.setFontHeight (11.0f);
}

void Knob::setAccent (juce::Colour c)
{
    slider.setColour (juce::Slider::thumbColourId, c);
    box.setAccent (c);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    if (layout == Layout::ValueRight)
    {
        const int k = std::min (r.getHeight(), 34);
        slider.setBounds (r.removeFromLeft (k).withSizeKeepingCentre (k, k));
        r.removeFromLeft (3);
        box.setBounds (r.withSizeKeepingCentre (r.getWidth(), 20));
    }
    else
    {
        box.setBounds (r.removeFromBottom (18).reduced (2, 0));
        const int k = std::min (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (k, k));
    }
}

// ---------------------------------------------------------------------------------------------
Fader::Fader (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    : box (state, id, "dB"), attachment (state, id, slider)
{
    setupSlider (slider, juce::Slider::LinearVertical, state.getParameter (id));
    addAndMakeVisible (slider);
    addAndMakeVisible (box);
}

void Fader::resized()
{
    auto r = getLocalBounds();
    box.setBounds (r.removeFromBottom (22).reduced (4, 0));
    r.removeFromBottom (6);
    slider.setBounds (r.reduced (0, 4));
}

void Fader::paint (juce::Graphics& g)
{
    const auto sb = slider.getBounds();
    g.setFont (juce::Font (uiFont (10.0f)));
    for (double db : { 0.0, -6.0, -12.0, -18.0, -24.0 })
    {
        const float y = float (sb.getY()) + float (slider.getPositionOfValue (db));
        g.setColour (colours::panelEdge.brighter (0.4f));
        g.drawHorizontalLine (int (y), float (sb.getCentreX() - 18), float (sb.getCentreX() - 15));
        g.setColour (colours::textDim);
        g.drawText (juce::String (int (db)), juce::Rectangle<int> (0, int (y) - 6, sb.getCentreX() - 20, 12),
                    juce::Justification::centredRight);
    }
}

// ---------------------------------------------------------------------------------------------
ToggleButton::ToggleButton (Style s, juce::String text, juce::Colour c) : juce::Button (text), style (s), colour (c)
{
    setClickingTogglesState (true);
}

void ToggleButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    switch (style)
    {
        case Style::VoiceNumber:
        {
            g.setColour (on ? colour : colours::well);
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (on ? colour.brighter (0.3f) : colour.withAlpha (over ? 0.9f : 0.5f));
            g.drawRoundedRectangle (r, 5.0f, 1.5f);
            g.setColour (on ? colours::background : colour.withAlpha (0.8f));
            g.setFont (juce::Font (uiFont (r.getHeight() * 0.6f, true)));
            g.drawText (getButtonText(), r, juce::Justification::centred);
            break;
        }
        case Style::OctaveArrow:
        {
            g.setColour (on ? colour : colours::well);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (over ? colour : colours::panelEdge.brighter (0.2f));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            const auto c = r.getCentre();
            const float s = std::min (r.getWidth(), r.getHeight()) * 0.28f;
            juce::Path p;
            p.addArrow ({ c.x, c.y - s * 1.2f, c.x, c.y + s * 1.2f }, 2.0f, s * 1.5f, s);
            g.setColour (on ? colours::background : colours::textDim);
            g.fillPath (p);
            break;
        }
        case Style::Led:
        {
            g.setColour (down ? colours::well.brighter (0.1f) : colours::well);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (over ? colour.withAlpha (0.8f) : colours::panelEdge.brighter (0.2f));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            const float d = std::min (8.0f, r.getHeight() * 0.4f);
            auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + 4.0f + d * 0.5f + 2.0f, r.getCentreY() });
            if (on)
            {
                g.setColour (colour.withAlpha (0.3f));
                g.fillEllipse (led.expanded (2.5f));
            }
            g.setColour (on ? colour : colours::panelEdge.brighter (0.3f));
            g.fillEllipse (led);
            g.setColour (on ? colours::text : colours::textDim);
            g.setFont (juce::Font (uiFont (11.0f, true)));
            g.drawText (getButtonText(), r.withTrimmedLeft (d + 8.0f), juce::Justification::centred);
            break;
        }
        case Style::Text:
        {
            g.setColour (on ? colour : colours::well);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (over ? colour : colours::panelEdge.brighter (0.2f));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            g.setColour (on ? colours::background : colours::textDim);
            g.setFont (juce::Font (uiFont (11.0f, true)));
            g.drawText (getButtonText(), r, juce::Justification::centred);
            break;
        }
    }
}

// ---------------------------------------------------------------------------------------------
SegmentSwitch::SegmentSwitch (juce::String l, juce::String r)
    : juce::Button (l + "/" + r), leftText (std::move (l)), rightText (std::move (r))
{
    setClickingTogglesState (true);
}

void SegmentSwitch::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (over ? colours::accent.withAlpha (0.6f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    auto left = r.withWidth (r.getWidth() / 2).reduced (2.0f);
    auto right = left.withX (r.getCentreX() + 2.0f).withWidth (r.getRight() - r.getCentreX() - 4.0f);
    const bool on = getToggleState();
    g.setColour (colours::accent);
    g.fillRoundedRectangle (on ? right : left, 3.0f);

    g.setFont (juce::Font (uiFont (11.0f, true)));
    g.setColour (on ? colours::textDim : colours::background);
    g.drawText (leftText, left, juce::Justification::centred);
    g.setColour (on ? colours::background : colours::textDim);
    g.drawText (rightText, right, juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------------
ChoiceSegments::ChoiceSegments (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::StringArray l)
    : param (state, id), labels (std::move (l))
{
    refresh();
}

void ChoiceSegments::refresh()
{
    const int v = int (std::lround (param.get()));
    if (v != current)
    {
        current = v;
        repaint();
    }
}

int ChoiceSegments::segmentAt (juce::Point<int> p) const
{
    return juce::jlimit (0, labels.size() - 1, p.x * labels.size() / std::max (1, getWidth()));
}

void ChoiceSegments::mouseDown (const juce::MouseEvent& e)
{
    const int s = segmentAt (e.getPosition());
    param.setGesture (double (s));
    refresh();
}

void ChoiceSegments::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    const int n = labels.size();
    const float w = r.getWidth() / float (n);
    const int hover = isMouseOver() ? segmentAt (getMouseXYRelative()) : -1;
    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float> (r.getX() + w * float (i), r.getY(), w, r.getHeight()).reduced (2.0f);
        if (i == current)
        {
            g.setColour (colours::accent2);
            g.fillRoundedRectangle (seg, 3.0f);
        }
        else if (i == hover)
        {
            g.setColour (colours::panel.brighter (0.15f));
            g.fillRoundedRectangle (seg, 3.0f);
        }
        g.setColour (i == current ? colours::background : colours::textDim);
        g.setFont (juce::Font (uiFont (11.0f, true)));
        g.drawText (labels[i], seg, juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------------------------
MomentaryButton::MomentaryButton (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::String t)
    : param (state, id), text (std::move (t))
{
}

void MomentaryButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (held ? colours::accent : colours::panel.brighter (isMouseOver() ? 0.15f : 0.05f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    // circular-arrow glyph + text
    auto glyph = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.25f);
    juce::Path arc;
    arc.addCentredArc (glyph.getCentreX(), glyph.getCentreY(), glyph.getWidth() * 0.5f, glyph.getWidth() * 0.5f, 0.0f,
                       0.6f, juce::MathConstants<float>::twoPi - 0.2f, true);
    g.setColour (held ? colours::background : colours::text);
    g.strokePath (arc, juce::PathStrokeType (1.6f));
    const juce::Point<float> tip (glyph.getCentreX() + glyph.getWidth() * 0.5f * std::sin (0.6f),
                                  glyph.getCentreY() - glyph.getWidth() * 0.5f * std::cos (0.6f));
    juce::Path head;
    head.addTriangle (tip.x - 3.5f, tip.y - 3.0f, tip.x + 2.5f, tip.y - 1.0f, tip.x - 1.0f, tip.y + 3.5f);
    g.fillPath (head);
    g.setFont (juce::Font (uiFont (11.0f, true)));
    g.drawText (text, r, juce::Justification::centred);
}

void MomentaryButton::mouseDown (const juce::MouseEvent&)
{
    held = true;
    param.begin();
    param.set (1.0);
    repaint();
}

void MomentaryButton::mouseUp (const juce::MouseEvent&)
{
    held = false;
    param.set (0.0);
    param.end();
    repaint();
}

} // namespace craft::doubler::ui
