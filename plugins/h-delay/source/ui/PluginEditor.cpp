#include "PluginEditor.h"

#include "dsp/DelayTime.h"

namespace craft::hdelay
{

using namespace ui;

namespace
{
constexpr int margin = 12;
}

HDelayEditor::HDelayEditor (HDelayProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      delayMsKnob (p.getState(), ids::delayMs, "Delay", true),
      delayNoteKnob (p.getState(), ids::delayNote, "Delay", true),
      feedbackKnob (p.getState(), ids::feedback, "Feedback", true),
      mixKnob (p.getState(), ids::mix, "Dry/Wet"),
      outputKnob (p.getState(), ids::output, "Output"),
      analogKnob (p.getState(), ids::analog, "Analog"),
      depthKnob (p.getState(), ids::modDepth, "Depth"),
      rateKnob (p.getState(), ids::modRate, "Rate"),
      hiPassKnob (p.getState(), ids::hiPass, "HiPass"),
      loPassKnob (p.getState(), ids::loPass, "LoPass"),
      link (p.getState()),
      display (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.getState();

    delayMsKnob.setScaleEnds ("1", "3500");
    delayNoteKnob.setScaleEnds ("1/64T", "2 bars");
    feedbackKnob.setScaleEnds ("0", "200");
    analogKnob.setDetentLabels ({ "Off", "1", "2", "3", "4" });
    depthKnob.setScaleEnds ("0", "100");
    rateKnob.setScaleEnds ("0.1", "6k");
    hiPassKnob.setScaleEnds ("Off", "20k");
    loPassKnob.setScaleEnds ("20", "Off");
    loPassKnob.getSlider().getProperties().set ("arcFromEnd", true);
    for (auto* k : { &depthKnob, &rateKnob, &hiPassKnob, &loPassKnob })
        k->setAccent (colours::accent2);
    analogKnob.getSlider().setMouseDragSensitivity (120);

    for (auto* k : { &delayMsKnob, &delayNoteKnob, &feedbackKnob, &mixKnob, &outputKnob, &analogKnob, &depthKnob, &rateKnob,
                     &hiPassKnob, &loPassKnob })
        addAndMakeVisible (*k);
    addAndMakeVisible (link);
    addAndMakeVisible (display);
    addAndMakeVisible (meter);

    tap.onTap = [this] (double ms) { handleTap (ms); };
    addAndMakeVisible (tap);

    auto attach = [&] (const char* id, juce::Button& b, const juce::String& tip) {
        b.setClickingTogglesState (true);
        b.setTooltip (tip);
        addAndMakeVisible (b);
        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, id, b));
    };
    attach (ids::lofi, lofi, "LoFi: band-limits every repeat (in the feedback loop)");
    attach (ids::phaseL, phaseL, "Invert the left wet signal");
    attach (ids::pingPong, pingPong, "Ping Pong: echoes alternate between the sides");
    attach (ids::dual, dual, "Dual: separate left and right delay times (drag the R value in the display)");
    attach (ids::phaseR, phaseR, "Invert the right wet signal");
    lofi.setColour (juce::TextButton::buttonOnColourId, colours::accent2);

    const std::pair<juce::TextButton*, param::Sync> syncButtons[] = { { &syncBpm, param::Sync::Bpm },
                                                                      { &syncHost, param::Sync::Host },
                                                                      { &syncMs, param::Sync::Ms } };
    for (auto [b, mode] : syncButtons)
    {
        b->setColour (juce::TextButton::buttonOnColourId, colours::accent2);
        b->onClick = [this, m = mode] { proc.setParameterValue (ids::sync, float (int (m))); };
        addAndMakeVisible (*b);
    }
    syncBpm.setTooltip ("Note values at the tempo set in the display (drag the BPM value)");
    syncHost.setTooltip ("Note values at the host tempo");
    syncMs.setTooltip ("Delay in milliseconds");

    setSize (editorWidth, editorHeight);
    updateSyncDependentControls();
    startTimerHz (30);
}

HDelayEditor::~HDelayEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HDelayEditor::handleTap (double intervalMs)
{
    const auto s = proc.readSettings();
    switch (s.sync)
    {
        case param::Sync::Ms:
            proc.setParameterValue (ids::delayMs, float (juce::jlimit (1.0, maxDelayMs, std::round (intervalMs))));
            break;
        case param::Sync::Host:
            proc.setParameterValue (ids::delayNote, float (nearestNote (intervalMs, s.hostBpm)));
            break;
        case param::Sync::Bpm:
            proc.setParameterValue (ids::bpm, float (juce::jlimit (30.0, 300.0, 60000.0 / intervalMs)));
            break;
    }
}

void HDelayEditor::updateSyncDependentControls()
{
    const auto sync = proc.readSettings().sync;
    if (int (sync) == lastSync)
        return;
    lastSync = int (sync);
    const bool ms = sync == param::Sync::Ms;
    delayMsKnob.setVisible (ms);
    delayNoteKnob.setVisible (! ms);
    syncBpm.setToggleState (sync == param::Sync::Bpm, juce::dontSendNotification);
    syncHost.setToggleState (sync == param::Sync::Host, juce::dontSendNotification);
    syncMs.setToggleState (ms, juce::dontSendNotification);
}

void HDelayEditor::timerCallback()
{
    updateSyncDependentControls();
    display.refresh();
    const auto pk = proc.takePeaks();
    meter.pushPeaks (pk[0], pk[1]);
    analogKnob.repaint (); // detent label highlight
}

void HDelayEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    drawPanel (g, leftPanel.toFloat());
    drawPanel (g, delayPanel.toFloat());
    drawPanel (g, centrePanel.toFloat());
    drawPanel (g, feedbackPanel.toFloat());
    drawPanel (g, rightPanel.toFloat());
    drawPanel (g, modPanel.toFloat(), "Modulation");
    drawPanel (g, filterPanel.toFloat(), "Filters");

    // product label (left column, under the LoFi toggle)
    auto label = leftPanel.reduced (10).removeFromBottom (64);
    auto mark = label.removeFromTop (18).toFloat().withSizeKeepingCentre (30.0f, 14.0f);
    g.setColour (colours::accent);
    g.drawEllipse (mark.withWidth (14.0f), 2.0f);
    g.setColour (colours::accent2);
    g.drawEllipse (mark.withTrimmedLeft (16.0f), 2.0f);
    g.setColour (colours::accent2);
    g.drawLine (mark.getX() + 13.0f, mark.getCentreY(), mark.getX() + 17.0f, mark.getCentreY(), 2.0f);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (15.0f, true)));
    g.drawText ("HYBRID", label.removeFromTop (17), juce::Justification::centred);
    g.drawText ("DELAY", label.removeFromTop (17), juce::Justification::centred);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (9.5f)));
    g.drawText ("CRAFT AUDIO  v" CRAFT_VERSION_STRING, label, juce::Justification::centred);

    // meter caption
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.drawText ("OUT", meter.getBounds().withHeight (18).translated (-14, 0), juce::Justification::centredLeft);
}

void HDelayEditor::resized()
{
    auto r = getLocalBounds().reduced (margin);
    auto bottom = r.removeFromBottom (122);
    r.removeFromBottom (margin);

    leftPanel = r.removeFromLeft (124);
    r.removeFromLeft (margin);
    rightPanel = r.removeFromRight (196);
    r.removeFromRight (margin);
    delayPanel = r.removeFromLeft (180);
    r.removeFromLeft (margin);
    feedbackPanel = r.removeFromRight (180);
    r.removeFromRight (margin);
    centrePanel = r;

    modPanel = bottom.removeFromLeft (leftPanel.getWidth() + margin + delayPanel.getWidth() + margin + 140);
    bottom.removeFromLeft (margin);
    filterPanel = bottom;

    // left column: tap pad, LoFi, product label
    auto lp = leftPanel.reduced (12);
    tap.setBounds (lp.removeFromTop (100));
    lp.removeFromTop (12);
    lofi.setBounds (lp.removeFromTop (26));

    // delay / feedback knobs
    const auto bigKnob = [] (juce::Rectangle<int> a) { return a.reduced (6, 14); };
    delayMsKnob.setBounds (bigKnob (delayPanel));
    delayNoteKnob.setBounds (bigKnob (delayPanel));
    feedbackKnob.setBounds (bigKnob (feedbackPanel));

    // centre: toggles, display, sync buttons
    auto c = centrePanel.reduced (10);
    auto toggles = c.removeFromTop (28);
    const float weights[] = { 0.8f, 1.5f, 1.0f, 0.8f };
    const float unit = float (toggles.getWidth() - 3 * 6) / (0.8f + 1.5f + 1.0f + 0.8f);
    int i = 0;
    for (auto* b : { &phaseL, &pingPong, &dual, &phaseR })
    {
        b->setBounds (i == 3 ? toggles : toggles.removeFromLeft (juce::roundToInt (unit * weights[i])));
        toggles.removeFromLeft (6);
        ++i;
    }
    auto syncRow = c.removeFromBottom (28);
    c.reduce (0, 10);
    display.setBounds (c);
    const int sw = (syncRow.getWidth() - 2 * 6) / 3;
    for (auto* b : { &syncBpm, &syncHost, &syncMs })
    {
        b->setBounds (syncRow.removeFromLeft (sw));
        syncRow.removeFromLeft (6);
    }

    // right column: meter on the right, three knobs stacked
    auto rp = rightPanel.reduced (8);
    meter.setBounds (rp.removeFromRight (62));
    rp.removeFromRight (4);
    const int kh = rp.getHeight() / 3;
    mixKnob.setBounds (rp.removeFromTop (kh));
    outputKnob.setBounds (rp.removeFromTop (kh));
    analogKnob.setBounds (rp);

    // bottom row
    auto mp = modPanel.reduced (10).withTrimmedTop (14);
    const int mw = 110;
    const int mx = mp.getCentreX() - mw - 20;
    depthKnob.setBounds (mx, mp.getY(), mw, mp.getHeight());
    rateKnob.setBounds (mx + mw + 40, mp.getY(), mw, mp.getHeight());

    auto fp = filterPanel.reduced (10).withTrimmedTop (14);
    const int fx = fp.getCentreX();
    hiPassKnob.setBounds (fx - 34 - mw, fp.getY(), mw, fp.getHeight());
    loPassKnob.setBounds (fx + 34, fp.getY(), mw, fp.getHeight());
    link.setBounds (juce::Rectangle<int> (34, 34).withCentre ({ fx, fp.getY() + fp.getHeight() / 2 - 6 }));
}

} // namespace craft::hdelay
