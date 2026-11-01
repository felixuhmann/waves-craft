#include "Meters.h"

#include "DoublerLookAndFeel.h"

namespace craft::doubler::ui
{

static constexpr float meterFloorDb = -48.0f, meterTopDb = 6.0f;

void PeakBox::setPeak (float db)
{
    if (db > peakDb)
    {
        peakDb = db;
        repaint();
    }
}

void PeakBox::reset()
{
    peakDb = -200.0f;
    repaint();
}

void PeakBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool over = peakDb > 0.0f;
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (over ? colours::warn : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (over ? colours::warn : colours::text);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.drawText (peakDb <= -120.0f ? juce::String ("-inf") : juce::String (peakDb, 1), r, juce::Justification::centred);
}

OutputSection::OutputSection (juce::AudioProcessorValueTreeState& state, const juce::String& masterId) : fader (state, masterId)
{
    addAndMakeVisible (fader);
    for (auto& p : peak)
    {
        p.onReset = [this] { resetAll(); };
        addAndMakeVisible (p);
    }
}

void OutputSection::resetAll()
{
    for (auto& p : peak)
        p.reset();
    clipped = false;
    repaint();
}

void OutputSection::pushPeaks (float l, float r)
{
    const float in[2] = { l, r };
    for (int c = 0; c < 2; ++c)
    {
        const float db = in[c] > 0.0f ? 20.0f * std::log10 (in[c]) : -200.0f;
        levelDb[c] = std::max (db, levelDb[c] - 0.8f); // ~24 dB/s fall at 30 Hz refresh
        peak[c].setPeak (db);
        if (in[c] > 1.0f && ! clipped)
        {
            clipped = true;
            repaint (clipArea());
        }
    }
    repaint (barArea (0).getUnion (barArea (1)).expanded (2));
}

juce::Rectangle<int> OutputSection::clipArea() const
{
    auto r = getLocalBounds().withTrimmedTop (18);
    return r.removeFromTop (16).withSizeKeepingCentre (48, 14);
}

juce::Rectangle<int> OutputSection::barArea (int ch) const
{
    auto r = getLocalBounds().withTrimmedTop (40).withTrimmedBottom (58);
    r.removeFromLeft (getWidth() / 2 + 4);
    const int w = 10;
    return { r.getX() + 10 + ch * (w + 4), r.getY() + 4, w, r.getHeight() - 8 };
}

void OutputSection::resized()
{
    auto r = getLocalBounds().withTrimmedTop (40);
    auto bottom = r.removeFromBottom (54);
    fader.setBounds (r.removeFromLeft (getWidth() / 2 + 4).withTrimmedBottom (-4));
    auto pk = bottom.withTrimmedTop (30).removeFromTop (20);
    const int w = (pk.getWidth() - 6) / 2;
    peak[0].setBounds (pk.removeFromLeft (w));
    pk.removeFromLeft (6);
    peak[1].setBounds (pk);
}

void OutputSection::mouseDown (const juce::MouseEvent& e)
{
    if (clipArea().expanded (4).contains (e.getPosition()))
        resetAll();
}

void OutputSection::paint (juce::Graphics& g)
{
    drawCaption (g, getLocalBounds().removeFromTop (16), "Output");

    // clip indicator
    auto clip = clipArea().toFloat();
    g.setColour (clipped ? colours::warn : colours::well);
    g.fillRoundedRectangle (clip, 3.0f);
    g.setColour (clipped ? colours::warn.brighter (0.3f) : colours::panelEdge);
    g.drawRoundedRectangle (clip, 3.0f, 1.0f);
    g.setColour (clipped ? colours::background : colours::textDim);
    g.setFont (juce::Font (uiFont (9.5f, true)));
    g.drawText ("CLIP", clip, juce::Justification::centred);

    const auto b0 = barArea (0), b1 = barArea (1);
    // scale +6 .. -48 dBFS
    g.setFont (juce::Font (uiFont (9.0f)));
    for (int db = 6; db >= -48; db -= 6)
    {
        const float y = float (b1.getY()) + float (b1.getHeight()) * (meterTopDb - float (db)) / (meterTopDb - meterFloorDb);
        g.setColour (colours::panelEdge.brighter (0.3f));
        g.drawHorizontalLine (int (y), float (b1.getRight() + 3), float (b1.getRight() + 6));
        if (db % 12 == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (db), juce::Rectangle<int> (b1.getRight() + 9, int (y) - 6, 24, 12),
                        juce::Justification::centredLeft);
        }
    }
    for (int c = 0; c < 2; ++c)
    {
        const auto b = (c == 0 ? b0 : b1).toFloat();
        g.setColour (colours::well);
        g.fillRoundedRectangle (b.expanded (2.0f), 3.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (levelDb[c] - meterFloorDb) / (meterTopDb - meterFloorDb));
        juce::ColourGradient grad (colours::accent2, 0.0f, b.getBottom(), colours::warn, 0.0f, b.getY(), false);
        grad.addColour (double (-meterFloorDb / (meterTopDb - meterFloorDb)) - 0.1, colours::accent);
        g.setGradientFill (grad);
        g.fillRect (b.withTop (b.getBottom() - b.getHeight() * frac));
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (9.5f, true)));
        g.drawText (c == 0 ? "L" : "R", juce::Rectangle<float> (b.getX() - 3, b.getBottom() + 3, b.getWidth() + 6, 10),
                    juce::Justification::centred);
    }
    drawCaption (g, getLocalBounds().removeFromBottom (54).removeFromTop (28).withTrimmedTop (12), "Peak", juce::Justification::centred, 9.5f);
}

} // namespace craft::doubler::ui
