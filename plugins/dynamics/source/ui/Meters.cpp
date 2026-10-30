#include "Meters.h"

#include "DynamicsLookAndFeel.h"

namespace craft::dynamics::ui
{

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
    const bool over = warnAboveZero && peakDb > 0.0f;
    g.setColour (over ? colours::warn : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (over ? colours::warn : colours::text);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.drawText (peakDb <= -120.0f ? juce::String ("-inf") : juce::String (peakDb, 1), r, juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------------
static constexpr float gainRangeDb = 24.0f;

ModuleMeters::ModuleMeters (juce::Colour c) : colour (c)
{
    gainHold.setTooltip ("Largest gain reduction (dB) - click to reset");
    gainHold.setWarnAboveZero (false);
    addAndMakeVisible (gainHold);
}

void ModuleMeters::setValues (float gainDb, float levelDb)
{
    if (std::abs (gainDb - gain) > 0.01f || std::abs (levelDb - level) > 0.05f)
    {
        gain = gainDb;
        level = levelDb;
        repaint (getLocalBounds().withTrimmedBottom (26));
    }
    gainHold.setPeak (-gainDb); // shows the deepest reduction as a positive number
}

void ModuleMeters::resized()
{
    gainHold.setBounds (getLocalBounds().removeFromBottom (20).withSizeKeepingCentre (getWidth() - 4, 20));
}

void ModuleMeters::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().withTrimmedBottom (26).toFloat();
    auto captions = r.removeFromTop (14.0f);
    g.setFont (juce::Font (uiFont (9.5f, true)));
    g.setColour (colours::textDim);
    const float half = r.getWidth() / 2.0f;
    g.drawText ("GAIN", captions.removeFromLeft (half), juce::Justification::centred);
    g.drawText ("LEVEL", captions, juce::Justification::centred);

    auto gainBar = r.removeFromLeft (half).withSizeKeepingCentre (10.0f, r.getHeight() - 4.0f);
    auto levelBar = r.withSizeKeepingCentre (10.0f, r.getHeight() - 4.0f);
    for (auto b : { gainBar, levelBar })
    {
        g.setColour (colours::well);
        g.fillRoundedRectangle (b.expanded (2.0f), 3.0f);
    }

    // gain: centre = 0 dB, reduction downward, increase upward
    const float zeroY = gainBar.getCentreY();
    const float gy = zeroY - juce::jlimit (-1.0f, 1.0f, gain / gainRangeDb) * gainBar.getHeight() / 2.0f;
    g.setColour (gain < 0.0f ? colour : colour.brighter (0.5f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (gainBar.getX(), std::min (zeroY, gy), gainBar.getRight(), std::max (zeroY, gy)));
    g.setColour (colours::textDim.withAlpha (0.6f));
    for (float db : { -gainRangeDb, -12.0f, 0.0f, 12.0f, gainRangeDb })
    {
        const float y = zeroY - db / gainRangeDb * gainBar.getHeight() / 2.0f;
        g.drawHorizontalLine (int (y), gainBar.getX() - 4.0f, gainBar.getX() - 1.0f);
    }

    // detector level: 0 ... -100 dB
    const float frac = juce::jlimit (0.0f, 1.0f, (level + 100.0f) / 100.0f);
    g.setColour (colour.withAlpha (0.75f));
    g.fillRect (levelBar.withTop (levelBar.getBottom() - levelBar.getHeight() * frac));
    g.setColour (colours::textDim.withAlpha (0.6f));
    for (int db = 0; db >= -100; db -= 20)
    {
        const float y = levelBar.getBottom() - levelBar.getHeight() * (float (db) + 100.0f) / 100.0f;
        g.drawHorizontalLine (int (y), levelBar.getRight() + 1.0f, levelBar.getRight() + 4.0f);
    }
}

// ---------------------------------------------------------------------------------------------
static constexpr float meterTopDb = 6.0f, meterFloorDb = -60.0f;

static float dbToFrac (float db)
{
    return juce::jlimit (0.0f, 1.0f, (db - meterFloorDb) / (meterTopDb - meterFloorDb));
}

IoMeters::IoMeters (int n) : channels (juce::jlimit (1, 2, n))
{
    for (int i = 0; i < numBars(); ++i)
    {
        hold[i].setTooltip ("Peak - click to reset");
        addAndMakeVisible (hold[i]);
    }
}

void IoMeters::push (const float* in, const float* out)
{
    for (int i = 0; i < numBars(); ++i)
    {
        const float lin = i < channels ? in[i] : out[i - channels];
        const float db = lin > 0.0f ? 20.0f * std::log10 (lin) : -200.0f;
        levelDb[i] = std::max (db, levelDb[i] - 0.8f);
        hold[i].setPeak (db);
        if (lin > 1.0f)
            clipped[i] = true;
    }
    repaint (getLocalBounds().withTrimmedBottom (26));
}

juce::Rectangle<int> IoMeters::barArea (int i) const
{
    auto r = getLocalBounds();
    r.removeFromTop (30);    // captions + clip LEDs
    r.removeFromBottom (28); // peak boxes
    const int groupW = r.getWidth() / 2;
    auto group = (i < channels) ? r.removeFromLeft (groupW) : r.withTrimmedLeft (groupW);
    group.removeFromLeft (24); // scale
    const int ch = i % channels;
    const int barW = 10;
    const int x = group.getCentreX() + (channels == 1 ? -barW / 2 : (ch == 0 ? -barW - 2 : 2));
    return { x, group.getY() + 2, barW, group.getHeight() - 4 };
}

void IoMeters::resized()
{
    // peak boxes: evenly spread over the full width, in bar order
    auto bottom = getLocalBounds().removeFromBottom (22).withSizeKeepingCentre (getWidth(), 20);
    const int gap = 3, w = (bottom.getWidth() - gap * (numBars() - 1)) / numBars();
    for (int i = 0; i < numBars(); ++i)
    {
        hold[i].setBounds (bottom.removeFromLeft (w));
        bottom.removeFromLeft (gap);
    }
}

void IoMeters::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < numBars(); ++i)
        if (barArea (i).toFloat().withY (14.0f).withHeight (14.0f).expanded (3.0f).contains (e.position))
        {
            clipped[i] = false;
            hold[i].reset();
            repaint();
        }
}

void IoMeters::paint (juce::Graphics& g)
{
    g.setFont (juce::Font (uiFont (9.5f, true)));
    g.setColour (colours::textDim);
    for (int grp = 0; grp < 2; ++grp)
    {
        const auto b0 = barArea (grp * channels), b1 = barArea (grp * channels + channels - 1);
        g.drawText (grp == 0 ? "IN" : "OUT", juce::Rectangle<int> (b0.getX() - 10, 0, b1.getRight() - b0.getX() + 20, 12), juce::Justification::centred);
    }
    g.setFont (juce::Font (uiFont (9.0f)));
    for (int grp = 0; grp < 2; ++grp)
    {
        const auto b0 = barArea (grp * channels);
        for (int db = 6; db >= -60; db -= 6)
        {
            const float y = float (b0.getBottom()) - float (b0.getHeight()) * dbToFrac (float (db));
            g.setColour (colours::panelEdge.brighter (0.3f));
            g.drawHorizontalLine (int (y), float (b0.getX() - 5), float (b0.getX() - 1));
            if (db % 12 == 0 || db == 6)
            {
                g.setColour (db > 0 ? colours::warn.withAlpha (0.8f) : colours::textDim);
                g.drawText (db > 0 ? "+" + juce::String (db) : juce::String (db), juce::Rectangle<int> (b0.getX() - 28, int (y) - 6, 22, 12),
                            juce::Justification::centredRight);
            }
        }
    }
    for (int i = 0; i < numBars(); ++i)
    {
        const auto b = barArea (i).toFloat();
        g.setColour (colours::well);
        g.fillRoundedRectangle (b.expanded (2.0f), 3.0f);
        juce::ColourGradient grad (colours::accent2, 0.0f, b.getBottom(), colours::warn, 0.0f, b.getY(), false);
        grad.addColour (double (dbToFrac (-6.0f)), colours::accent);
        g.setGradientFill (grad);
        g.fillRect (b.withTop (b.getBottom() - b.getHeight() * dbToFrac (levelDb[i])));
        g.setColour (colours::warn.withAlpha (0.6f));
        g.drawHorizontalLine (int (b.getBottom() - b.getHeight() * dbToFrac (0.0f)), b.getX(), b.getRight());

        const auto led = juce::Rectangle<float> (b.getX(), 15.0f, b.getWidth(), 9.0f);
        g.setColour (clipped[i] ? colours::warn : colours::well);
        g.fillRoundedRectangle (led, 2.0f);
        g.setColour (colours::panelEdge.brighter (0.2f));
        g.drawRoundedRectangle (led, 2.0f, 1.0f);
    }
}

} // namespace craft::dynamics::ui
