#include "Meters.h"

#include "FlangerLookAndFeel.h"

namespace craft::flanger::ui
{

static constexpr float meterTopDb = 6.0f, meterFloorDb = -48.0f;

static float dbToFrac (float db)
{
    return juce::jlimit (0.0f, 1.0f, (db - meterFloorDb) / (meterTopDb - meterFloorDb));
}

void PeakHoldBox::setPeak (float db)
{
    if (db > peakDb)
    {
        peakDb = db;
        repaint();
    }
}

void PeakHoldBox::reset()
{
    peakDb = -200.0f;
    repaint();
}

void PeakHoldBox::mouseDown (const juce::MouseEvent&)
{
    reset();
    if (onReset)
        onReset();
}

void PeakHoldBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (peakDb > 0.0f ? colours::warn : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (peakDb > 0.0f ? colours::warn : colours::text);
    g.setFont (juce::Font (uiFont (11.0f, true)));
    const juce::String text = peakDb <= -120.0f ? juce::String ("-inf") : juce::String (peakDb, 1);
    g.drawText (text, r, juce::Justification::centred);
}

MeterPanel::MeterPanel (int numChannels) : channels (juce::jlimit (1, 2, numChannels))
{
    for (int c = 0; c < channels; ++c)
    {
        hold[c].setTooltip ("Peak hold - click to reset");
        hold[c].onReset = [this, c] {
            clipped[c] = false;
            repaint();
        };
        addAndMakeVisible (hold[c]);
    }
}

void MeterPanel::pushPeaks (float p1, float p2)
{
    const float in[2] = { p1, p2 };
    for (int c = 0; c < channels; ++c)
    {
        const float db = in[c] > 0.0f ? 20.0f * std::log10 (in[c]) : -200.0f;
        levelDb[c] = std::max (db, levelDb[c] - 0.8f); // ~24 dB/s fall at 30 Hz refresh
        hold[c].setPeak (db);
        if (in[c] > 1.0f)
            clipped[c] = true;
    }
    repaint (getLocalBounds().withTrimmedBottom (30));
}

juce::Rectangle<int> MeterPanel::barArea (int ch) const
{
    auto r = getLocalBounds();
    r.removeFromTop (20);    // clip LEDs
    r.removeFromBottom (30); // peak hold boxes
    const int barW = 14;
    const int cx = r.getCentreX() + 10;
    if (channels == 1)
        return { cx - barW / 2, r.getY() + 4, barW, r.getHeight() - 8 };
    return { ch == 0 ? cx - barW - 3 : cx + 3, r.getY() + 4, barW, r.getHeight() - 8 };
}

juce::Rectangle<float> MeterPanel::clipArea (int ch) const
{
    const auto b = barArea (ch).toFloat();
    return { b.getX(), 4.0f, b.getWidth(), 10.0f };
}

void MeterPanel::resized()
{
    auto bottom = getLocalBounds().removeFromBottom (26);
    if (channels == 1)
        hold[0].setBounds (juce::Rectangle<int> (44, 22).withCentre ({ barArea (0).getCentreX(), bottom.getCentreY() }));
    else
    {
        hold[0].setBounds (juce::Rectangle<int> (40, 22).withCentre ({ barArea (0).getCentreX() - 15, bottom.getCentreY() }));
        hold[1].setBounds (juce::Rectangle<int> (40, 22).withCentre ({ barArea (1).getCentreX() + 15, bottom.getCentreY() }));
    }
}

void MeterPanel::mouseDown (const juce::MouseEvent& e)
{
    for (int c = 0; c < channels; ++c)
        if (clipArea (c).expanded (3.0f).contains (e.position))
        {
            clipped[c] = false;
            hold[c].reset();
            repaint();
        }
}

void MeterPanel::paint (juce::Graphics& g)
{
    const auto b0 = barArea (0);
    g.setFont (juce::Font (uiFont (10.0f)));
    for (int db = 6; db >= -48; db -= 6)
    {
        const float y = float (b0.getBottom()) - b0.getHeight() * dbToFrac (float (db));
        g.setColour (colours::panelEdge.brighter (0.3f));
        g.drawHorizontalLine (int (y), float (b0.getX() - 5), float (b0.getX() - 1));
        if (db % 12 == 0 || db == 6)
        {
            g.setColour (db > 0 ? colours::warn.withAlpha (0.8f) : colours::textDim);
            g.drawText (db > 0 ? "+" + juce::String (db) : juce::String (db), juce::Rectangle<int> (b0.getX() - 34, int (y) - 6, 27, 12),
                        juce::Justification::centredRight);
        }
    }

    for (int c = 0; c < channels; ++c)
    {
        const auto b = barArea (c).toFloat();
        g.setColour (colours::well);
        g.fillRoundedRectangle (b.expanded (2.0f), 3.0f);
        const float frac = dbToFrac (levelDb[c]);
        auto fill = b.withTop (b.getBottom() - b.getHeight() * frac);
        juce::ColourGradient grad (colours::accent2, 0.0f, b.getBottom(), colours::warn, 0.0f, b.getY(), false);
        grad.addColour (double (dbToFrac (-6.0f)), colours::accent);
        g.setGradientFill (grad);
        g.fillRect (fill);
        // 0 dBFS line
        g.setColour (colours::warn.withAlpha (0.6f));
        g.drawHorizontalLine (int (b.getBottom() - b.getHeight() * dbToFrac (0.0f)), b.getX(), b.getRight());

        // clip LED
        const auto led = clipArea (c);
        g.setColour (clipped[c] ? colours::warn : colours::well);
        g.fillRoundedRectangle (led, 2.0f);
        g.setColour (clipped[c] ? colours::warn.brighter (0.4f) : colours::panelEdge.brighter (0.2f));
        g.drawRoundedRectangle (led, 2.0f, 1.0f);
    }
}

} // namespace craft::flanger::ui
