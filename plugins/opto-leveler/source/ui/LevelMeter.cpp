#include "LevelMeter.h"

#include "OptoLookAndFeel.h"

namespace craft::opto_leveler::ui
{

namespace
{
constexpr float arcSpan = juce::MathConstants<float>::pi * 0.62f; // total sweep of the needle
constexpr double vuTau = 0.065;                                   // 99 % in ~300 ms
// rectified average -> sine RMS
constexpr double avgToRms = 3.14159265358979323846 / (2.0 * 1.41421356237309504880);
} // namespace

LevelMeter::LevelMeter() { setOpaque (false); }

void LevelMeter::setSource (Source s)
{
    if (s != source)
    {
        source = s;
        repaint();
    }
}

void LevelMeter::push (double inAverage, double outAverage, double grDb, double dt)
{
    const double k = 1.0 - std::exp (-std::max (dt, 0.0) / vuTau);
    inState += (inAverage - inState) * k;
    outState += (outAverage - outState) * k;
    grState += (grDb - grState) * k;
    repaint();
}

double LevelMeter::reading() const
{
    if (source == Source::GR)
        return -grState;
    const double avg = source == Source::In ? inState : outState;
    return 20.0 * std::log10 (std::max (avg * avgToRms, 1.0e-9)) - referenceDbfs;
}

float LevelMeter::valueToAngle (float v) const
{
    // VU-like spacing: compressed at the low end (linear in amplitude between -20 and +3)
    const float lin = std::pow (10.0f, juce::jlimit (minVu, maxVu, v) / 20.0f);
    const float lo = std::pow (10.0f, minVu / 20.0f), hi = std::pow (10.0f, maxVu / 20.0f);
    const float t = (lin - lo) / (hi - lo);
    return -arcSpan * 0.5f + t * arcSpan;
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::well);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    auto area = bounds.reduced (12.0f, 10.0f);
    auto readoutArea = area.removeFromBottom (22.0f);
    // pivot below the visible area so the needle sweeps a shallow arc
    const float radius = std::min (area.getWidth() * 0.62f, area.getHeight() * 1.05f);
    const juce::Point<float> pivot (area.getCentreX(), area.getBottom() + radius * 0.18f);
    const float rScale = radius * 0.92f;

    // scale arc: normal range and the zone above 0
    auto arc = [&] (float v0, float v1, float r, juce::Colour c, float w) {
        juce::Path p;
        p.addCentredArc (pivot.x, pivot.y, r, r, 0.0f, valueToAngle (v0), valueToAngle (v1), true);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };
    arc (minVu, 0.0f, rScale, colours::textDim.withAlpha (0.55f), 1.5f);
    arc (0.0f, maxVu, rScale, colours::warn.withAlpha (0.85f), 3.0f);

    g.setFont (juce::Font (uiFont (10.0f)));
    for (float v : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
    {
        const float a = valueToAngle (v);
        const float s = std::sin (a), c = -std::cos (a);
        const bool major = v == -20.0f || v == -10.0f || v == -5.0f || v == -3.0f || v == 0.0f || v == 3.0f;
        const float r0 = rScale, r1 = rScale + (major ? 8.0f : 5.0f);
        g.setColour (v > 0.0f ? colours::warn : colours::textDim);
        g.drawLine (pivot.x + r0 * s, pivot.y + r0 * c, pivot.x + r1 * s, pivot.y + r1 * c, major ? 1.4f : 1.0f);
        if (major)
        {
            const float rt = rScale + 17.0f;
            const juce::String txt = source == Source::GR ? juce::String (juce::roundToInt (-v))
                                                          : (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v));
            g.drawText (txt, juce::Rectangle<float> (pivot.x + rt * s - 14.0f, pivot.y + rt * c - 7.0f, 28.0f, 14.0f),
                        juce::Justification::centred);
        }
    }

    // caption inside the dial
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f, true)));
    const char* cap = source == Source::GR ? "GAIN REDUCTION dB" : (source == Source::In ? "INPUT VU" : "OUTPUT VU");
    g.drawText (cap, area.withTrimmedTop (area.getHeight() * 0.52f).removeFromTop (16.0f), juce::Justification::centred);

    // needle (clipped to the dial)
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (area.expanded (4.0f, 0.0f).toNearestInt());
        const float v = float (reading());
        const float a = valueToAngle (v);
        const float s = std::sin (a), c = -std::cos (a);
        const float rIn = radius * 0.30f, rOut = rScale + 6.0f;
        const juce::Line<float> needle (pivot.x + rIn * s, pivot.y + rIn * c, pivot.x + rOut * s, pivot.y + rOut * c);
        g.setColour (colours::accent.withAlpha (0.18f));
        g.drawLine (needle, 6.0f);
        g.setColour (colours::accent);
        g.drawLine (needle, 1.8f);
    }

    // digital readout
    const double r = reading();
    juce::String text;
    if (source == Source::GR)
        text = "GR " + juce::String (std::max (0.0, -r), 1) + " dB";
    else
        text = (r >= 0.0 ? "+" : "") + juce::String (std::max (r, -60.0), 1) + " VU";
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (13.0f, true)));
    g.drawText (text, readoutArea, juce::Justification::centred);
}

} // namespace craft::opto_leveler::ui
