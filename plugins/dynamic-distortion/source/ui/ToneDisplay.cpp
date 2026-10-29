#include "ToneDisplay.h"

#include "DistortionLookAndFeel.h"

namespace craft::ddp::ui
{

namespace
{
constexpr double fMin = 20.0, fMax = 20000.0, dbRange = 21.0;
}

double ToneDisplay::responseDb (const dd::Settings& s, double f)
{
    namespace eq = dd::eq;
    const auto mid = eq::midShape (s.midFreq);
    const auto h = eq::bass (s.bass).at (f / eq::bassF0) * eq::mid (s.mid, mid.qn).at (f / mid.centreHz)
                   * eq::treble (s.treble).at (f / eq::trebleF0);
    return 20.0 * std::log10 (std::max (std::abs (h) * dd::post::lpMagnitude (f, s.lowPassHz), 1.0e-9));
}

void ToneDisplay::setSettings (const dd::Settings& s)
{
    const bool same = valid && s.bass == settings.bass && s.mid == settings.mid && s.midFreq == settings.midFreq
                      && s.treble == settings.treble && s.lowPassHz == settings.lowPassHz;
    if (same)
        return;
    settings = s;
    valid = true;
    repaint();
}

void ToneDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 5.0f);
    auto plot = r.reduced (8.0f, 8.0f).withTrimmedBottom (10.0f);

    auto xOf = [&] (double f) { return plot.getX() + float (std::log (f / fMin) / std::log (fMax / fMin)) * plot.getWidth(); };
    auto yOf = [&] (double db) {
        return plot.getCentreY() - float (juce::jlimit (-dbRange, dbRange, db) / dbRange) * plot.getHeight() * 0.5f;
    };

    // grid
    g.setFont (juce::Font (uiFont (9.0f)));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xOf (f);
        g.setColour (colours::grid);
        g.fillRect (juce::Rectangle<float> (x, plot.getY(), 1.0f, plot.getHeight()));
        g.setColour (colours::textDim.withAlpha (0.7f));
        const auto label = f >= 1000.0 ? juce::String (int (f / 1000.0)) + "k" : juce::String (int (f));
        g.drawText (label, juce::Rectangle<float> (x - 14.0f, plot.getBottom() + 1.0f, 28.0f, 10.0f), juce::Justification::centred);
    }
    for (double db : { -18.0, -12.0, -6.0, 0.0, 6.0, 12.0, 18.0 })
    {
        g.setColour (db == 0.0 ? colours::panelEdge.brighter (0.3f) : colours::grid);
        g.fillRect (juce::Rectangle<float> (plot.getX(), yOf (db), plot.getWidth(), 1.0f));
    }

    if (! valid)
        return;

    juce::Path curve;
    const int n = std::max (2, int (plot.getWidth()));
    for (int i = 0; i <= n; ++i)
    {
        const double f = fMin * std::pow (fMax / fMin, double (i) / n);
        const juce::Point<float> p (xOf (f), yOf (responseDb (settings, f)));
        if (i == 0)
            curve.startNewSubPath (p);
        else
            curve.lineTo (p);
    }
    juce::Path fill (curve);
    fill.lineTo (plot.getRight(), yOf (0.0));
    fill.lineTo (plot.getX(), yOf (0.0));
    fill.closeSubPath();
    g.setColour (colours::tone.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (colours::tone);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // markers: mid frequency and LP corner
    auto marker = [&] (double f, juce::Colour c) {
        const float x = xOf (juce::jlimit (fMin, fMax, f));
        g.setColour (c);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ x, yOf (responseDb (settings, f)) }));
    };
    marker (settings.midFreq, colours::text);
    marker (settings.lowPassHz, colours::heatHot);

    g.setColour (colours::textDim);
    g.drawText ("dB", plot.withHeight (10.0f).translated (2.0f, 0.0f), juce::Justification::topLeft);
}

} // namespace craft::ddp::ui
