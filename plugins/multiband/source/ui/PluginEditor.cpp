#include "PluginEditor.h"

namespace craft::multiband
{

using namespace ui;

namespace
{
constexpr int headerH = 40, margin = 12, gap = 10;
constexpr int displayH = 250, lowerH = 330;
constexpr int bandW = 168, masterW = 236, outputW = 112;
constexpr int rowLabelH = 13, rowBoxH = 22, rowGap = 6;

const char* const bandTitles[] = { "Low", "Low Mid", "High Mid", "High" };
const char* const rowTitles[] = { "Threshold", "Gain", "Range", "Attack", "Release" };
const char* const rowUnits[] = { "dB", "dB", "dB", "ms", "ms" };
} // namespace

MultibandEditor::MultibandEditor (MultibandProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      display (p.getState()),
      knee (*p.getState().getParameter (ids::knee), ids::knee),
      q (*p.getState().getParameter (ids::xoverQ), ids::xoverQ),
      releaseMode (*p.getState().getParameter (ids::autoRelease), "Manual", "Auto"),
      behavior (*p.getState().getParameter (ids::opto), "Electro", "Opto"),
      output (p.getState(), ids::outputGain)
{
    using BP = ids::BandParam;
    setLookAndFeel (&lnf);
    auto& st = p.getState();
    addAndMakeVisible (display);

    const BP rowParams[] = { BP::threshold, BP::gain, BP::range, BP::attack, BP::release };
    for (int b = 0; b < numBands; ++b)
    {
        auto& col = columns[size_t (b)];
        col.meter = std::make_unique<ThresholdMeter> (*st.getParameter (ids::band (b, BP::threshold)), colours::band (b));
        addAndMakeVisible (*col.meter);
        for (int r = 0; r < 5; ++r)
        {
            const auto id = ids::band (b, rowParams[r]);
            col.boxes[size_t (r)] = std::make_unique<ValueBox> (*st.getParameter (id), id, rowUnits[r]);
            col.boxes[size_t (r)]->setAccent (colours::band (b));
            addAndMakeVisible (*col.boxes[size_t (r)]);
        }
        col.solo = std::make_unique<PillToggle> ("SOLO", colours::solo);
        col.bypass = std::make_unique<PillToggle> ("BYPASS", colours::bypass);
        col.solo->setTooltip ("Solo: listen to the soloed bands only");
        col.bypass->setTooltip ("Bypass: this band at unity gain, no dynamics");
        addAndMakeVisible (*col.solo);
        addAndMakeVisible (*col.bypass);
        col.soloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, ids::band (b, BP::solo), *col.solo);
        col.bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, ids::band (b, BP::bypass), *col.bypass);
    }

    for (int r = 0; r < 5; ++r)
    {
        std::array<juce::RangedAudioParameter*, 4> ps {};
        for (int b = 0; b < numBands; ++b)
            ps[size_t (b)] = st.getParameter (ids::band (b, rowParams[r]));
        std::function<juce::String (double)> describe;
        if (r <= 2)
        {
            const double span = r == 0 ? 80.0 : (r == 1 ? 36.0 : 42.0);
            describe = [span] (double d) { return (d >= 0 ? "+" : "") + juce::String (d * span, 1) + " dB"; };
        }
        else
        {
            describe = [] (double d) { return juce::String::charToString (0x00d7) + juce::String (std::pow (1000.0, d), 2); };
        }
        offsets[size_t (r)] = std::make_unique<OffsetBox> (ps, describe);
        addAndMakeVisible (*offsets[size_t (r)]);
    }
    addAndMakeVisible (knee);
    addAndMakeVisible (q);
    addAndMakeVisible (releaseMode);
    addAndMakeVisible (behavior);
    addAndMakeVisible (output);

    setSize (margin + numBands * bandW + (numBands - 1) * gap + gap + masterW + gap + outputW + margin,
             headerH + margin + displayH + gap + lowerH + margin);
    startTimerHz (30);
}

MultibandEditor::~MultibandEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MultibandEditor::timerCallback()
{
    const auto m = proc.takeMeters();
    display.setLiveGains (m.bandGainDb, proc.getCurrentSampleRate());
    for (int b = 0; b < numBands; ++b)
    {
        auto& col = columns[size_t (b)];
        col.meter->pushLevel (m.bandLevel[size_t (b)]);
        for (auto& box : col.boxes)
            box->refresh();
    }
    knee.refresh();
    q.refresh();
    releaseMode.refresh();
    behavior.refresh();
    output.setChannels (m.channels);
    output.pushLevels (m.output[0], m.channels == 2 ? m.output[1] : m.output[0]);

    // gain-change readout under each band title
    repaint (columns[0].area.getUnion (columns[3].area).withHeight (34));
    lastGains = m.bandGainDb;
}

void MultibandEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);

    // logo mark: four stacked bars of different heights (bands), each with its own colour
    auto mark = h.removeFromLeft (30).toFloat().withSizeKeepingCentre (26, 20);
    const float heights[] = { 0.55f, 1.0f, 0.75f, 0.4f };
    for (int b = 0; b < 4; ++b)
    {
        const float w = mark.getWidth() / 4.0f;
        const float hh = mark.getHeight() * heights[b];
        g.setColour (colours::band (b));
        g.fillRoundedRectangle (mark.getX() + float (b) * w + 1.0f, mark.getBottom() - hh, w - 2.0f, hh, 1.5f);
    }
    h.removeFromLeft (6);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText ("CRAFT MULTIBAND", h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" CRAFT_VERSION_STRING, h, juce::Justification::centredRight);

    drawPanel (g, displayArea.toFloat());

    for (int b = 0; b < numBands; ++b)
    {
        const auto& col = columns[size_t (b)];
        drawPanel (g, col.area.toFloat());
        auto top = col.area.reduced (8, 6);
        auto titleRow = top.removeFromTop (16);
        g.setColour (colours::band (b));
        g.fillRoundedRectangle (titleRow.removeFromLeft (4).toFloat(), 1.5f);
        titleRow.removeFromLeft (6);
        drawTitle (g, titleRow, juce::String (b + 1) + "  " + bandTitles[b], juce::Justification::centredLeft, colours::text);
        // live gain readout
        g.setFont (juce::Font (uiFont (11.0f, true)));
        g.setColour (colours::band (b));
        g.drawText (param::formatDb (lastGains[size_t (b)]) + " dB", titleRow, juce::Justification::centredRight);

        for (int r = 0; r < 5; ++r)
        {
            const auto bb = col.boxes[size_t (r)]->getBounds();
            g.setColour (colours::textDim);
            g.setFont (juce::Font (uiFont (10.0f, true)));
            g.drawText (juce::String (rowTitles[r]).toUpperCase(), bb.withY (bb.getY() - rowLabelH).withHeight (rowLabelH),
                        juce::Justification::centredLeft);
        }
    }

    drawPanel (g, masterArea.toFloat());
    drawTitle (g, masterArea.reduced (10, 6).removeFromTop (16), "Master", juce::Justification::centredLeft, colours::text);
    g.setFont (juce::Font (uiFont (10.0f, true)));
    g.setColour (colours::textDim);
    for (int r = 0; r < 5; ++r)
        g.drawText (juce::String (rowTitles[r]).toUpperCase(), offsetLabels[size_t (r)], juce::Justification::centredLeft);
    g.drawText ("KNEE", kneeLabel, juce::Justification::centredLeft);
    g.drawText ("CROSSOVER Q", qLabel, juce::Justification::centredLeft);
    g.drawText ("RELEASE", releaseLabel, juce::Justification::centredLeft);
    g.drawText ("BEHAVIOR", behaviorLabel, juce::Justification::centredLeft);
    g.setFont (juce::Font (uiFont (9.5f)));
    g.drawText ("ALL BANDS (OFFSET)", offsetLabels[0].withY (offsetLabels[0].getY() - 16).withHeight (12),
                juce::Justification::centredLeft);

    drawPanel (g, outputArea.toFloat());
}

void MultibandEditor::resized()
{
    auto r = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    displayArea = r.removeFromTop (displayH);
    display.setBounds (displayArea.reduced (6));
    r.removeFromTop (gap);

    for (int b = 0; b < numBands; ++b)
    {
        auto& col = columns[size_t (b)];
        col.area = r.removeFromLeft (bandW);
        r.removeFromLeft (gap);
        auto c = col.area.reduced (8, 6);
        c.removeFromTop (22);
        auto buttons = c.removeFromBottom (22);
        col.solo->setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 3));
        buttons.removeFromLeft (6);
        col.bypass->setBounds (buttons);
        c.removeFromBottom (8);
        col.meter->setBounds (c.removeFromLeft (58));
        c.removeFromLeft (6);
        for (int k = 0; k < 5; ++k)
        {
            c.removeFromTop (rowLabelH);
            col.boxes[size_t (k)]->setBounds (c.removeFromTop (rowBoxH));
            c.removeFromTop (rowGap + 3);
        }
    }

    masterArea = r.removeFromLeft (masterW);
    r.removeFromLeft (gap);
    outputArea = r;
    output.setBounds (outputArea.reduced (8, 6));

    auto m = masterArea.reduced (10, 6);
    m.removeFromTop (38);
    auto left = m.removeFromLeft ((m.getWidth() - 10) / 2);
    m.removeFromLeft (10);
    auto right = m;
    for (int k = 0; k < 5; ++k)
    {
        offsetLabels[size_t (k)] = left.removeFromTop (rowLabelH);
        offsets[size_t (k)]->setBounds (left.removeFromTop (rowBoxH));
        left.removeFromTop (rowGap + 3);
    }
    auto place = [&right] (juce::Rectangle<int>& label, juce::Component& comp, int hgt) {
        label = right.removeFromTop (rowLabelH);
        comp.setBounds (right.removeFromTop (hgt));
        right.removeFromTop (rowGap + 3);
    };
    place (kneeLabel, knee, rowBoxH);
    place (qLabel, q, rowBoxH);
    place (releaseLabel, releaseMode, 24);
    place (behaviorLabel, behavior, 24);
}

} // namespace craft::multiband
