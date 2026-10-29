#include "StepEditor.h"

#include "MotionLookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::mfp::ui
{

StepEditor::StepEditor (juce::AudioProcessorValueTreeState& st)
{
    for (int i = 0; i < 16; ++i)
    {
        params[size_t (i)] = st.getParameter (ids::step (i));
        if (params[size_t (i)] == nullptr)
            continue;
        attachments[size_t (i)] = std::make_unique<juce::ParameterAttachment> (*params[size_t (i)], [this, i] (float v) {
            values[size_t (i)] = v;
            repaint();
        });
        attachments[size_t (i)]->sendInitialUpdate();
    }
    setTooltip ("Sequencer steps: drag a bar (Drag mode) or paint across bars (Draw mode); double-click resets a step");
}

void StepEditor::setPlayingStep (int step)
{
    if (step != playing)
    {
        playing = step;
        repaint();
    }
}

void StepEditor::setLastStep (int l)
{
    if (l != last)
    {
        last = l;
        repaint();
    }
}

juce::Rectangle<float> StepEditor::bar (int i) const
{
    const float w = float (getWidth()) / 16.0f;
    return { float (i) * w, 0.0f, w, float (getHeight()) };
}

int StepEditor::barAt (float x) const
{
    return juce::jlimit (0, 15, int (x / (float (getWidth()) / 16.0f)));
}

float StepEditor::valueAtY (float y) const
{
    const float h = float (getHeight()) - 8.0f;
    return juce::jlimit (-100.0f, 100.0f, 100.0f - 200.0f * (y - 4.0f) / h);
}

void StepEditor::set (int i, float v)
{
    auto& a = attachments[size_t (i)];
    if (a == nullptr)
        return;
    if (! inGesture[size_t (i)])
    {
        a->beginGesture();
        inGesture[size_t (i)] = true;
    }
    a->setValueAsPartOfGesture (v);
}

void StepEditor::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (colours::well);
    g.fillRoundedRectangle (area, 5.0f);

    const float zeroY = 4.0f + (float (getHeight()) - 8.0f) * 0.5f;
    for (int i = 0; i < 16; ++i)
    {
        auto b = bar (i).reduced (2.0f, 4.0f);
        const bool active = i < last;
        // beat grouping: alternate shading every four steps
        if ((i / 4) % 2 == 1)
        {
            g.setColour (colours::panel.withAlpha (0.55f));
            g.fillRect (bar (i).reduced (0.0f, 2.0f));
        }
        const float v = values[size_t (i)];
        const float y = 4.0f + (float (getHeight()) - 8.0f) * (100.0f - v) / 200.0f;
        auto fill = juce::Rectangle<float>::leftTopRightBottom (b.getX() + 1.0f, std::min (y, zeroY), b.getRight() - 1.0f,
                                                                std::max (y, zeroY) + 0.5f);
        const auto c = active ? colours::seq : colours::textDim.withAlpha (0.35f);
        g.setColour (c.withAlpha (i == playing ? 1.0f : 0.7f));
        g.fillRoundedRectangle (fill, 2.0f);
        // value cap
        g.setColour (active ? colours::text : colours::textDim);
        g.fillRect (juce::Rectangle<float> (b.getX() + 1.0f, y - 1.0f, b.getWidth() - 2.0f, 2.0f));
        if (i == playing)
        {
            g.setColour (colours::seq.brighter (0.4f));
            g.drawRoundedRectangle (bar (i).reduced (1.0f, 2.0f), 3.0f, 1.2f);
        }
        if (! active)
        {
            g.setColour (colours::background.withAlpha (0.45f));
            g.fillRect (bar (i));
        }
    }
    g.setColour (colours::textDim.withAlpha (0.5f));
    g.drawHorizontalLine (int (zeroY), 2.0f, float (getWidth()) - 2.0f);
}

void StepEditor::mouseDown (const juce::MouseEvent& e)
{
    const int i = barAt (e.position.x);
    if (drawMode)
    {
        lastDrawBar = i;
        lastDrawValue = valueAtY (e.position.y);
        set (i, lastDrawValue);
    }
    else
    {
        dragBar = i;
        dragStartValue = values[size_t (i)];
        set (i, dragStartValue);
    }
}

void StepEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (drawMode)
    {
        const int i = barAt (e.position.x);
        const float v = valueAtY (e.position.y);
        // fill every bar crossed since the last event, interpolating the value
        const int from = lastDrawBar < 0 ? i : lastDrawBar;
        const int dir = i >= from ? 1 : -1;
        for (int k = from; k != i + dir; k += dir)
        {
            const float t = (i == from) ? 1.0f : float (k - from) / float (i - from);
            set (k, juce::jmap (t, lastDrawValue, v));
        }
        lastDrawBar = i;
        lastDrawValue = v;
    }
    else if (dragBar >= 0)
    {
        const float scale = 200.0f / std::max (20.0f, float (getHeight()) - 8.0f);
        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        set (dragBar, juce::jlimit (-100.0f, 100.0f, dragStartValue - float (e.getDistanceFromDragStartY()) * scale * fine));
    }
}

void StepEditor::mouseUp (const juce::MouseEvent&)
{
    for (int i = 0; i < 16; ++i)
        if (inGesture[size_t (i)] && attachments[size_t (i)] != nullptr)
        {
            attachments[size_t (i)]->endGesture();
            inGesture[size_t (i)] = false;
        }
    dragBar = -1;
    lastDrawBar = -1;
}

void StepEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = barAt (e.position.x);
    if (attachments[size_t (i)] != nullptr)
        attachments[size_t (i)]->setValueAsCompleteGesture (0.0f);
}

} // namespace craft::mfp::ui
