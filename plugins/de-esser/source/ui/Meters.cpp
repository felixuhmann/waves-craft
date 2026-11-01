#include "Meters.h"

#include "LookAndFeel.h"

namespace craft::deesser::ui
{

// ---------------------------------------------------------------------------------------------
PeakReadout::PeakReadout (bool reductionStyle) : reduction (reductionStyle), peakDb (reductionStyle ? 0.0f : -200.0f)
{
    setTooltip ("Peak hold - click to reset");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PeakReadout::push (float db)
{
    if (reduction ? db < peakDb : db > peakDb)
    {
        peakDb = db;
        repaint();
    }
}

void PeakReadout::reset()
{
    peakDb = reduction ? 0.0f : -200.0f;
    repaint();
}

void PeakReadout::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool over = ! reduction && peakDb > 0.0f;
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (over ? colours::warn : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (over ? colours::warn : colours::text);
    g.setFont (juce::Font (uiFont (11.5f, true)));
    juce::String text;
    if (reduction)
        text = peakDb > -0.05f ? juce::String ("0.0") : juce::String (peakDb, 1);
    else
        text = peakDb <= -120.0f ? juce::String ("-inf") : juce::String (peakDb, 1);
    g.drawText (text, r, juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------------
BarMeter::BarMeter (int numBars, bool reductionStyle, std::function<float (float)> toFraction, std::vector<MeterScale> sc)
    : bars (numBars), reduction (reductionStyle), toFrac (std::move (toFraction)), scale (std::move (sc)),
      levels (size_t (numBars), reductionStyle ? 0.0f : -200.0f)
{
    setInterceptsMouseClicks (false, false);
}

void BarMeter::setLevels (const float* db)
{
    bool changed = false;
    for (int i = 0; i < bars; ++i)
    {
        // display ballistics: instant rise, ~26 dB/s fall at 30 Hz refresh (reduction: instant deepen)
        float v = levels[size_t (i)];
        if (reduction)
            v = db[i] > v ? db[i] : std::max (db[i], v - 0.85f);
        else
            v = db[i] > v ? db[i] : std::max (db[i], v - 0.85f);
        changed = changed || v != levels[size_t (i)];
        levels[size_t (i)] = v;
    }
    if (changed)
        repaint();
}

void BarMeter::setZone (float fromFraction, float toFraction, juce::Colour tint)
{
    zoneFrom = fromFraction;
    zoneTo = toFraction;
    zoneTint = tint;
}

juce::Rectangle<int> BarMeter::getBarsArea() const
{
    auto r = getLocalBounds().reduced (0, 8);
    if (barLabels.size() > 0)
        r.removeFromTop (14);
    const int scaleW = 30;
    if (scaleLeft)
        r.removeFromLeft (scaleW);
    else
        r.removeFromRight (scaleW);
    const int barW = 12, gap = 4;
    const int w = bars * barW + (bars - 1) * gap;
    return r.withSizeKeepingCentre (w, r.getHeight());
}

void BarMeter::paint (juce::Graphics& g)
{
    const auto area = getBarsArea();
    const auto af = area.toFloat();

    // scale
    g.setFont (juce::Font (uiFont (10.0f)));
    for (const auto& m : scale)
    {
        const float f = juce::jlimit (0.0f, 1.0f, toFrac (m.value));
        const float y = reduction ? af.getY() + f * af.getHeight() : af.getBottom() - f * af.getHeight();
        g.setColour (colours::panelEdge.brighter (0.35f));
        if (scaleLeft)
            g.drawHorizontalLine (int (y), af.getX() - 6.0f, af.getX() - 2.0f);
        else
            g.drawHorizontalLine (int (y), af.getRight() + 2.0f, af.getRight() + 6.0f);
        g.setColour (colours::textDim);
        const auto box = scaleLeft ? juce::Rectangle<float> (af.getX() - 34.0f, y - 6.0f, 26.0f, 12.0f)
                                   : juce::Rectangle<float> (af.getRight() + 8.0f, y - 6.0f, 26.0f, 12.0f);
        g.drawText (m.text, box, scaleLeft ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }

    const int barW = 12, gap = 4;
    for (int i = 0; i < bars; ++i)
    {
        const auto b = juce::Rectangle<float> (af.getX() + float (i * (barW + gap)), af.getY(), float (barW), af.getHeight());
        g.setColour (colours::well);
        g.fillRoundedRectangle (b.expanded (2.0f), 3.0f);
        if (zoneTo > zoneFrom)
        {
            g.setColour (zoneTint);
            const float y0 = reduction ? b.getY() + zoneFrom * b.getHeight() : b.getBottom() - zoneTo * b.getHeight();
            g.fillRect (b.withY (y0).withHeight ((zoneTo - zoneFrom) * b.getHeight()));
        }

        const float f = juce::jlimit (0.0f, 1.0f, toFrac (levels[size_t (i)]));
        if (reduction)
        {
            auto fill = b.withHeight (b.getHeight() * f);
            juce::ColourGradient grad (colours::accent, 0.0f, b.getY(), colours::warn, 0.0f, b.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRect (fill);
        }
        else
        {
            auto fill = b.withTop (b.getBottom() - b.getHeight() * f);
            juce::ColourGradient grad (colours::accent2, 0.0f, b.getBottom(), colours::warn, 0.0f, b.getY(), false);
            grad.addColour (0.8, colours::accent);
            g.setGradientFill (grad);
            g.fillRect (fill);
        }

        if (i < barLabels.size())
        {
            g.setColour (colours::text);
            g.setFont (juce::Font (uiFont (11.0f, true)));
            g.drawText (barLabels[i], juce::Rectangle<float> (b.getX() - 4.0f, b.getY() - 18.0f, b.getWidth() + 8.0f, 12.0f),
                        juce::Justification::centred);
        }
    }
}

} // namespace craft::deesser::ui
