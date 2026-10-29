#include "PluginEditor.h"

namespace craft::dynamics
{

using namespace ui;

namespace
{
constexpr int margin = 12, headerH = 40, topRowH = 312, bottomRowH = 200;
constexpr int graphW = 312, moduleW = 334, outputW = 236;
constexpr int filterW = 430, globalW = 300;
constexpr int captionH = 14, boxH = 24, groupGap = 8;
} // namespace

DynamicsEditor::DynamicsEditor (DynamicsProcessor& p) : AudioProcessorEditor (p), proc (p), info (p.getInfo())
{
    setLookAndFeel (&lnf);
    auto& st = proc.getState();
    const auto amber = colours::accent, teal = colours::accent2;
    const juce::String eqTip = "EQ mode: Wideband (full-band detection and gain), Sidechain (detection through the filter), "
                               "Split (detection through the filter, gain on the active band only)";

    transfer = std::make_unique<TransferGraph> (st, info.comp, info.gate);
    addAndMakeVisible (*transfer);

    if (info.comp)
    {
        box (ids::compThreshold, amber, "Compressor threshold");
        box (ids::ratio, amber, "Ratio: below 1:1 expands upward, negative ratios over-compress (cancellation)");
        box (ids::makeup, amber, "Makeup gain");
        box (ids::compAttack, amber, "Compressor attack");
        box (ids::compRelease, amber, "Compressor release");
        box (ids::pdr, amber, "Program-dependent release: short peaks release fast, sustained material with Release");
        segments (ids::levelRef, { "LowRef", "PeakRef" }, amber, "LowRef: curve relative to the threshold; PeakRef: gain referenced to peak level");
        toggle (ids::compBypass, "BYPASS", colours::warn, "Bypass the compressor/expander");
        if (info.filter)
            segments (ids::compEq, { "Wide", "Sidechain", "Split" }, amber, eqTip);
        compMeters = std::make_unique<ModuleMeters> (amber);
        addAndMakeVisible (*compMeters);
    }
    if (info.gate)
    {
        segments (ids::gateMode, { "Gate", "Expander" }, teal, "Gate (open/close with hysteresis and hold) or downward expander");
        box (ids::gateOpen, teal, "Gate open threshold (expander threshold)");
        box (ids::gateClose, teal, "Gate close threshold (-inf: same as open)");
        box (ids::floor, teal, "Floor: gain while closed. Negative-polarity values (N) create a cancellation notch in expander mode");
        box (ids::gateAttack, teal, "Gate attack");
        box (ids::gateRelease, teal, "Gate release");
        box (ids::hold, teal, "Hold: minimum open time");
        toggle (ids::gateBypass, "BYPASS", colours::warn, "Bypass the gate/expander");
        if (info.filter)
            segments (ids::gateEq, { "Wide", "Sidechain", "Split" }, teal, eqTip);
        gateMeters = std::make_unique<ModuleMeters> (teal);
        addAndMakeVisible (*gateMeters);
    }
    if (info.filter)
    {
        filterGraph = std::make_unique<FilterGraph> (st);
        addAndMakeVisible (*filterGraph);
        segments (ids::filterType, { "LP", "BP", "HP", "BR" }, amber, "Filter type: low-pass, band-pass, high-pass, band-reject");
        box (ids::filterFreq, amber, "Filter frequency");
        box (ids::filterQ, amber, "Filter Q");
        box (ids::inputGainL, colours::text, "Input gain, left");
        box (ids::inputGainR, colours::text, "Input gain, right");
        segments (ids::keyMode, { "Stereo", "L > R", "R > L" }, colours::text,
                  "Key: Stereo (linked), L > R (left keys the right channel; only right is output), R > L");
        segments (ids::monitor, { "Audio", "Sidechain", "Passive" }, colours::text,
                  "Monitor: the processed audio, the detection signal, or the passive band (+6 dB)");
        toggle (ids::lookahead, "LOOKAHEAD", amber, "LookAhead: delays the audio by 340 samples (reported as latency)");
    }
    if (info.comp && info.gate)
    {
        toggle (ids::linkThreshold, "THRESH", colours::text, "Move the compressor threshold and gate open threshold together",
                ParamToggle::Icon::Link);
        toggle (ids::linkAttack, "ATTACK", colours::text, "Move both attack times together", ParamToggle::Icon::Link);
        toggle (ids::linkRelease, "RELEASE", colours::text, "Move both release times together", ParamToggle::Icon::Link);
    }

    outFader = std::make_unique<Fader> (st, ids::outputGain,
                                        std::vector<std::pair<double, juce::String>> { { 18.0, "+18" }, { 12.0, "+12" }, { 6.0, "+6" },
                                                                                       { 0.0, "0" }, { -12.0, "-12" }, { -24.0, "-24" },
                                                                                       { -40.0, "-40" } });
    addAndMakeVisible (*outFader);
    ioMeters = std::make_unique<IoMeters> (proc.getNumChannelsInUse());
    addAndMakeVisible (*ioMeters);

    const int width = info.gate && info.comp ? 2 * margin + graphW + margin + 2 * moduleW + margin
                                             : 2 * margin + graphW + margin + moduleW + margin + outputW;
    const int height = headerH + margin + topRowH + margin + (info.filter ? bottomRowH + margin : 0);
    setSize (width, height);
    startTimerHz (30);
}

DynamicsEditor::~DynamicsEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

ValueBox* DynamicsEditor::box (const char* id, juce::Colour c, const juce::String& tooltip)
{
    auto b = std::make_unique<ValueBox> (*proc.getState().getParameter (id), c);
    b->setTooltip (tooltip + "\nDrag to change, click to type, double-click to reset");
    addAndMakeVisible (*b);
    byId[id] = b.get();
    boxes.push_back (std::move (b));
    return boxes.back().get();
}

SegmentedChoice* DynamicsEditor::segments (const char* id, juce::StringArray labels, juce::Colour c, const juce::String& tooltip)
{
    auto s = std::make_unique<SegmentedChoice> (*proc.getState().getParameter (id), std::move (labels), c);
    s->setTooltip (tooltip);
    addAndMakeVisible (*s);
    auto* raw = s.get();
    byId[id] = raw;
    others.push_back (std::move (s));
    return raw;
}

ParamToggle* DynamicsEditor::toggle (const char* id, const juce::String& text, juce::Colour c, const juce::String& tooltip, ParamToggle::Icon icon)
{
    auto t = std::make_unique<ParamToggle> (*proc.getState().getParameter (id), text, c, icon);
    t->setTooltip (tooltip);
    addAndMakeVisible (*t);
    auto* raw = t.get();
    byId[id] = raw;
    others.push_back (std::move (t));
    return raw;
}

void DynamicsEditor::timerCallback()
{
    for (auto& b : boxes)
        b->refresh();
    outFader->refresh();

    const auto s = proc.readSettings();
    const auto m = proc.takeMeters();
    transfer->update (s, m.compLevelDb, m.gateLevelDb);
    if (compMeters != nullptr)
        compMeters->setValues (s.compBypass ? 0.0f : m.compGainDb, m.compLevelDb);
    if (gateMeters != nullptr)
        gateMeters->setValues (s.gateBypass ? 0.0f : m.gateGainDb, m.gateLevelDb);
    if (filterGraph != nullptr)
        filterGraph->update (s, proc.getCurrentSampleRate());
    ioMeters->push (m.inPeak, m.outPeak);
}

juce::Rectangle<int> DynamicsEditor::content (juce::Rectangle<int> panel)
{
    auto r = panel.reduced (12, 8);
    r.removeFromTop (26);
    return r;
}

juce::Rectangle<int> DynamicsEditor::captioned (juce::Rectangle<int> area, const juce::String& caption)
{
    captions.emplace_back (area.removeFromTop (captionH), caption);
    return area.removeFromTop (boxH);
}

void DynamicsEditor::layoutComp (juce::Rectangle<int> panel)
{
    panels.push_back ({ panel, "Compressor / Expander", colours::accent });
    byId[ids::compBypass]->setBounds (panel.reduced (12, 8).removeFromTop (20).removeFromRight (72));
    auto a = content (panel);
    compMeters->setBounds (a.removeFromRight (64));
    a.removeFromRight (10);
    const int colW = (a.getWidth() - 2 * groupGap) / 3;
    auto row = [&] (const char* i0, const char* c0, const char* i1, const char* c1, const char* i2, const char* c2) {
        auto r = a.removeFromTop (captionH + boxH);
        byId[i0]->setBounds (captioned (r.removeFromLeft (colW), c0));
        r.removeFromLeft (groupGap);
        byId[i1]->setBounds (captioned (r.removeFromLeft (colW), c1));
        r.removeFromLeft (groupGap);
        byId[i2]->setBounds (captioned (r, c2));
        a.removeFromTop (groupGap + 4);
    };
    row (ids::compThreshold, "THRESHOLD", ids::ratio, "RATIO", ids::makeup, "MAKEUP");
    row (ids::compAttack, "ATTACK", ids::compRelease, "RELEASE", ids::pdr, "PDR");
    byId[ids::levelRef]->setBounds (captioned (a.removeFromTop (captionH + boxH), "LEVEL REFERENCE"));
    a.removeFromTop (groupGap + 4);
    if (info.filter)
        byId[ids::compEq]->setBounds (captioned (a.removeFromTop (captionH + boxH), "EQ MODE"));
}

void DynamicsEditor::layoutGate (juce::Rectangle<int> panel)
{
    panels.push_back ({ panel, "Gate / Expander", colours::accent2 });
    byId[ids::gateBypass]->setBounds (panel.reduced (12, 8).removeFromTop (20).removeFromRight (72));
    auto a = content (panel);
    gateMeters->setBounds (a.removeFromRight (64));
    a.removeFromRight (10);
    const int colW = (a.getWidth() - 2 * groupGap) / 3;
    byId[ids::gateMode]->setBounds (captioned (a.removeFromTop (captionH + boxH), "MODE"));
    a.removeFromTop (groupGap + 4);
    auto row = [&] (const char* i0, const char* c0, const char* i1, const char* c1, const char* i2, const char* c2) {
        auto r = a.removeFromTop (captionH + boxH);
        byId[i0]->setBounds (captioned (r.removeFromLeft (colW), c0));
        r.removeFromLeft (groupGap);
        byId[i1]->setBounds (captioned (r.removeFromLeft (colW), c1));
        r.removeFromLeft (groupGap);
        byId[i2]->setBounds (captioned (r, c2));
        a.removeFromTop (groupGap + 4);
    };
    row (ids::gateOpen, "OPEN", ids::gateClose, "CLOSE", ids::floor, "FLOOR");
    row (ids::gateAttack, "ATTACK", ids::gateRelease, "RELEASE", ids::hold, "HOLD");
    if (info.filter)
        byId[ids::gateEq]->setBounds (captioned (a.removeFromTop (captionH + boxH), "EQ MODE"));
}

void DynamicsEditor::layoutFilter (juce::Rectangle<int> panel)
{
    panels.push_back ({ panel, "Filter module", colours::passive });
    auto a = content (panel);
    auto right = a.removeFromRight (124);
    a.removeFromRight (10);
    filterGraph->setBounds (a);
    byId[ids::filterType]->setBounds (captioned (right.removeFromTop (captionH + boxH), "TYPE"));
    right.removeFromTop (groupGap + 4);
    byId[ids::filterFreq]->setBounds (captioned (right.removeFromTop (captionH + boxH), "FREQ"));
    right.removeFromTop (groupGap + 4);
    byId[ids::filterQ]->setBounds (captioned (right.removeFromTop (captionH + boxH), "Q"));
}

void DynamicsEditor::layoutGlobal (juce::Rectangle<int> panel)
{
    panels.push_back ({ panel, "Global", colours::text });
    auto a = content (panel);
    auto r = a.removeFromTop (captionH + boxH);
    const int half = (r.getWidth() - groupGap) / 2;
    byId[ids::inputGainL]->setBounds (captioned (r.removeFromLeft (half), "INPUT L"));
    r.removeFromLeft (groupGap);
    byId[ids::inputGainR]->setBounds (captioned (r, "INPUT R"));
    a.removeFromTop (6);
    byId[ids::keyMode]->setBounds (captioned (a.removeFromTop (captionH + boxH), "KEY"));
    a.removeFromTop (6);
    byId[ids::monitor]->setBounds (captioned (a.removeFromTop (captionH + boxH), "MONITOR"));
    a.removeFromTop (8);
    auto bottom = a.removeFromTop (boxH);
    if (info.comp && info.gate)
    {
        const int w = (bottom.getWidth() - 3 * 6) / 4;
        byId[ids::lookahead]->setBounds (bottom.removeFromLeft (w + 8));
        for (auto* id : { ids::linkThreshold, ids::linkAttack, ids::linkRelease })
        {
            bottom.removeFromLeft (6);
            byId[id]->setBounds (bottom.removeFromLeft (w - 3));
        }
    }
    else
        byId[ids::lookahead]->setBounds (bottom.removeFromLeft (110));
}

void DynamicsEditor::layoutOutput (juce::Rectangle<int> panel)
{
    panels.push_back ({ panel, "Output", colours::text });
    auto a = content (panel);
    captions.emplace_back (a.removeFromTop (captionH).removeFromLeft (64), "GAIN");
    outFader->setBounds (a.removeFromLeft (64));
    a.removeFromLeft (6);
    ioMeters->setBounds (a.withTrimmedTop (-captionH));
}

void DynamicsEditor::resized()
{
    panels.clear();
    captions.clear();
    auto body = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    auto top = body.removeFromTop (topRowH);
    body.removeFromTop (margin);

    transfer->setBounds (top.removeFromLeft (graphW));
    panels.push_back ({ transfer->getBounds(), {}, {} }); // graph background panel (no title)
    top.removeFromLeft (margin);
    if (info.comp)
    {
        layoutComp (top.removeFromLeft (moduleW));
        top.removeFromLeft (margin);
    }
    if (info.gate)
    {
        layoutGate (top.removeFromLeft (moduleW));
        top.removeFromLeft (margin);
    }
    const bool outputInTop = ! (info.comp && info.gate);
    if (outputInTop)
        layoutOutput (top);

    if (info.filter)
    {
        layoutFilter (body.removeFromLeft (filterW));
        body.removeFromLeft (margin);
        if (outputInTop)
            layoutGlobal (body);
        else
        {
            layoutGlobal (body.removeFromLeft (globalW));
            body.removeFromLeft (margin);
            layoutOutput (body);
        }
    }
    // the transfer graph sits inside its panel with a small inset
    transfer->setBounds (panels.front().bounds.reduced (8));
}

void DynamicsEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2).removeFromLeft (getWidth() / 2));
    g.setColour (colours::accent2);
    g.fillRect (juce::Rectangle<int> (getWidth() / 2, headerH - 2, getWidth() - getWidth() / 2, 2));

    // logo mark: a knee - unity line bending into a flatter slope
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);
    auto mark = h.removeFromLeft (28).toFloat().withSizeKeepingCentre (24, 20);
    juce::Path knee;
    knee.startNewSubPath (mark.getX(), mark.getBottom());
    knee.lineTo (mark.getX() + mark.getWidth() * 0.5f, mark.getY() + mark.getHeight() * 0.45f);
    knee.quadraticTo (mark.getX() + mark.getWidth() * 0.62f, mark.getY() + mark.getHeight() * 0.3f, mark.getRight(),
                      mark.getY() + mark.getHeight() * 0.18f);
    g.setColour (colours::accent);
    g.strokePath (knee, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::accent2);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ mark.getX() + mark.getWidth() * 0.5f, mark.getY() + mark.getHeight() * 0.45f }));
    h.removeFromLeft (8);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText (proc.getName().toUpperCase(), h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" CRAFT_VERSION_STRING, h, juce::Justification::centredRight);

    for (const auto& p : panels)
    {
        drawPanel (g, p.bounds.toFloat());
        if (p.title.isNotEmpty())
            drawSectionTitle (g, p.bounds.reduced (12, 8).removeFromTop (20).withTrimmedRight (80), p.title, p.colour);
    }
    g.setFont (juce::Font (uiFont (10.0f, true)));
    g.setColour (colours::textDim);
    for (const auto& [r, text] : captions)
        g.drawText (text, r, juce::Justification::centredLeft);
}

} // namespace craft::dynamics
