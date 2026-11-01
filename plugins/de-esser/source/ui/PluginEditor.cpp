#include "PluginEditor.h"

#include "dsp/Parameters.h"

namespace craft::deesser
{

using namespace ui;

namespace
{
constexpr int headerH = 40, margin = 12, leftW = 150, centreW = 284, rightW = 132, bodyH = 330;

juce::RangedAudioParameter& param (DeEsserProcessor& p, const char* id) { return *p.getState().getParameter (id); }

// Sidechain energy: 0 .. -80 dBFS, linear in dB (same axis as the Threshold slider).
float sidechainFraction (float db) { return (db + 80.0f) / 80.0f; }

// Attenuation: 0, 3, 6, 12, 18 dB and "Inf" (the 25.6 dB floor) equally spaced.
float attenFraction (float reductionDb)
{
    static const float pts[][2] = { { 0.0f, 0.0f }, { 3.0f, 0.2f }, { 6.0f, 0.4f }, { 12.0f, 0.6f }, { 18.0f, 0.8f }, { 25.6f, 1.0f } };
    if (reductionDb <= 0.0f)
        return 0.0f;
    for (int i = 1; i < 6; ++i)
        if (reductionDb <= pts[i][0])
            return pts[i - 1][1] + (reductionDb - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]) * (pts[i][1] - pts[i - 1][1]);
    return 1.0f;
}

// Output: -30 .. 0 dBFS.
float outputFraction (float db) { return (db + 30.0f) / 30.0f; }

float toDb (float lin) { return lin > 0.0f ? 20.0f * std::log10 (lin) : -200.0f; }

std::vector<MeterScale> sidechainScale()
{
    std::vector<MeterScale> s;
    for (int db = 0; db >= -80; db -= 10)
        s.push_back ({ float (db), juce::String (-db) });
    return s;
}
} // namespace

DeEsserEditor::DeEsserEditor (DeEsserProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      audioMode (param (p, ids::audio), "Split", "Wideband"),
      frequencyBox (param (p, ids::frequency), ids::frequency, "Hz"),
      filterSelector (param (p, ids::sidechain)),
      monitorSelector (param (p, ids::monitor)),
      thresholdAttachment (p.getState(), ids::threshold, thresholdSlider),
      thresholdBox (param (p, ids::threshold), ids::threshold, "dB"),
      sidechainMeter (1, false, sidechainFraction, sidechainScale()),
      attenMeter (1, true, attenFraction,
                  { { 0.0f, "0" }, { 3.0f, "-3" }, { 6.0f, "-6" }, { 12.0f, "-12" }, { 18.0f, "-18" }, { 25.6f, "Inf" } }),
      outputMeter (2, false, outputFraction,
                   { { 0.0f, "0" }, { -3.0f, "-3" }, { -6.0f, "-6" }, { -10.0f, "-10" }, { -15.0f, "-15" }, { -20.0f, "-20" },
                     { -30.0f, "-30" } })
{
    setLookAndFeel (&lnf);

    audioMode.setTooltip ("Split: reduce only the band above Freq. Wideband: reduce the whole signal");
    for (juce::Component* c : { (juce::Component*) &audioMode, (juce::Component*) &frequencyBox, (juce::Component*) &filterSelector,
                                (juce::Component*) &monitorSelector, (juce::Component*) &thresholdSlider,
                                (juce::Component*) &thresholdBox, (juce::Component*) &sidechainMeter,
                                (juce::Component*) &sidechainReadout, (juce::Component*) &attenMeter,
                                (juce::Component*) &attenReadout, (juce::Component*) &outputMeter })
        addAndMakeVisible (c);

    thresholdSlider.setSliderStyle (juce::Slider::LinearVertical);
    thresholdSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    thresholdSlider.setSliderSnapsToMousePosition (false);
    thresholdSlider.setMouseDragSensitivity (240);
    thresholdSlider.setDoubleClickReturnValue (true, dsp::param::Threshold::defaultValue, juce::ModifierKeys::altModifier);
    thresholdSlider.setColour (juce::Slider::thumbColourId, colours::accent);
    thresholdSlider.setTooltip ("Threshold - double-click or Alt-click resets to 0 dB");

    sidechainMeter.setScaleOnLeft (false);
    attenMeter.setScaleOnLeft (false);
    attenMeter.setZone (0.0f, attenFraction (6.0f), colours::accent2.withAlpha (0.12f));
    outputMeter.setScaleOnLeft (true);
    if (proc.getNumAudioChannels() > 1)
        outputMeter.setBarLabels ({ "L", "R" });
    else
        outputMeter.setBarLabels ({ "M", "M" });

    for (auto& r : outputReadout)
        addAndMakeVisible (r);

    setSize (margin + leftW + margin + centreW + margin + rightW + margin, headerH + margin + bodyH + margin);
    startTimerHz (30);
}

DeEsserEditor::~DeEsserEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DeEsserEditor::timerCallback()
{
    frequencyBox.refresh();
    thresholdBox.refresh();

    const auto m = proc.takeMeters();
    const float scDb = toDb (m.sidechainPeak);
    sidechainMeter.setLevels (&scDb);
    sidechainReadout.push (scDb);

    const float grDb = -toDb (m.minGain);
    attenMeter.setLevels (&grDb);
    attenReadout.push (-grDb);

    const float out[2] = { toDb (m.output[0]), toDb (m.output[1]) };
    outputMeter.setLevels (out);
    for (int c = 0; c < 2; ++c)
        outputReadout[c].push (out[c]);

    if (m.monitoringSidechain != monitoringSidechain)
    {
        monitoringSidechain = m.monitoringSidechain;
        repaint (monitorLed.expanded (4));
    }
    repaint (centreBlock); // threshold flag and line over the meters
}

void DeEsserEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);
    // logo mark: a sibilant "s" wave fading into a flat line
    auto mark = h.removeFromLeft (30).toFloat().withSizeKeepingCentre (28, 16);
    juce::Path wave;
    for (int i = 0; i <= 28; ++i)
    {
        const float t = float (i) / 28.0f;
        const float amp = 7.0f * (1.0f - t) * (1.0f - t) + 0.5f;
        const juce::Point<float> pt (mark.getX() + t * mark.getWidth(), mark.getCentreY() - amp * std::sin (t * 18.0f));
        if (i == 0)
            wave.startNewSubPath (pt);
        else
            wave.lineTo (pt);
    }
    g.setColour (colours::accent2);
    g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    h.removeFromLeft (6);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText (proc.getName().toUpperCase(), h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" DE_ESSER_VERSION_STRING, h, juce::Justification::centredRight);

    for (auto b : { audioBlock, freqBlock, monitorBlock, centreBlock, rightBlock })
        drawPanel (g, b.toFloat());

    auto title = [&g] (juce::Rectangle<int> r, const juce::String& t) {
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (11.5f, true)));
        g.drawText (t, r, juce::Justification::centred);
    };
    title (audioBlock.withHeight (22).withTrimmedTop (4), "AUDIO");
    title (freqBlock.withHeight (22).withTrimmedTop (4), "FREQUENCY");
    title (filterSelector.getBounds().translated (0, -20).withHeight (18), "SIDECHAIN");
    title (monitorBlock.withHeight (22).withTrimmedTop (4), "MONITOR");
    title (centreBlock.withHeight (22).withTrimmedTop (4), "THRESHOLD  /  SIDECHAIN  /  ATTEN");
    title (rightBlock.withHeight (22).withTrimmedTop (4), "OUTPUT");

    // monitor indicator: lights while the sidechain is being monitored
    const auto led = monitorLed.toFloat();
    if (monitoringSidechain)
    {
        g.setColour (colours::warn.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (monitoringSidechain ? colours::warn : colours::well);
    g.fillEllipse (led);
    g.setColour (colours::panelEdge.brighter (0.3f));
    g.drawEllipse (led, 1.0f);
    g.setColour (monitoringSidechain ? colours::text : colours::textDim);
    g.setFont (juce::Font (uiFont (10.0f, true)));
    g.drawText ("SIDECHAIN MONITOR", juce::Rectangle<int> (monitorLed.getRight() + 6, monitorLed.getCentreY() - 6, 110, 12),
                juce::Justification::centredLeft);

    // column captions under the meters
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.0f, true)));
    g.drawText ("SC LEVEL", sidechainReadout.getBounds().translated (0, 22).withHeight (12).expanded (12, 0),
                juce::Justification::centred);
    g.drawText ("ATTEN", attenReadout.getBounds().translated (0, 22).withHeight (12).expanded (12, 0), juce::Justification::centred);
    g.drawText ("THRESH", thresholdBox.getBounds().translated (0, 22).withHeight (12).expanded (12, 0), juce::Justification::centred);
}

void DeEsserEditor::paintOverChildren (juce::Graphics& g)
{
    // threshold line across the sidechain meter, and a value flag next to the slider handle
    const double thr = thresholdSlider.getValue();
    const float y = float (thresholdSlider.getY()) + float (thresholdSlider.getPositionOfValue (thr));
    const auto bars = sidechainMeter.getBarsArea().translated (sidechainMeter.getX(), sidechainMeter.getY()).toFloat();
    g.setColour (colours::accent);
    g.drawLine (bars.getX() - 4.0f, y, bars.getRight() + 4.0f, y, 1.5f);

    const auto flagText = formatValue (ids::threshold, thr);
    const auto flag = juce::Rectangle<float> (40.0f, 16.0f).withCentre ({ float (thresholdSlider.getX()) - 24.0f, y });
    juce::Path p;
    p.addRoundedRectangle (flag, 2.5f);
    p.addTriangle (flag.getRight(), y - 5.0f, flag.getRight(), y + 5.0f, flag.getRight() + 6.0f, y);
    g.setColour (colours::accent);
    g.fillPath (p);
    g.setColour (colours::background);
    g.setFont (juce::Font (uiFont (11.0f, true)));
    g.drawText (flagText, flag, juce::Justification::centred);
}

void DeEsserEditor::resized()
{
    auto body = getLocalBounds().withTrimmedTop (headerH).reduced (margin);

    // left column: Audio / Frequency + SideChain / Monitor
    auto left = body.removeFromLeft (leftW);
    body.removeFromLeft (margin);
    audioBlock = left.removeFromTop (68);
    left.removeFromTop (8);
    freqBlock = left.removeFromTop (128);
    left.removeFromTop (8);
    monitorBlock = left;

    audioMode.setBounds (audioBlock.reduced (12, 0).withTrimmedTop (28).withHeight (30));
    frequencyBox.setBounds (freqBlock.reduced (24, 0).withTrimmedTop (28).withHeight (26));
    filterSelector.setBounds (freqBlock.reduced (12, 0).withTrimmedTop (82).withHeight (30));
    auto mon = monitorBlock.reduced (12, 0).withTrimmedTop (28);
    monitorSelector.setBounds (mon.removeFromTop (60));
    monitorLed = mon.withTrimmedTop (8).removeFromTop (12).withWidth (12).translated (4, 0);

    // right: output meters
    rightBlock = body.removeFromRight (rightW);
    body.removeFromRight (margin);
    centreBlock = body;

    auto rb = rightBlock.reduced (8).withTrimmedTop (18);
    auto rbReadouts = rb.removeFromBottom (24);
    outputMeter.setBounds (rb);
    const auto ob = outputMeter.getBarsArea().translated (outputMeter.getX(), outputMeter.getY());
    outputReadout[0].setBounds (juce::Rectangle<int> (40, 22).withCentre ({ ob.getX() - 8, rbReadouts.getCentreY() }));
    outputReadout[1].setBounds (juce::Rectangle<int> (40, 22).withCentre ({ ob.getRight() + 16, rbReadouts.getCentreY() }));

    // centre: threshold slider | sidechain meter | attenuation meter, readouts underneath
    auto cb = centreBlock.reduced (10).withTrimmedTop (16);
    auto readouts = cb.removeFromBottom (40);
    auto sliderCol = cb.removeFromLeft (92);
    auto scCol = cb.removeFromLeft (84);
    auto attCol = cb;

    sidechainMeter.setBounds (scCol);
    attenMeter.setBounds (attCol.withTrimmedRight (10));
    const auto scBars = sidechainMeter.getBarsArea().translated (scCol.getX(), scCol.getY());
    const int thumb = lnf.getSliderThumbRadius (thresholdSlider);
    thresholdSlider.setBounds (sliderCol.getRight() - 34, scBars.getY() - thumb, 30, scBars.getHeight() + 2 * thumb);

    auto rowY = readouts.getY() + 2;
    thresholdBox.setBounds (juce::Rectangle<int> (64, 22).withCentre ({ thresholdSlider.getBounds().getCentreX() - 18, rowY + 11 }));
    sidechainReadout.setBounds (juce::Rectangle<int> (48, 22).withCentre ({ scBars.getCentreX(), rowY + 11 }));
    const auto atBars = attenMeter.getBarsArea().translated (attenMeter.getX(), attenMeter.getY());
    attenReadout.setBounds (juce::Rectangle<int> (48, 22).withCentre ({ atBars.getCentreX(), rowY + 11 }));
}

} // namespace craft::deesser
