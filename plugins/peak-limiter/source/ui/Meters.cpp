#include "Meters.h"

#include "LimiterLookAndFeel.h"

namespace craft::limiter::ui
{

namespace
{
constexpr float rangeDb = 30.0f;
constexpr int readoutH = 24, titleH = 22, labelH = 18;
} // namespace

GainReductionMeter::GainReductionMeter()
{
    setTooltip ("Gain reduction - click to reset the peak readout");
}

juce::Rectangle<float> GainReductionMeter::barArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (float (titleH) + 8.0f);
    r.removeFromBottom (float (readoutH + labelH) + 8.0f);
    return r.withSizeKeepingCentre (22.0f, r.getHeight()).translated (10.0f, 0.0f);
}

juce::Rectangle<float> GainReductionMeter::readoutArea() const
{
    return getLocalBounds().removeFromBottom (readoutH).toFloat().withSizeKeepingCentre (64.0f, float (readoutH));
}

float GainReductionMeter::yForGr (float gr) const
{
    const auto b = barArea();
    return b.getY() + b.getHeight() * juce::jlimit (0.0f, 1.0f, gr / rangeDb);
}

void GainReductionMeter::pushGain (float minGain)
{
    const float gr = minGain < 1.0f ? -20.0f * std::log10 (std::max (minGain, 1.0e-6f)) : 0.0f;
    grDb = std::max (gr, grDb - 1.0f); // instant rise, ~30 dB/s return at 30 Hz refresh
    if (grDb < 0.05f)
        grDb = 0.0f;
    if (gr > peakGrDb)
        peakGrDb = gr;
    repaint();
}

void GainReductionMeter::resetPeak()
{
    peakGrDb = 0.0f;
    repaint();
}

void GainReductionMeter::mouseDown (const juce::MouseEvent&) { resetPeak(); }

void GainReductionMeter::paint (juce::Graphics& g)
{
    drawTitle (g, getLocalBounds().removeFromTop (18), "Atten");

    const auto b = barArea();
    // scale 0 ... 30 dB
    g.setFont (juce::Font (uiFont (10.5f)));
    for (int db = 0; db <= 30; db += 3)
    {
        const float y = yForGr (float (db));
        g.setColour (colours::panelEdge.brighter (0.35f));
        g.drawHorizontalLine (int (y), b.getX() - 7.0f, b.getX() - 3.0f);
        if (db % 6 == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (db), juce::Rectangle<float> (b.getX() - 40.0f, y - 7.0f, 30.0f, 14.0f),
                        juce::Justification::centredRight);
        }
    }

    g.setColour (colours::well);
    g.fillRoundedRectangle (b.expanded (2.0f), 3.0f);
    if (grDb > 0.0f)
    {
        const auto fill = b.withBottom (yForGr (grDb));
        juce::ColourGradient grad (colours::accent, 0.0f, b.getY(), colours::warn, 0.0f, b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (fill);
    }
    if (peakGrDb > 0.05f)
    {
        g.setColour (colours::text.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (b.getX(), yForGr (peakGrDb) - 1.0f, b.getWidth(), 2.0f));
    }

    // label + peak readout
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.drawText ("PEAK GR", getLocalBounds().withTrimmedBottom (readoutH + 2).removeFromBottom (labelH),
                juce::Justification::centredBottom);

    const auto rr = readoutArea().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (rr, 3.0f);
    g.setColour (peakGrDb > 0.05f ? colours::warn.withAlpha (0.8f) : colours::panelEdge);
    g.drawRoundedRectangle (rr, 3.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (13.0f, true)));
    const juce::String text = peakGrDb > 0.05f ? "-" + juce::String (peakGrDb, 1) : juce::String ("0.0");
    g.drawText (text, rr, juce::Justification::centred);
}

} // namespace craft::limiter::ui
