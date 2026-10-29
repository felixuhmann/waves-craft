#include "LfoDisplay.h"

#include "FlangerLookAndFeel.h"

namespace craft::flanger::ui
{

namespace
{
float waveValue (float phase, bool triangle)
{
    phase -= std::floor (phase);
    if (triangle)
        return phase < 0.25f ? 4 * phase : (phase < 0.75f ? 2 - 4 * phase : 4 * phase - 4);
    return std::sin (juce::MathConstants<float>::twoPi * phase);
}
} // namespace

void LfoDisplay::setState (const State& s)
{
    const bool changed = s.triangle != st.triangle || s.stopped != st.stopped || s.staticLfo != st.staticLfo
                         || s.channels != st.channels || s.phase[0] != st.phase[0] || s.phase[1] != st.phase[1]
                         || s.value[0] != st.value[0] || s.value[1] != st.value[1];
    st = s;
    if (changed)
        repaint();
}

void LfoDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

    auto inner = r.reduced (10.0f, 8.0f);
    auto bar = inner.removeFromBottom (16.0f);
    inner.removeFromBottom (8.0f);
    auto curve = inner;

    // grid
    g.setColour (colours::panelEdge);
    g.drawHorizontalLine (int (curve.getCentreY()), curve.getX(), curve.getRight());
    for (int i = 1; i < 4; ++i)
    {
        const float x = curve.getX() + curve.getWidth() * float (i) / 4.0f;
        g.drawVerticalLine (int (x), curve.getY(), curve.getBottom());
    }

    // one LFO cycle
    juce::Path p;
    const int steps = 96;
    auto pointAt = [&] (float phase) {
        return juce::Point<float> (curve.getX() + phase * curve.getWidth(),
                                   curve.getCentreY() - waveValue (phase, st.triangle) * curve.getHeight() * 0.45f);
    };
    for (int i = 0; i <= steps; ++i)
    {
        const auto pt = pointAt (float (i) / steps);
        if (i == 0)
            p.startNewSubPath (pt);
        else
            p.lineTo (pt);
    }
    g.setColour (colours::textDim.withAlpha (0.8f));
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // channel markers on the curve
    const juce::Colour chColour[2] = { colours::accent, colours::accent2 };
    for (int ch = st.channels - 1; ch >= 0; --ch)
    {
        const auto pt = pointAt (st.phase[ch]);
        g.setColour (chColour[ch].withAlpha (0.25f));
        g.drawVerticalLine (int (pt.x), curve.getY(), curve.getBottom());
        g.setColour (chColour[ch]);
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (pt));
        g.setColour (colours::well);
        g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (pt), 1.0f);
    }

    // sweep bar: modulation value -1 (short delay) ... +1 (long delay)
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (colours::panelEdge.brighter (0.3f));
    g.drawVerticalLine (int (bar.getCentreX()), bar.getY() + 2, bar.getBottom() - 2);
    for (int ch = 0; ch < st.channels; ++ch)
    {
        const float x = bar.getX() + 6.0f + (st.value[ch] * 0.5f + 0.5f) * (bar.getWidth() - 12.0f);
        auto m = juce::Rectangle<float> (12.0f, bar.getHeight() - 4.0f).withCentre ({ x, bar.getCentreY() });
        g.setColour (chColour[ch]);
        g.fillRoundedRectangle (m, 2.0f);
        g.setColour (colours::background);
        g.setFont (juce::Font (uiFont (9.5f, true)));
        g.drawText (st.channels == 1 ? "M" : (ch == 0 ? "L" : "R"), m, juce::Justification::centred);
    }

    if (st.stopped || st.staticLfo)
    {
        g.setColour (colours::text.withAlpha (0.85f));
        g.setFont (juce::Font (uiFont (10.5f, true)));
        g.drawText (st.stopped ? "STOPPED" : "STATIC", curve.reduced (4.0f), juce::Justification::topRight);
    }
}

// ---------------------------------------------------------------------------------------------
void StereoPhaseIndicator::setOffsetDegrees (float deg)
{
    if (deg != offsetDeg)
    {
        offsetDeg = deg;
        repaint();
    }
}

void StereoPhaseIndicator::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float size = std::min (b.getWidth(), b.getHeight()) - 2.0f;
    const auto circle = juce::Rectangle<float> (size, size).withCentre (b.getCentre());
    const auto c = circle.getCentre();
    const float rad = size / 2.0f;

    g.setColour (colours::well);
    g.fillEllipse (circle);
    g.setColour (colours::panelEdge.brighter (0.2f));
    g.drawEllipse (circle, 1.0f);

    // lead arc from L (12 o'clock) clockwise to R
    const float a = juce::degreesToRadians (offsetDeg);
    if (a > 0.0f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rad * 0.62f, rad * 0.62f, 0.0f, 0.0f, a, true);
        g.setColour (colours::accent2.withAlpha (0.45f));
        g.strokePath (arc, juce::PathStrokeType (2.5f));
    }
    auto hand = [&] (float angle, juce::Colour col) {
        const juce::Point<float> tip (c.x + std::sin (angle) * rad * 0.85f, c.y - std::cos (angle) * rad * 0.85f);
        g.setColour (col);
        g.drawLine ({ c, tip }, 2.0f);
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (tip));
    };
    hand (0.0f, colours::accent);
    hand (a, colours::accent2);
    g.setColour (colours::text);
    g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (c));
}

} // namespace craft::flanger::ui
