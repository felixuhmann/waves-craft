#include "NeedleMeter.h"

#include <cmath>

#include "FetLookAndFeel.h"

namespace craft::fet_limiter::ui
{

namespace
{
// Meter scale: -20 .. +3 "meter dB". GR mode shows -GR (rest at 0); level modes show peak dBFS + 18
// (0 on the scale = -18 dBFS).
constexpr float scaleMin = -20.0f, scaleMax = 3.0f;
constexpr float sweep = 0.95f; // half the needle sweep in radians
constexpr float levelReferenceDbfs = -18.0f;
} // namespace

float NeedleMeter::scaleToNorm (float db)
{
    // slightly compressed towards the bottom, like a classic moving-coil scale: sqrt-ish law on the dB range
    const float t = juce::jlimit (0.0f, 1.0f, (db - scaleMin) / (scaleMax - scaleMin));
    return std::pow (t, 1.6f);
}

void NeedleMeter::setMode (Mode m)
{
    mode = m;
    repaint();
}

void NeedleMeter::setAccent (juce::Colour c)
{
    accent = c;
    repaint();
}

void NeedleMeter::update (float value, float overPeak, float dt)
{
    float targetDb;
    if (mode == Mode::GainReduction)
        targetDb = -value;
    else
        targetDb = (value > 1.0e-6f ? 20.0f * std::log10 (value) : -120.0f) - levelReferenceDbfs;
    targetDb = juce::jlimit (scaleMin - 1.0f, scaleMax + 0.5f, targetDb);
    // needle ballistics: ~300 ms integration
    const float k = 1.0f - std::exp (-dt / 0.09f);
    needleDb += (targetDb - needleDb) * k;
    if (overPeak >= 1.0f)
        overHold = 0.6f;
    else
        overHold = std::max (0.0f, overHold - dt);
    repaint();
}

void NeedleMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // face
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 6.0f);
    auto face = r.reduced (6.0f);
    juce::ColourGradient grad (juce::Colour (0xff23282e), face.getCentreX(), face.getY(), juce::Colour (0xff171a1e), face.getCentreX(),
                               face.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (face, 4.0f);

    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.32f);
    const float radius = face.getHeight() * 1.05f;
    auto angleFor = [] (float db) { return -sweep + 2.0f * sweep * scaleToNorm (db); };

    // scale arc: normal range in dim text colour, the range above 0 in the accent
    juce::Path arcLow, arcHigh;
    arcLow.addCentredArc (pivot.x, pivot.y, radius * 0.86f, radius * 0.86f, 0.0f, angleFor (scaleMin), angleFor (0.0f), true);
    arcHigh.addCentredArc (pivot.x, pivot.y, radius * 0.86f, radius * 0.86f, 0.0f, angleFor (0.0f), angleFor (scaleMax), true);
    g.setColour (colours::textDim);
    g.strokePath (arcLow, juce::PathStrokeType (1.5f));
    g.setColour (accent);
    g.strokePath (arcHigh, juce::PathStrokeType (3.0f));

    g.setFont (juce::Font (uiFont (10.5f, true)));
    for (float db : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
    {
        const float a = angleFor (db);
        const juce::Point<float> dir (std::sin (a), -std::cos (a));
        const bool major = db == -20.0f || db == -10.0f || db == -5.0f || db == -3.0f || db == 0.0f || db == 3.0f || db == -7.0f
                           || db == -1.0f || db == 1.0f || db == 2.0f || db == -2.0f;
        g.setColour (db > 0.0f ? accent : colours::text.withAlpha (0.8f));
        g.drawLine ({ pivot + dir * (radius * 0.86f), pivot + dir * (radius * (major ? 0.93f : 0.90f)) }, 1.3f);
        const auto tp = pivot + dir * (radius * 0.99f);
        juce::String label = db > 0.0f ? "+" + juce::String (int (db)) : juce::String (int (db));
        g.drawText (label, juce::Rectangle<float> (26.0f, 12.0f).withCentre (tp), juce::Justification::centred);
    }

    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.0f, true)));
    g.drawText (mode == Mode::GainReduction ? "GAIN REDUCTION dB" : "LEVEL dB (0 = -18 dBFS)",
                face.withTrimmedTop (face.getHeight() * 0.52f).withHeight (14.0f), juce::Justification::centred);

    // needle (clipped to the face)
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (face.toNearestInt());
        const float a = juce::jlimit (-sweep - 0.06f, sweep + 0.06f, angleFor (needleDb));
        const juce::Point<float> dir (std::sin (a), -std::cos (a));
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawLine ({ pivot.translated (2.0f, 2.0f), (pivot + dir * (radius * 0.95f)).translated (2.0f, 2.0f) }, 2.0f);
        g.setColour (colours::text);
        g.drawLine ({ pivot, pivot + dir * (radius * 0.95f) }, 1.6f);
    }
    // pivot cover
    g.setColour (colours::panel);
    g.fillRect (face.withTop (face.getBottom() - face.getHeight() * 0.16f));
    g.setColour (colours::panelEdge);
    g.drawLine (face.getX(), face.getBottom() - face.getHeight() * 0.16f, face.getRight(), face.getBottom() - face.getHeight() * 0.16f, 1.0f);

    // over LED (top-right corner)
    const auto led = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ face.getRight() - 12.0f, face.getY() + 12.0f });
    if (overHold > 0.0f)
    {
        g.setColour (colours::warn.withAlpha (0.35f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (overHold > 0.0f ? colours::warn : colours::warn.darker (0.8f).withAlpha (0.6f));
    g.fillEllipse (led);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (9.0f, true)));
    g.drawText ("OVER", juce::Rectangle<float> (40.0f, 10.0f).withCentre ({ led.getCentreX() - 26.0f, led.getCentreY() }),
                juce::Justification::centredRight);
}

} // namespace craft::fet_limiter::ui
