#include "PluginEditor.h"

#include "dsp/Parameters.h"

namespace craft::levelrider
{

using namespace ui;

namespace
{
constexpr int W = 760, headerH = 40, margin = 12, topH = 138, mainH = 286, bottomH = 34;

juce::RangedAudioParameter& param (LevelRiderProcessor& p, const char* id) { return *p.getState().getParameter (id); }

float physical (juce::RangedAudioParameter& p) { return p.convertFrom0to1 (p.getValue()); }

juce::String signedDb (double v)
{
    const double r = std::round (v * 10.0) / 10.0;
    return (r > 0.0 ? "+" : "") + juce::String (r == 0.0 ? 0.0 : r, 1);
}
} // namespace

LevelRiderEditor::LevelRiderEditor (LevelRiderProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      targetBar (param (p, ids::target)),
      targetBox (param (p, ids::target), ids::target),
      attackSwitch (param (p, ids::attack), { "Slow", "Fast" }),
      vocalKnob (p.getState(), ids::vocalSens, "Vocal Sens", true),
      musicKnob (p.getState(), ids::musicSens, "Music Sens", true),
      rangeFader (param (p, ids::rangeMax), param (p, ids::rangeMin), param (p, ids::idle)),
      maxBox (param (p, ids::rangeMax), ids::rangeMax),
      minBox (param (p, ids::rangeMin), ids::rangeMin),
      idleBox (param (p, ids::idle), ids::idle),
      riderFader (RiderFader::Callbacks {
          [this] (double db) { proc.beginRiderDrag (db); },
          [this] (double db) { proc.dragRider (db); },
          [this] { proc.endRiderDrag(); },
          [this] { riderOneShot (physical (param (proc, ids::idle))); } }),
      riderBox (ValueBox::Callbacks {
          [this] { return signedDb (display.gainDb) + " dB"; },
          [this] { return signedDb (display.gainDb); },
          [this] (const juce::String& t) {
              riderOneShot (juce::jlimit (-12.0, 12.0, t.trim().removeCharacters (" dDbB+").getDoubleValue()));
          },
          [this] { return (display.gainDb + 12.0f) / 24.0f; },
          [this] (float n) { proc.dragRider (-12.0 + 24.0 * n); },
          [this] { proc.beginRiderDrag (display.gainDb); },
          [this] { proc.endRiderDrag(); },
          [this] { riderOneShot (physical (param (proc, ids::idle))); } }),
      outputFader (param (p, ids::output)),
      outputBox (param (p, ids::output), ids::output),
      automationSwitch ({ "Off", "Write", "Read" }, [this] { return int (proc.getAutomationMode()); },
                        [this] (int i) { proc.setAutomationMode (dsp::AutomationMode (i)); })
{
    setLookAndFeel (&lnf);

    if (p.isLive())
    {
        spillKnob = std::make_unique<Knob> (p.getState(), ids::spill, "Spill", false);
        addAndMakeVisible (*spillKnob);
    }

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &targetBar, &targetBox, &attackSwitch, &vocalKnob, &musicKnob, &rangeFader, &maxBox, &minBox, &idleBox,
             &riderFader, &riderBox, &outputFader, &outputBox, &automationSwitch })
        addAndMakeVisible (c);

    attackSwitch.setTooltip ("Attack: Slow (smooth, holds 0.5 s before idling) or Fast (quick reaction)");
    attackSwitch.setLitColours ({ colours::accent, colours::accent });
    automationSwitch.setLitColours ({ colours::textDim, colours::warn, colours::accent2 });
    automationSwitch.setTooltip ("Off: internal riding. Write: also write the Rider moves as host automation. "
                                 "Read: the Rider follows the host's automation");
    riderBox.setFontHeight (15.0f);
    riderBox.setTooltip ("Current ride gain - type a value or drag to move the Rider; double-click returns to Idle");
    outputFader.onClearClip = [this] { proc.clearClip(); };

    setSize (W, headerH + margin + topH + margin + mainH + margin + bottomH + margin);
    startTimerHz (30);
}

LevelRiderEditor::~LevelRiderEditor()
{
    stopTimer();
    proc.endRiderDrag();
    setLookAndFeel (nullptr);
}

void LevelRiderEditor::riderOneShot (double db)
{
    // set g once: hold the override for a few blocks, then let riding resume from that value
    proc.beginRiderDrag (db);
    juce::Component::SafePointer<LevelRiderEditor> safe (this);
    juce::Timer::callAfterDelay (60, [safe] {
        if (safe != nullptr)
            safe->proc.endRiderDrag();
    });
}

void LevelRiderEditor::timerCallback()
{
    display = proc.takeDisplay();
    targetBar.setLevel (display.vocalLevelDb, display.riding);
    riderFader.setState (display.gainDb, display.rangeMinDb, display.rangeMaxDb, physical (param (proc, ids::idle)), display.riding);
    outputFader.setClip (display.clipped, display.outputPeak);
    for (auto* b : { &targetBox, &maxBox, &minBox, &idleBox, &riderBox, &outputBox })
        b->refresh();
    vocalKnob.refresh();
    musicKnob.refresh();
    if (spillKnob != nullptr)
        spillKnob->refresh();
    automationSwitch.refresh();
    repaint (topLeft.getUnion (topRight));
    repaint (rangeBlock);
}

void LevelRiderEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);
    // logo mark: a fader cap riding along a wave
    auto mark = h.removeFromLeft (30).toFloat().withSizeKeepingCentre (28, 18);
    juce::Path wave;
    for (int i = 0; i <= 28; ++i)
    {
        const float t = float (i) / 28.0f;
        const juce::Point<float> pt (mark.getX() + t * mark.getWidth(), mark.getCentreY() + 5.0f * std::sin (t * 6.2832f));
        if (i == 0)
            wave.startNewSubPath (pt);
        else
            wave.lineTo (pt);
    }
    g.setColour (colours::accent2);
    g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::accent);
    g.fillRoundedRectangle (juce::Rectangle<float> (10.0f, 6.0f).withCentre ({ mark.getX() + 7.0f, mark.getCentreY() - 5.0f }), 1.5f);
    h.removeFromLeft (8);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText (proc.getName().toUpperCase(), h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" LEVEL_RIDER_VERSION_STRING, h, juce::Justification::centredRight);

    for (auto b : { topLeft, topRight, rangeBlock, riderBlock, outputBlock })
        drawPanel (g, b.toFloat());

    auto title = [&g] (juce::Rectangle<int> r, const juce::String& t) {
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (11.5f, true)));
        g.drawText (t, r.withHeight (22).withTrimmedTop (4), juce::Justification::centred);
    };
    title (topLeft, "TARGET  /  VOCAL LEVEL");
    title (topRight, "DETECTION");
    title (rangeBlock, "RANGE");
    title (riderBlock, "RIDER");
    title (outputBlock, "OUTPUT");

    // Ride / Idle status lights
    auto lightLabel = [&g] (juce::Rectangle<int> led, const char* text, bool on, juce::Colour c) {
        drawLed (g, led.toFloat(), c, on ? 1.0f : 0.0f);
        g.setColour (on ? colours::text : colours::textDim);
        g.setFont (juce::Font (uiFont (11.0f, true)));
        g.drawText (text, juce::Rectangle<int> (led.getRight() + 6, led.getCentreY() - 7, 40, 14), juce::Justification::centredLeft);
    };
    lightLabel (rideLed, "RIDE", display.riding, colours::ride);
    lightLabel (idleLed, "IDLE", ! display.riding, colours::accent);

    // music activity indicator: brightness follows the sidechain influence |T_eff - T|
    const bool musicOff = param (proc, ids::musicSens).getValue() <= 0.0f;
    const float influence = std::abs (display.musicOffsetDb) / 6.0f;
    drawLed (g, musicLed.toFloat(), colours::accent2, display.sidechainActive && ! musicOff ? 0.15f + 0.85f * influence : 0.0f);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.0f, true)));
    const juce::String musicText = musicOff ? "MUSIC OFF"
                                 : ! display.sidechainActive ? "NO SIDECHAIN"
                                                             : "MUSIC " + signedDb (display.musicOffsetDb) + " dB";
    g.drawText (musicText, juce::Rectangle<int> (musicLed.getX() - 30, musicLed.getBottom() + 4, musicLed.getWidth() + 60, 12),
                juce::Justification::centred);

    // range captions and effective range readout
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (9.5f, true)));
    g.drawText ("MAX", maxBox.getBounds().translated (0, -13).withHeight (12), juce::Justification::centred);
    g.drawText ("MIN", minBox.getBounds().translated (0, -13).withHeight (12), juce::Justification::centred);
    g.drawText ("IDLE", idleBox.getBounds().translated (0, -13).withHeight (12), juce::Justification::centred);
    const float effective = display.rangeMaxDb - display.rangeMinDb;
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.drawText ("EFFECTIVE RANGE  " + juce::String (effective, 1) + " dB",
                rangeBlock.withTrimmedTop (rangeBlock.getHeight() - 24).withTrimmedBottom (6), juce::Justification::centred);

    // automation caption
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f, true)));
    g.drawText ("AUTOMATION", automationSwitch.getBounds().translated (-100, 0).withWidth (92), juce::Justification::centredRight);
    if (proc.getAutomationMode() == dsp::AutomationMode::Write)
    {
        g.setColour (colours::warn);
        g.drawText ("writing Rider automation", automationSwitch.getBounds().translated (automationSwitch.getWidth() + 10, 0).withWidth (200),
                    juce::Justification::centredLeft);
    }
}

void LevelRiderEditor::resized()
{
    auto body = getLocalBounds().withTrimmedTop (headerH).reduced (margin, 0);
    body.removeFromTop (margin);

    // top: Target bar + status + attack | detection knobs
    auto top = body.removeFromTop (topH);
    topLeft = top.removeFromLeft (420);
    top.removeFromLeft (margin);
    topRight = top;

    auto tl = topLeft.reduced (12, 0).withTrimmedTop (24);
    auto barRow = tl.removeFromTop (58);
    targetBox.setBounds (barRow.removeFromRight (70).withSizeKeepingCentre (70, 22).translated (0, -4));
    barRow.removeFromRight (6);
    targetBar.setBounds (barRow);
    tl.removeFromTop (8);
    auto statusRow = tl.removeFromTop (30);
    rideLed = juce::Rectangle<int> (12, 12).withPosition (statusRow.getX() + 4, statusRow.getCentreY() - 6);
    idleLed = rideLed.translated (64, 0);
    attackSwitch.setBounds (statusRow.removeFromRight (150).withSizeKeepingCentre (150, 26));

    auto tr = topRight.reduced (10, 0).withTrimmedTop (22).withTrimmedBottom (8);
    const int knobs = spillKnob != nullptr ? 3 : 2;
    auto ledCol = tr.removeFromRight (90);
    musicLed = juce::Rectangle<int> (16, 16).withCentre (ledCol.getCentre().translated (0, -8));
    const int kw = tr.getWidth() / knobs;
    vocalKnob.setBounds (tr.removeFromLeft (kw));
    musicKnob.setBounds (tr.removeFromLeft (kw));
    if (spillKnob != nullptr)
        spillKnob->setBounds (tr.removeFromLeft (kw));

    // main: Range | Rider | Output
    body.removeFromTop (margin);
    auto main = body.removeFromTop (mainH);
    const int sideW = 212;
    rangeBlock = main.removeFromLeft (sideW);
    main.removeFromLeft (margin);
    outputBlock = main.removeFromRight (sideW);
    main.removeFromRight (margin);
    riderBlock = main;

    auto rb = rangeBlock.reduced (10, 0).withTrimmedTop (24).withTrimmedBottom (26);
    auto rbBoxes = rb.removeFromRight (74);
    rangeFader.setBounds (rb.withTrimmedLeft (14));
    maxBox.setBounds (rbBoxes.withHeight (22).withY (rb.getY() + 20).withSizeKeepingCentre (64, 22));
    idleBox.setBounds (rbBoxes.withHeight (22).withY (rb.getCentreY() - 4).withSizeKeepingCentre (64, 22));
    minBox.setBounds (rbBoxes.withHeight (22).withY (rb.getBottom() - 36).withSizeKeepingCentre (64, 22));

    auto rid = riderBlock.reduced (10, 0).withTrimmedTop (22).withTrimmedBottom (8);
    riderBox.setBounds (rid.removeFromBottom (28).withSizeKeepingCentre (96, 26));
    riderFader.setBounds (rid.withSizeKeepingCentre (220, rid.getHeight()));

    auto ob = outputBlock.reduced (10, 0).withTrimmedTop (24).withTrimmedBottom (8);
    outputBox.setBounds (ob.removeFromBottom (26).withSizeKeepingCentre (76, 22));
    outputFader.setBounds (ob.withSizeKeepingCentre (110, ob.getHeight()));

    // bottom: automation mode
    body.removeFromTop (margin);
    bottomBlock = body.removeFromTop (bottomH);
    automationSwitch.setBounds (bottomBlock.withSizeKeepingCentre (240, 28));
}

} // namespace craft::levelrider
