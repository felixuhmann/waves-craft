#include "PluginEditor.h"

namespace craft::doubler
{

using namespace ui;

namespace
{
constexpr int headerH = 40, margin = 12, gap = 10;
constexpr int leftW = 920, rightW = 150;
constexpr int displaysH = 210, stripH = 76, rowH = 40, rowHeaderH = 18, routingH = 38;

// voice-row columns: x offset and width inside the voice panel
struct Column
{
    int x, w;
    const char* caption;
};
constexpr Column columns[] = {
    { 14, 36, "Voice" },   { 72, 100, "Gain" },    { 192, 100, "Pan" },    { 316, 76, "Delay" }, { 410, 64, "Feedback" },
    { 492, 36, "Oct" },    { 548, 82, "Detune" },  { 648, 82, "Depth" },   { 748, 82, "Rate" },
};
} // namespace

juce::Button& DoublerEditor::addToggle (std::unique_ptr<juce::Button> b, const juce::String& id, const juce::String& tip)
{
    auto& ref = *b;
    ref.setTooltip (tip);
    addAndMakeVisible (ref);
    attachments.push_back (std::make_unique<Attachment> (proc.getState(), id, ref));
    owned.push_back (std::move (b));
    return ref;
}

Knob& DoublerEditor::addKnob (const juce::String& id, const juce::String& unit, juce::Colour accent)
{
    auto k = std::make_unique<Knob> (proc.getState(), id, unit, Knob::Layout::ValueRight);
    k->setAccent (accent);
    auto& ref = *k;
    addAndMakeVisible (ref);
    knobs.push_back (&ref);
    owned.push_back (std::move (k));
    return ref;
}

ValueBox& DoublerEditor::addBox (const juce::String& id, const juce::String& unit, juce::Colour accent, const juce::String& tip)
{
    auto b = std::make_unique<ValueBox> (proc.getState(), id, unit);
    b->setAccent (accent);
    b->setTooltip (tip + "  (drag; double-click to type; Alt-click resets)");
    auto& ref = *b;
    addAndMakeVisible (ref);
    boxes.push_back (&ref);
    owned.push_back (std::move (b));
    return ref;
}

DoublerEditor::DoublerEditor (DoublerProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.getState();
    const int nv = proc.getNumVoices();

    stage = std::make_unique<StageDisplay> (st, nv);
    plane = std::make_unique<DelayPlaneDisplay> (st, nv, [this] (int v) { return proc.getVoiceCents (v); });
    eq = std::make_unique<EqDisplay> (st);
    addAndMakeVisible (*stage);
    addAndMakeVisible (*plane);
    addAndMakeVisible (*eq);

    // middle strip
    using TB = ToggleButton;
    directOn = &addToggle (std::make_unique<TB> (TB::Style::Led, "On", colours::direct), ids::directOn, "Direct (dry) signal on/off");
    directGain = &addKnob (ids::directGain, {}, colours::direct);
    directPan = &addKnob (ids::directPan, {}, colours::direct);
    align = &addToggle (std::make_unique<SegmentSwitch> ("No", "Yes"), ids::align,
                        "Align Direct: delay the direct by the inherent latency so voice delays are relative to it");
    fbHighPass = &addBox (ids::fbHighPass, "Hz", colours::accent, "High-pass in every voice's feedback loop");
    range = &addToggle (std::make_unique<SegmentSwitch> ("80", "20"), ids::range,
                        "Range: lowest frequency the pitch shifter splices cleanly (80 Hz: shorter latency)");
    modReset = std::make_unique<MomentaryButton> (st, ids::modReset, "Reset");
    modReset->setTooltip ("Restart every voice's modulation at its nominal detune");
    addAndMakeVisible (*modReset);
    eqLowOn = &addToggle (std::make_unique<TB> (TB::Style::Led, "On", colours::accent), ids::eqLowOn, "Low shelf on/off");
    eqLowGain = &addKnob (ids::eqLowGain, {}, colours::accent);
    eqLowFreq = &addBox (ids::eqLowFreq, "Hz", colours::accent, "Low shelf frequency");
    eqHighOn = &addToggle (std::make_unique<TB> (TB::Style::Led, "On", colours::accent2), ids::eqHighOn, "High shelf on/off");
    eqHighGain = &addKnob (ids::eqHighGain, {}, colours::accent2);
    eqHighFreq = &addBox (ids::eqHighFreq, "Hz", colours::accent2, "High shelf frequency");

    // voice rows
    for (int n = 0; n < nv; ++n)
    {
        const auto c = colours::voice (n);
        VoiceRow r;
        r.on = &addToggle (std::make_unique<TB> (TB::Style::VoiceNumber, juce::String (n + 1), c), ids::voice (n, ids::on),
                           "Voice " + juce::String (n + 1) + " on/off");
        r.gain = &addKnob (ids::voice (n, ids::gain), "dB", c);
        r.pan = &addKnob (ids::voice (n, ids::pan), {}, c);
        r.delay = &addBox (ids::voice (n, ids::delay), "ms", c, "Delay");
        r.feedback = &addBox (ids::voice (n, ids::feedback), "%", c, "Feedback");
        r.octave = &addToggle (std::make_unique<TB> (TB::Style::OctaveArrow, "Oct", c), ids::voice (n, ids::octave),
                               "Shift this voice down one octave");
        r.detune = &addBox (ids::voice (n, ids::detune), "ct", c, "Detune");
        r.depth = &addBox (ids::voice (n, ids::depth), "ct", c, "Modulation depth (sign = initial direction)");
        r.rate = &addBox (ids::voice (n, ids::rate), "Hz", c, "Modulation rate");
        rows.push_back (r);
    }

    // routing
    routingA = std::make_unique<ChoiceSegments> (st, ids::routingA, juce::StringArray { "L", "L+R", "L-R" });
    routingB = std::make_unique<ChoiceSegments> (st, ids::routingB, juce::StringArray { "R", "R+L", "R-L" });
    routingA->setTooltip ("Input of voices 1 and 3: L, (L+R)/2 or (L-R)/2");
    routingB->setTooltip ("Input of voices 2 and 4: R, (R+L)/2 or (R-L)/2");
    addAndMakeVisible (*routingA);
    addAndMakeVisible (*routingB);

    output = std::make_unique<OutputSection> (st, ids::master);
    addAndMakeVisible (*output);

    const int h = headerH + margin + displaysH + gap + stripH + gap + rowHeaderH + nv * rowH + 14 + gap + routingH + margin;
    setSize (margin + leftW + margin + rightW + margin, h);
    startTimerHz (30);
}

DoublerEditor::~DoublerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DoublerEditor::timerCallback()
{
    for (auto* k : knobs)
        k->refresh();
    for (auto* b : boxes)
        b->refresh();
    stage->refresh();
    plane->refresh();
    eq->refresh();
    routingA->refresh();
    routingB->refresh();
    output->refresh();
    const auto pk = proc.takePeaks();
    output->pushPeaks (pk[0], pk[1]);
}

void DoublerEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header: logo mark (two offset waves = a doubled voice), product name, maker and version
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2));
    auto hb = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);
    auto mark = hb.removeFromLeft (30).toFloat().withSizeKeepingCentre (28, 18);
    for (int k = 0; k < 2; ++k)
    {
        juce::Path wave;
        for (float x = 0; x <= mark.getWidth(); x += 1.0f)
        {
            const float y = mark.getCentreY() + (k == 0 ? -2.5f : 2.5f)
                          + 5.0f * std::sin ((x / mark.getWidth()) * juce::MathConstants<float>::twoPi * 1.25f + (k == 0 ? 0.0f : 0.9f));
            if (x == 0)
                wave.startNewSubPath (mark.getX() + x, y);
            else
                wave.lineTo (mark.getX() + x, y);
        }
        g.setColour (k == 0 ? colours::accent : colours::accent2);
        g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    hb.removeFromLeft (8);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText (proc.getName().toUpperCase(), hb, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" DOUBLER_VERSION_STRING, hb, juce::Justification::centredRight);

    drawPanel (g, stripPanel.toFloat());
    drawPanel (g, voicePanel.toFloat());
    drawPanel (g, routingPanel.toFloat());
    drawPanel (g, outputPanel.toFloat());

    // group separators in the strip
    for (const auto& c : captions)
        drawCaption (g, c.r, c.text, c.j);

    // voice rows: colour bar + alternating shading
    for (size_t n = 0; n < rows.size(); ++n)
    {
        auto rowR = juce::Rectangle<int> (voicePanel.getX() + 4, voicePanel.getY() + rowHeaderH + 6 + int (n) * rowH,
                                          voicePanel.getWidth() - 8, rowH - 2);
        if (n % 2 == 1)
        {
            g.setColour (colours::background.withAlpha (0.35f));
            g.fillRoundedRectangle (rowR.toFloat(), 4.0f);
        }
        g.setColour (colours::voice (int (n)));
        g.fillRoundedRectangle (rowR.removeFromLeft (3).reduced (0, 6).toFloat(), 1.5f);
    }
}

void DoublerEditor::resized()
{
    captions.clear();
    auto area = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    outputPanel = area.removeFromRight (rightW);
    area.removeFromRight (margin);
    output->setBounds (outputPanel.reduced (8));

    // top row: three displays
    auto top = area.removeFromTop (displaysH);
    const int dw = (top.getWidth() - 2 * gap) / 3;
    stage->setBounds (top.removeFromLeft (dw));
    top.removeFromLeft (gap);
    plane->setBounds (top.removeFromLeft (dw));
    top.removeFromLeft (gap);
    eq->setBounds (top);
    area.removeFromTop (gap);

    // middle strip
    stripPanel = area.removeFromTop (stripH);
    {
        auto s = stripPanel.reduced (10, 6);
        auto group = [&] (int width, const juce::String& caption) {
            auto g = s.removeFromLeft (width);
            s.removeFromLeft (14);
            captions.push_back ({ g.removeFromTop (16), caption });
            return g.withSizeKeepingCentre (g.getWidth(), 34);
        };
        auto d = group (204, "Direct dB / Pan");
        directOn->setBounds (d.removeFromLeft (46).withSizeKeepingCentre (46, 24));
        d.removeFromLeft (6);
        directGain->setBounds (d.removeFromLeft (73));
        d.removeFromLeft (6);
        directPan->setBounds (d.removeFromLeft (73));
        align->setBounds (group (60, "Align").withSizeKeepingCentre (60, 24));
        fbHighPass->setBounds (group (64, "FB HP").withSizeKeepingCentre (64, 22));
        range->setBounds (group (60, "Range Hz").withSizeKeepingCentre (60, 24));
        modReset->setBounds (group (66, "Modulation").withSizeKeepingCentre (66, 24));
        auto eqGroup = [&] (juce::Rectangle<int> e, juce::Button* on, Knob* gain, ValueBox* freq) {
            on->setBounds (e.removeFromLeft (44).withSizeKeepingCentre (44, 24));
            e.removeFromLeft (4);
            gain->setBounds (e.removeFromLeft (73));
            e.removeFromLeft (4);
            freq->setBounds (e.withSizeKeepingCentre (e.getWidth(), 22));
        };
        eqGroup (group (184, "EQ Low dB / Hz"), eqLowOn, eqLowGain, eqLowFreq);
        eqGroup (group (184, "EQ High dB / Hz"), eqHighOn, eqHighGain, eqHighFreq);
    }
    area.removeFromTop (gap);

    // routing strip at the bottom
    routingPanel = area.removeFromBottom (routingH);
    area.removeFromBottom (gap);
    {
        auto r = routingPanel.reduced (12, 7);
        const bool four = proc.getNumVoices() == 4;
        captions.push_back ({ r.removeFromLeft (60), "Input", juce::Justification::centredLeft });
        captions.push_back ({ r.removeFromLeft (80), four ? "Voices 1/3" : "Voice 1", juce::Justification::centredRight });
        r.removeFromLeft (8);
        routingA->setBounds (r.removeFromLeft (160));
        r.removeFromLeft (30);
        captions.push_back ({ r.removeFromLeft (80), four ? "Voices 2/4" : "Voice 2", juce::Justification::centredRight });
        r.removeFromLeft (8);
        routingB->setBounds (r.removeFromLeft (160));
    }

    // voice rows
    voicePanel = area;
    for (const auto& c : columns)
        captions.push_back ({ { voicePanel.getX() + c.x - 6, voicePanel.getY() + 6, c.w + 12, rowHeaderH - 4 }, c.caption });
    for (size_t n = 0; n < rows.size(); ++n)
    {
        const int y = voicePanel.getY() + rowHeaderH + 6 + int (n) * rowH;
        auto cell = [&] (int col, int h) {
            const auto& c = columns[col];
            return juce::Rectangle<int> (voicePanel.getX() + c.x, y + (rowH - 2 - h) / 2, c.w, h);
        };
        auto& r = rows[n];
        r.on->setBounds (cell (0, 28));
        r.gain->setBounds (cell (1, 34));
        r.pan->setBounds (cell (2, 34));
        r.delay->setBounds (cell (3, 22));
        r.feedback->setBounds (cell (4, 22));
        r.octave->setBounds (cell (5, 26));
        r.detune->setBounds (cell (6, 22));
        r.depth->setBounds (cell (7, 22));
        r.rate->setBounds (cell (8, 22));
    }
}

} // namespace craft::doubler
