#include "Meters.h"

#include "CraftLookAndFeel.h"

namespace craft::ui
{

static constexpr float meterFloorDb = -24.0f;

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

void PeakHoldBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (peakDb > 0.0f ? colours::warn : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (peakDb > 0.0f ? colours::warn : colours::text);
    g.setFont (juce::Font (uiFont (11.5f, true)));
    const juce::String text = peakDb <= -120.0f ? juce::String ("-inf") : juce::String (peakDb, 1);
    g.drawText (text, r, juce::Justification::centred);
}

MeterPanel::MeterPanel()
{
    for (auto& h : hold)
    {
        h.setTooltip ("Peak hold - click to reset");
        addAndMakeVisible (h);
    }
    modeSwitch.onClick = [this] {
        if (onModeChange)
            onModeChange (modeSwitch.getToggleState());
        for (auto& h : hold)
            h.reset();
        repaint();
    };
    addAndMakeVisible (modeSwitch);
}

void MeterPanel::setMode (bool ms)
{
    modeSwitch.setToggleState (ms, juce::dontSendNotification);
    repaint();
}

void MeterPanel::pushPeaks (float p1, float p2)
{
    const float in[2] = { p1, p2 };
    for (int c = 0; c < 2; ++c)
    {
        const float db = in[c] > 0.0f ? 20.0f * std::log10 (in[c]) : -200.0f;
        levelDb[c] = std::max (db, levelDb[c] - 0.8f); // ~24 dB/s fall at 30 Hz refresh
        hold[c].setPeak (db);
    }
    repaint (barArea (0).getUnion (barArea (1)).expanded (2));
}

juce::Rectangle<int> MeterPanel::barArea (int ch) const
{
    auto r = getLocalBounds();
    r.removeFromTop (34);
    r.removeFromBottom (64);
    const int barW = 16;
    const int cx = r.getCentreX() + 8;
    return { ch == 0 ? cx - barW - 4 : cx + 4, r.getY() + 6, barW, r.getHeight() - 12 };
}

void MeterPanel::resized()
{
    auto r = getLocalBounds();
    auto bottom = r.removeFromBottom (64);
    auto holds = bottom.removeFromTop (26).reduced (0, 2);
    const auto b0 = barArea (0), b1 = barArea (1);
    hold[0].setBounds (juce::Rectangle<int> (40, 22).withCentre ({ b0.getCentreX() - 14, holds.getCentreY() }));
    hold[1].setBounds (juce::Rectangle<int> (40, 22).withCentre ({ b1.getCentreX() + 14, holds.getCentreY() }));
    bottom.removeFromTop (16);
    modeSwitch.setBounds (bottom.removeFromTop (22).withSizeKeepingCentre (96, 22));
}

void MeterPanel::paint (juce::Graphics& g)
{
    const bool ms = modeSwitch.getToggleState();
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (12.0f, true)));
    g.drawText ("OUTPUT", getLocalBounds().removeFromTop (18), juce::Justification::centred);

    auto bottom = getLocalBounds().removeFromBottom (64);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.drawText ("METER MODE", bottom.withTrimmedTop (26).removeFromTop (16), juce::Justification::centred);

    const auto b0 = barArea (0), b1 = barArea (1);
    // scale 0 .. -24 dB
    g.setFont (juce::Font (uiFont (10.0f)));
    for (int db = 0; db >= -24; db -= 3)
    {
        const float y = b0.getY() + b0.getHeight() * float (db) / meterFloorDb;
        g.setColour (colours::panelEdge.brighter (0.3f));
        g.drawHorizontalLine (int (y), float (b0.getX() - 5), float (b0.getX() - 1));
        if (db % 6 == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (db), juce::Rectangle<int> (b0.getX() - 36, int (y) - 6, 28, 12),
                        juce::Justification::centredRight);
        }
    }

    for (int c = 0; c < 2; ++c)
    {
        const auto b = (c == 0 ? b0 : b1).toFloat();
        g.setColour (colours::well);
        g.fillRoundedRectangle (b.expanded (2.0f), 3.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, 1.0f - levelDb[c] / meterFloorDb);
        auto fill = b.withTop (b.getBottom() - b.getHeight() * frac);
        juce::ColourGradient grad (colours::accent2, 0.0f, b.getBottom(), colours::warn, 0.0f, b.getY(), false);
        grad.addColour (0.75, colours::accent);
        g.setGradientFill (grad);
        g.fillRect (fill);

        g.setColour (colours::text);
        g.setFont (juce::Font (uiFont (11.0f, true)));
        g.drawText (ms ? (c == 0 ? "M" : "S") : (c == 0 ? "L" : "R"),
                    juce::Rectangle<float> (b.getX() - 4, b.getY() - 16, b.getWidth() + 8, 12),
                    juce::Justification::centred);
    }
}

} // namespace craft::ui
