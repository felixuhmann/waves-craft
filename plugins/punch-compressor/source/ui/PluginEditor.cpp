#include "PluginEditor.h"

namespace craft::punch
{

using namespace ui;

namespace
{
constexpr int headerH = 42, margin = 12, gap = 10;
constexpr int leftW = 300, rightW = 200;
} // namespace

PunchCompressorEditor::PunchCompressorEditor (PunchCompressorProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      meterSelect (*p.getState().getParameter (ids::meter), { "IN", "GR", "OUT" }, colours::punch),
      syncSelect (*p.getState().getParameter (ids::sync), { "BPM", "HOST", "MS" }, colours::accent),
      analogSelect (*p.getState().getParameter (ids::analog), { "OFF", "1", "2", "3", "4" }, colours::accent2),
      stageSelect (*p.getState().getParameter (ids::limiter), { "CLIPPER", "LIMITER" }, colours::accent2),
      threshold (p.getState(), ids::threshold, "Threshold", "dB", true),
      ratio (p.getState(), ids::ratio, "Ratio", ""),
      attack (p.getState(), ids::attack, "Attack", "ms"),
      releaseMs (p.getState(), ids::release, "Release", "ms"),
      releaseNote (p.getState(), ids::note, "Release", ""),
      punch (p.getState(), ids::punch, "Punch", ""),
      mix (p.getState(), ids::mix, "Mix", "%"),
      output (p.getState(), ids::output, "Output", "dB"),
      tempoBox (*p.getState().getParameter (ids::tempo), ids::tempo, "BPM")
{
    setLookAndFeel (&lnf);

    vu.setTooltip ("Click to reset the peak readout");
    addAndMakeVisible (vu);
    meterSelect.setTooltip ("Meter: input level, gain reduction or output level (display only)");
    addAndMakeVisible (meterSelect);
    threshold.setAccent (colours::punch);
    addAndMakeVisible (threshold);
    for (auto* k : { &ratio, &attack, &releaseMs, &releaseNote, &punch, &mix, &output })
        addAndMakeVisible (*k);
    punch.setAccent (colours::punch);
    punch.getSlider().setTooltip ("Punch: delays the gain reduction (and its recovery) by up to 30 ms, letting transients through");

    syncSelect.setTooltip ("Release in milliseconds, or as a note value at the host tempo / a manual tempo");
    addAndMakeVisible (syncSelect);
    tempoBox.decorate = [this] (const juce::String& t) {
        if (t == "Off")
            return juce::String ("HOST ") + juce::String (proc.getHostBpm(), 1);
        return t + " BPM";
    };
    tempoBox.setTooltip ("Manual tempo for the BPM mode (drag below 30 for Off: host tempo). Double-click resets");
    addChildComponent (tempoBox);
    hostTempo.setJustificationType (juce::Justification::centred);
    hostTempo.setFont (juce::Font (uiFont (13.0f, true)));
    hostTempo.setColour (juce::Label::textColourId, colours::textDim);
    addChildComponent (hostTempo);

    analogSelect.setTooltip ("Analog colour: Off or one of four modes (tone, harmonics and noise)");
    addAndMakeVisible (analogSelect);
    stageSelect.setTooltip ("Output peak stage (ceiling 0 dBFS): both settings use the look-ahead peak limiter");
    addAndMakeVisible (stageSelect);

    setSize (editorWidth, editorHeight);
    updateSyncDependentControls();
    startTimerHz (30);
}

PunchCompressorEditor::~PunchCompressorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PunchCompressorEditor::updateSyncDependentControls()
{
    const int sync = syncSelect.getIndex(); // 0 BPM, 1 Host, 2 ms
    if (sync == lastSync)
        return;
    lastSync = sync;
    releaseMs.setVisible (sync == 2);
    releaseNote.setVisible (sync != 2);
    tempoBox.setVisible (sync == 0);
    hostTempo.setVisible (sync != 0);
    repaint();
}

void PunchCompressorEditor::timerCallback()
{
    for (auto* k : { &threshold, &ratio, &attack, &releaseMs, &releaseNote, &punch, &mix, &output })
        k->refresh();
    for (auto* s : { &meterSelect, &syncSelect, &analogSelect, &stageSelect })
        s->refresh();
    tempoBox.refresh();
    updateSyncDependentControls();

    const int sync = syncSelect.getIndex();
    const juce::String host = juce::String (proc.getHostBpm(), 1) + " BPM";
    hostTempo.setText (sync == 1 ? "HOST " + host : "--", juce::dontSendNotification);

    const int m = meterSelect.getIndex();
    const auto src = m == 0 ? VuMeter::Source::In : (m == 2 ? VuMeter::Source::Out : VuMeter::Source::Gr);
    vu.setSource (src);
    const auto snap = proc.takeMeters();
    vu.push (src == VuMeter::Source::In ? snap.input : (src == VuMeter::Source::Out ? snap.output : snap.minGain));
}

void PunchCompressorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::punch);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);

    // logo mark: a transient spike passing over a flattened envelope
    auto mark = h.removeFromLeft (30).toFloat().withSizeKeepingCentre (26, 20);
    juce::Path env;
    env.startNewSubPath (mark.getX(), mark.getBottom() - 3.0f);
    env.lineTo (mark.getX() + 7.0f, mark.getBottom() - 3.0f);
    env.lineTo (mark.getX() + 10.0f, mark.getY() + 1.0f);
    env.lineTo (mark.getX() + 13.0f, mark.getY() + 9.0f);
    env.lineTo (mark.getRight(), mark.getY() + 9.0f);
    g.setColour (colours::punch);
    g.strokePath (env, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::accent);
    g.fillRect (juce::Rectangle<float> (mark.getX() + 13.0f, mark.getY() + 12.0f, mark.getWidth() - 13.0f, 2.0f));

    h.removeFromLeft (6);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText ("CRAFT PUNCH COMPRESSOR", h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" CRAFT_VERSION_STRING, h, juce::Justification::centredRight);

    drawPanel (g, leftPanel.toFloat());
    drawPanel (g, centrePanel.toFloat());
    drawPanel (g, rightPanel.toFloat());

    // right panel captions
    auto caption = [&g] (juce::Component& c, const juce::String& text) {
        drawTitle (g, c.getBounds().translated (0, -19).withHeight (16), text);
    };
    caption (syncSelect, "Release Sync");
    drawTitle (g, tempoRow.translated (0, -19).withHeight (16), "Tempo");
    caption (analogSelect, "Analog");
    caption (stageSelect, "Output Stage");
    caption (meterSelect, "Meter");

    // look-ahead note
    const double fs = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f)));
    g.drawText ("LOOK-AHEAD " + juce::String (proc.getLatencySamples()) + " SMP @ " + juce::String (fs / 1000.0, 1) + " kHz",
                rightPanel.reduced (8).removeFromBottom (16), juce::Justification::centred);
}

void PunchCompressorEditor::resized()
{
    auto r = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    leftPanel = r.removeFromLeft (leftW);
    r.removeFromLeft (gap);
    rightPanel = r.removeFromRight (rightW);
    r.removeFromRight (gap);
    centrePanel = r;

    // left: meter, meter selector, large threshold knob
    auto l = leftPanel.reduced (10);
    vu.setBounds (l.removeFromTop (118));
    l.removeFromTop (24);
    meterSelect.setBounds (l.removeFromTop (24).withSizeKeepingCentre (150, 24));
    l.removeFromTop (8);
    threshold.setBounds (l);

    // centre: 3 x 2 knobs
    auto c = centrePanel.reduced (8, 10);
    const int rowH = c.getHeight() / 2;
    auto row1 = c.removeFromTop (rowH), row2 = c;
    const int colW = row1.getWidth() / 3;
    ratio.setBounds (row1.removeFromLeft (colW).reduced (4));
    attack.setBounds (row1.removeFromLeft (colW).reduced (4));
    releaseMs.setBounds (row1.reduced (4));
    releaseNote.setBounds (releaseMs.getBounds());
    punch.setBounds (row2.removeFromLeft (colW).reduced (4));
    mix.setBounds (row2.removeFromLeft (colW).reduced (4));
    output.setBounds (row2.reduced (4));

    // right: sync, tempo, analog, output stage
    auto rr = rightPanel.reduced (12);
    rr.removeFromTop (22);
    syncSelect.setBounds (rr.removeFromTop (26));
    rr.removeFromTop (30);
    tempoRow = rr.removeFromTop (26);
    tempoBox.setBounds (tempoRow);
    hostTempo.setBounds (tempoRow);
    rr.removeFromTop (30);
    analogSelect.setBounds (rr.removeFromTop (26));
    rr.removeFromTop (30);
    stageSelect.setBounds (rr.removeFromTop (26));
}

} // namespace craft::punch
