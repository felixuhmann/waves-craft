#include "PluginEditor.h"

namespace craft::ddp
{

using namespace ui;

namespace
{
constexpr int editorW = 860, editorH = 500, headerH = 46, margin = 12;
}

DynamicDistortionEditor::DynamicDistortionEditor (DynamicDistortionProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.getState();
    auto knob = [&] (std::unique_ptr<Knob>& k, const char* id, const juce::String& title, juce::Colour c) {
        k = std::make_unique<Knob> (st, id, title, c);
        addAndMakeVisible (*k);
        knobs.push_back (k.get());
    };

    // distortion section
    knob (input, ids::input, "Input", colours::level);
    knob (drive, ids::drive, "Drive", colours::heat);
    knob (attack, ids::attack, "Attack", colours::heatHot);
    knob (release, ids::release, "Release", colours::heatHot);
    knob (direct, ids::direct, "Direct", colours::level);
    knob (distLevel, ids::distLevel, "Distortion", colours::heat);
    knob (output, ids::output, "Output", colours::level);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    // tone section
    knob (bass, ids::bass, "Bass", colours::tone);
    knob (mid, ids::mid, "Mid", colours::tone);
    knob (midFreq, ids::midFreq, "Mid Freq", colours::tone);
    knob (treble, ids::treble, "Treble", colours::tone);
    knob (lowPass, ids::lowPass, "Low-pass", colours::heatHot);
    addAndMakeVisible (toneDisplay);

    bypass.setClickingTogglesState (true);
    bypass.setTooltip ("Bypass (latency-compensated)");
    addAndMakeVisible (bypass);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, ids::bypass, bypass);

    setSize (editorW, editorH);
    toneDisplay.setSettings (proc.readSettings());
    startTimerHz (30);
}

DynamicDistortionEditor::~DynamicDistortionEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DynamicDistortionEditor::timerCallback()
{
    for (auto* k : knobs)
        k->refresh();
    const int ch = proc.getTotalNumOutputChannels() > 1 ? 2 : 1;
    inMeter.push (proc.live.inPeak[0].exchange (0.0f), proc.live.inPeak[1].exchange (0.0f), ch);
    outMeter.push (proc.live.outPeak[0].exchange (0.0f), proc.live.outPeak[1].exchange (0.0f), ch);
    const float e = proc.live.expansionDb.load();
    if (std::abs (e - expansionDb) > 0.02f)
    {
        expansionDb = e;
        repaint (expansionBar.expanded (2));
    }
    toneDisplay.setSettings (proc.readSettings());
}

void DynamicDistortionEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header: wordmark and a small clipped-sine glyph
    auto head = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);
    {
        auto glyph = head.removeFromLeft (30).toFloat().withSizeKeepingCentre (26.0f, 18.0f);
        juce::Path wave;
        for (int i = 0; i <= 40; ++i)
        {
            const float t = float (i) / 40.0f;
            const float v = juce::jlimit (-0.62f, 0.62f, std::sin (t * juce::MathConstants<float>::twoPi));
            const juce::Point<float> pt (glyph.getX() + t * glyph.getWidth(), glyph.getCentreY() - v * glyph.getHeight() * 0.75f);
            if (i == 0)
                wave.startNewSubPath (pt);
            else
                wave.lineTo (pt);
        }
        g.setColour (colours::heat);
        g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    head.removeFromLeft (6);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)).withExtraKerningFactor (0.06f));
    g.drawText ("CRAFT", head.removeFromLeft (64), juce::Justification::centredLeft);
    g.setColour (colours::heat);
    g.drawText ("DYNAMIC DISTORTION", head.removeFromLeft (230), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f)));
    g.drawText (juce::String ("v") + CRAFT_VERSION_STRING + "  |  latency " + juce::String (proc.getLatencySamples()) + " smp",
                head.removeFromLeft (220), juce::Justification::centredLeft);

    drawSection (g, distArea, "Distortion", "envelope-driven drive and clip", colours::heat);
    drawSection (g, toneArea, "Tone", "pre-distortion EQ, post low-pass", colours::tone);

    // sub-group frames inside the distortion section
    auto frame = [&] (juce::Rectangle<int> r, const juce::String& label) {
        g.setColour (colours::well.withAlpha (0.45f));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (colours::textDim.withAlpha (0.8f));
        g.setFont (juce::Font (uiFont (9.5f, true)).withExtraKerningFactor (0.12f));
        g.drawText (label, r.withHeight (14).translated (0, -15), juce::Justification::centred);
    };
    frame (dynamicsBox, "DYNAMICS");
    frame (mixBox, "MIX");

    // expansion activity: horizontal bar 0 .. +8 dB
    {
        auto b = expansionBar.toFloat();
        g.setColour (colours::well);
        g.fillRoundedRectangle (b, 3.0f);
        const float t = juce::jlimit (0.0f, 1.0f, expansionDb / 8.0f);
        juce::ColourGradient grad (colours::heatHot.withAlpha (0.6f), b.getX(), 0.0f, colours::heat, b.getRight(), 0.0f, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * t), 3.0f);
        g.setColour (colours::text);
        g.setFont (juce::Font (uiFont (10.0f, true)));
        g.drawText ("EXPANSION  +" + juce::String (expansionDb, 1) + " dB", expansionBar.translated (0, 14),
                    juce::Justification::centred);
    }

    // meter captions
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (9.5f, true)));
    g.drawText ("IN", inMeter.getBounds().withHeight (12).translated (0, -14), juce::Justification::centred);
    g.drawText ("OUT", outMeter.getBounds().withHeight (12).translated (0, -14), juce::Justification::centred);
}

void DynamicDistortionEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (headerH);
    bypass.setBounds (head.removeFromRight (90 + margin).withTrimmedRight (margin).withSizeKeepingCentre (84, 24));

    r = r.reduced (margin, 0).withTrimmedBottom (margin);
    distArea = r.removeFromTop (240);
    r.removeFromTop (10);
    toneArea = r;

    // distortion: [IN meter][Input] [Drive] [Attack Release + expansion] [Direct Distortion] [Output][OUT meter]
    {
        auto a = distArea.reduced (14, 0).withTrimmedTop (34).withTrimmedBottom (14);
        auto meterIn = a.removeFromLeft (22);
        inMeter.setBounds (meterIn.withTrimmedTop (16));
        a.removeFromLeft (6);
        input->setBounds (a.removeFromLeft (84).withSizeKeepingCentre (84, 104));
        auto meterOut = a.removeFromRight (22);
        outMeter.setBounds (meterOut.withTrimmedTop (16));
        a.removeFromRight (6);
        output->setBounds (a.removeFromRight (84).withSizeKeepingCentre (84, 104));
        a.reduce (10, 0);

        drive->setBounds (a.removeFromLeft (170).withSizeKeepingCentre (160, 176));
        a.removeFromLeft (10);
        mixBox = a.removeFromRight (190).withTrimmedTop (16);
        a.removeFromRight (12);
        dynamicsBox = a.withTrimmedTop (16);

        auto dyn = dynamicsBox.reduced (8, 8);
        expansionBar = dyn.removeFromBottom (34).withTrimmedBottom (16).reduced (14, 2);
        const int w = dyn.getWidth() / 2;
        attack->setBounds (dyn.removeFromLeft (w).reduced (4, 0));
        release->setBounds (dyn.reduced (4, 0));

        auto mix = mixBox.reduced (8, 8);
        direct->setBounds (mix.removeFromLeft (mix.getWidth() / 2).reduced (4, 10));
        distLevel->setBounds (mix.reduced (4, 10));
    }

    // tone: response curve, then the five controls
    {
        auto a = toneArea.reduced (14, 0).withTrimmedTop (32).withTrimmedBottom (12);
        toneDisplay.setBounds (a.removeFromLeft (330));
        a.removeFromLeft (14);
        const int w = a.getWidth() / 5;
        for (auto* k : { bass.get(), mid.get(), midFreq.get(), treble.get(), lowPass.get() })
            k->setBounds (a.removeFromLeft (w).reduced (3, 0));
    }
}

} // namespace craft::ddp
