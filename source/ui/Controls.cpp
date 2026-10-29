#include "Controls.h"

#include "CraftLookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::ui
{

// ---------------------------------------------------------------------------------------------
ValueBox::ValueBox (juce::RangedAudioParameter& p, juce::String id) : param (p), paramId (std::move (id))
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

juce::String ValueBox::currentText() const
{
    return formatValue (paramId, param.convertFrom0to1 (param.getValue()));
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
void setupSlider (juce::Slider& s, juce::Slider::SliderStyle style)
{
    s.setSliderStyle (style);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setSliderSnapsToMousePosition (false);
    s.setVelocityBasedMode (false);
    s.setMouseDragSensitivity (220);
}

void drawTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, juce::Justification j)
{
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (12.0f, true)));
    g.drawFittedText (text.toUpperCase(), r, j, 1);
}
} // namespace

Fader::Fader (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& t,
              std::vector<ScaleMark> sc)
    : box (*state.getParameter (id), id), title (t), scale (std::move (sc)), attachment (state, id, slider)
{
    setupSlider (slider, juce::Slider::LinearVertical);
    auto* p = state.getParameter (id);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
    addAndMakeVisible (slider);
    addAndMakeVisible (box);
}

void Fader::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (20);
    box.setBounds (r.removeFromBottom (22).reduced (6, 0));
    r.removeFromBottom (6);
    slider.setBounds (r.withTrimmedLeft (r.getWidth() / 3));
}

void Fader::paint (juce::Graphics& g)
{
    drawTitle (g, getLocalBounds().removeFromTop (18), title, juce::Justification::centred);

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
HSlider::HSlider (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& t,
                  std::vector<ScaleMark> sc)
    : box (*state.getParameter (id), id), title (t), scale (std::move (sc)), attachment (state, id, slider)
{
    setupSlider (slider, juce::Slider::LinearHorizontal);
    auto* p = state.getParameter (id);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
    slider.setColour (juce::Slider::thumbColourId, colours::accent2);
    addAndMakeVisible (slider);
    addAndMakeVisible (box);
}

void HSlider::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (76);
    box.setBounds (r.removeFromRight (52).withSizeKeepingCentre (52, 22));
    r.removeFromRight (6);
    slider.setBounds (r.withTrimmedBottom (12));
}

void HSlider::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    drawTitle (g, r.removeFromLeft (72).withTrimmedBottom (12), title, juce::Justification::centredLeft);
    const auto sb = slider.getBounds();
    g.setFont (juce::Font (uiFont (10.0f)));
    for (const auto& m : scale)
    {
        const float x = float (sb.getX()) + float (slider.getPositionOfValue (m.value));
        g.setColour (colours::panelEdge.brighter (0.4f));
        g.drawVerticalLine (int (x), float (sb.getBottom() - 1), float (sb.getBottom() + 2));
        g.setColour (colours::textDim);
        g.drawText (m.text, juce::Rectangle<int> (int (x) - 16, sb.getBottom() + 1, 32, 11), juce::Justification::centred);
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

    g.setFont (juce::Font (uiFont (11.5f, true)));
    g.setColour (on ? colours::textDim : colours::background);
    g.drawText (leftText, left, juce::Justification::centred);
    g.setColour (on ? colours::background : colours::textDim);
    g.drawText (rightText, right, juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------------
static void paintRoundToggle (juce::Graphics& g, juce::Button& b, bool over, bool active)
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
    paintRoundToggle (g, *this, over, inverted);
    auto c = getLocalBounds().toFloat().getCentre();
    const float s = std::min (getWidth(), getHeight()) * 0.22f;
    g.setColour (inverted ? colours::background : colours::text);
    g.fillRect (juce::Rectangle<float> (2 * s, 2.2f).withCentre (c));
    if (! inverted)
        g.fillRect (juce::Rectangle<float> (2.2f, 2 * s).withCentre (c));
}

void SwapButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const bool on = getToggleState();
    paintRoundToggle (g, *this, over, on);
    auto r = getLocalBounds().toFloat().reduced (getWidth() * 0.22f, 0.0f);
    const float y = r.getCentreY();
    juce::Path p;
    p.addArrow ({ r.getCentreX(), y, r.getX(), y }, 2.0f, 8.0f, 6.0f);
    p.addArrow ({ r.getCentreX(), y, r.getRight(), y }, 2.0f, 8.0f, 6.0f);
    g.setColour (on ? colours::background : colours::text);
    g.fillPath (p);
}

} // namespace craft::ui
