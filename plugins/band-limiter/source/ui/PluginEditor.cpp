#include "PluginEditor.h"

namespace craft::band_limiter
{

using namespace ui;

namespace
{
constexpr int headerH = 38, margin = 10;
const juce::Colour bandColours[dsp::numBands] = { juce::Colour (0xffd9775f), juce::Colour (0xffe3a447), juce::Colour (0xffb9c46b),
                                                  juce::Colour (0xff58c4b6), juce::Colour (0xff7aa2e0) };
} // namespace

std::unique_ptr<ValueBox> BandLimiterEditor::makeBox (juce::RangedAudioParameter* p, const juce::String& tip)
{
    auto box = std::make_unique<ValueBox> (*p, [p] { return p->getCurrentValueAsText(); });
    box->setTooltip (tip + "\nDrag to change, click to type, double-click to reset");
    addAndMakeVisible (*box);
    return box;
}

BandLimiterEditor::BandLimiterEditor (BandLimiterProcessor& p)
    : AudioProcessorEditor (p), proc (p), profileLayout (dsp::isProfileVariant (BandLimiterProcessor::variant))
{
    setLookAndFeel (&lnf);
    auto& st = proc.getState();
    threshold = st.getParameter (ids::threshold);
    ceiling = st.getParameter (ids::ceiling);

    inputMeter = std::make_unique<LevelMeter> (threshold, colours::accent);
    inputMeter->setTooltip ("Input level - drag the marker to set the Threshold");
    outputMeter = std::make_unique<LevelMeter> (ceiling, colours::accent2);
    outputMeter->setTooltip ("Output level - drag the marker to set the Out Ceiling");
    inputMeter->onMarkerDrag = [this] (float db) { moveLevel (threshold, db); };
    outputMeter->onMarkerDrag = [this] (float db) { moveLevel (ceiling, db); };
    addAndMakeVisible (*inputMeter);
    addAndMakeVisible (*outputMeter);
    attenuation.setTooltip ("Attenuation, 0 to 30 dB");
    addAndMakeVisible (attenuation);

    thresholdBox = makeBox (threshold, "Threshold: make-up equals -Threshold, the sum of the bands stays at this level");
    ceilingBox = makeBox (ceiling, "Out Ceiling: highest output sample peak");
    levelLink.setClickingTogglesState (true);
    levelLink.setTooltip ("Move the Threshold and Out Ceiling markers together");
    addAndMakeVisible (levelLink);

    if (profileLayout)
    {
        profile = std::make_unique<SegmentedChoice> (*st.getParameter (ids::profile),
                                                     juce::StringArray { "1", "2", "3", "4", "5", "6", "7", "8", "9" });
        profile->setTooltip ("1 Standard, 2 Relaxed, 3 Merged, 4 Broad, 5 Broad Fixed, 6 Warm Balance, 7 Long, 8 Bright, 9 Low Weight");
        addAndMakeVisible (*profile);
        multiplierBox = makeBox (st.getParameter (ids::multiplier), "Release multiplier: scales all band releases of the profile");
    }
    else
    {
        std::array<juce::RangedAudioParameter*, dsp::numCrossovers> xo;
        std::array<juce::RangedAudioParameter*, dsp::numBands> g, pr;
        for (int b = 0; b < dsp::numBands; ++b)
        {
            g[size_t (b)] = st.getParameter (ids::gain (b));
            pr[size_t (b)] = st.getParameter (ids::priority (b));
        }
        for (int k = 0; k < dsp::numCrossovers; ++k)
        {
            xo[size_t (k)] = st.getParameter (ids::crossover (k));
            crossoverBoxes[size_t (k)] = makeBox (xo[size_t (k)], juce::String (crossoverNames[k]) + " crossover");
        }
        display = std::make_unique<BandDisplay> (xo, g, pr);
        addAndMakeVisible (*display);

        for (int b = 0; b < dsp::numBands; ++b)
        {
            auto& s = strips[size_t (b)];
            s.solo = std::make_unique<juce::TextButton> ("Solo");
            s.solo->setClickingTogglesState (true);
            s.solo->setTooltip ("Hear only the soloed bands");
            s.soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, ids::solo (b), *s.solo);
            addAndMakeVisible (*s.solo);
            s.link = std::make_unique<juce::TextButton> ("G=P");
            s.link->setClickingTogglesState (true);
            s.link->setTooltip ("Link this band's Gain and Priority: the band marker moves both");
            s.link->onClick = [this, b] {
                std::array<bool, dsp::numBands> linked {};
                for (int i = 0; i < dsp::numBands; ++i)
                    linked[size_t (i)] = strips[size_t (i)].link->getToggleState();
                display->setLinked (linked);
                juce::ignoreUnused (b);
            };
            addAndMakeVisible (*s.link);
            s.gain = makeBox (g[size_t (b)], "Band gain, before the limiter");
            s.priority = makeBox (pr[size_t (b)], "Priority: shifts attenuation away from this band to the others");
            s.release = makeBox (st.getParameter (ids::release (b)), "Band release");
        }
        separationBox = makeBox (st.getParameter (ids::separation), "Separation: 100 % each band's detector sees only its band, 0 % all bands");
        if (BandLimiterProcessor::variant == dsp::Variant::LinearMulti)
        {
            behaviour = std::make_unique<SegmentedChoice> (*st.getParameter (ids::behaviour),
                                                           juce::StringArray { "Adapt", "Aggr", "Warm", "Scaled", "Manual" });
            behaviour->setTooltip ("Release behaviour: Manual has no program-adaptive shortening");
            addAndMakeVisible (*behaviour);
        }
    }

    quantize = std::make_unique<SegmentedChoice> (*st.getParameter (ids::quantize), juce::StringArray { "24", "22", "20", "18", "16" });
    quantize->setTooltip ("Output word length (bits)");
    dither = std::make_unique<SegmentedChoice> (*st.getParameter (ids::dither), juce::StringArray { "None", "Type 1", "Type 2" });
    dither->setTooltip ("Dither: Type 1 also runs on digital silence, Type 2 stays silent on digital silence");
    shaping = std::make_unique<SegmentedChoice> (*st.getParameter (ids::shaping), juce::StringArray { "None", "Mod", "Norm", "Ultra" });
    shaping->setTooltip ("Noise shaping");
    for (auto* c : { quantize.get(), dither.get(), shaping.get() })
        addAndMakeVisible (*c);

    setSize (profileLayout ? 560 : 960, profileLayout ? 400 : 620);
    startTimerHz (30);
}

BandLimiterEditor::~BandLimiterEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BandLimiterEditor::moveLevel (juce::RangedAudioParameter* moved, float db)
{
    auto valueOf = [] (juce::RangedAudioParameter* q) { return q->convertFrom0to1 (q->getValue()); };
    if (levelLink.getToggleState())
    {
        auto* other = moved == threshold ? ceiling : threshold;
        const float delta = juce::jlimit (-30.0f - juce::jmin (valueOf (moved), valueOf (other)),
                                          0.0f - juce::jmax (valueOf (moved), valueOf (other)), db - valueOf (moved));
        other->setValueNotifyingHost (other->convertTo0to1 (valueOf (other) + delta));
        moved->setValueNotifyingHost (moved->convertTo0to1 (valueOf (moved) + delta));
    }
    else
        moved->setValueNotifyingHost (moved->convertTo0to1 (db));
}

void BandLimiterEditor::timerCallback()
{
    const auto m = proc.takeMeters();
    inputMeter->setLevel (m.input);
    outputMeter->setLevel (m.output);
    attenuation.setGain (m.minGain);
    if (display != nullptr)
    {
        display->pushGains (m.bandGain);
        for (int b = 0; b < dsp::numBands; ++b)
        {
            auto& s = strips[size_t (b)];
            s.peak = std::max (m.bandPeak[size_t (b)], s.peak * 0.85f);
            s.gainValue = m.bandGain[size_t (b)];
            repaint (s.meterArea);
        }
    }
    activeLed = std::max (m.output > 0.0f ? 1.0f : 0.0f, activeLed * 0.8f);
    repaint (ledArea);
    for (auto* b : { thresholdBox.get(), ceilingBox.get(), separationBox.get(), multiplierBox.get() })
        if (b != nullptr)
            b->refresh();
}

void BandLimiterEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    auto header = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText (proc.getName(), header, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.5f)));
    g.drawText (juce::String (proc.getLatencySamples()) + " samples latency", header, juce::Justification::centredRight);

    for (auto panel : { levelPanel, xoPanel, outPanel })
        if (! panel.isEmpty())
            drawPanel (g, panel.toFloat());

    auto captionAbove = [&g] (juce::Rectangle<int> r, const juce::String& text) {
        drawSectionTitle (g, r.withHeight (14).translated (0, -15), text);
    };

    if (! levelPanel.isEmpty())
    {
        drawSectionTitle (g, inputMeter->getBounds().withHeight (14).translated (0, -15), "In", juce::Justification::centred);
        drawSectionTitle (g, attenuation.getBounds().withHeight (14).translated (0, -15), "Att", juce::Justification::centred);
        drawSectionTitle (g, outputMeter->getBounds().withHeight (14).translated (0, -15), "Out", juce::Justification::centred);
    }
    captionAbove (thresholdBox->getBounds(), "Threshold");
    captionAbove (ceilingBox->getBounds(), "Ceiling");

    if (display != nullptr)
    {
        for (int b = 0; b < dsp::numBands; ++b)
        {
            const auto& s = strips[size_t (b)];
            drawPanel (g, s.area.toFloat());
            g.setColour (bandColours[b]);
            g.setFont (juce::Font (uiFont (12.5f, true)));
            g.drawText ("Band " + juce::String (b + 1), s.area.withHeight (20).reduced (8, 0), juce::Justification::centredLeft);

            captionAbove (s.gain->getBounds(), "Gain");
            captionAbove (s.priority->getBounds(), "Priority");
            captionAbove (s.release->getBounds(), "Release");

            // Band meter: level before limiting, marker at the level the limiter holds it down by
            const auto bar = s.meterArea.toFloat();
            g.setColour (colours::well);
            g.fillRoundedRectangle (bar, 2.0f);
            const float top = dbToY (linToDb (s.peak), bar);
            g.setColour (bandColours[b].withAlpha (0.8f));
            if (s.peak > 1.0e-4f)
                g.fillRoundedRectangle (bar.withTop (top), 2.0f);
            const float gr = dbToY (linToDb (s.gainValue), bar);
            g.setColour (colours::warn);
            g.fillRect (bar.getX() - 1.0f, gr - 1.0f, bar.getWidth() + 2.0f, 2.0f);
        }
        for (int k = 0; k < dsp::numCrossovers; ++k)
            captionAbove (crossoverBoxes[size_t (k)]->getBounds(), crossoverNames[k]);
        captionAbove (separationBox->getBounds(), "Separation");
    }
    if (behaviour != nullptr)
        captionAbove (behaviour->getBounds(), "Release behaviour");
    if (profile != nullptr)
    {
        captionAbove (profile->getBounds(), "Profile");
        captionAbove (multiplierBox->getBounds(), "Release x");
    }
    captionAbove (quantize->getBounds(), "Quantize");
    captionAbove (dither->getBounds(), "Dither");
    captionAbove (shaping->getBounds(), "Shaping");

    if (! ledArea.isEmpty())
    {
        g.setColour (colours::accent2.withAlpha (0.2f + 0.8f * activeLed));
        g.fillEllipse (ledArea.toFloat());
    }
}

void BandLimiterEditor::layoutSelectors (juce::Rectangle<int> area)
{
    const int w = area.getWidth() / 3;
    auto place = [&area, w] (juce::Component& c) {
        auto cell = area.removeFromLeft (w).reduced (8, 0);
        c.setBounds (cell.withTrimmedTop (22).withHeight (24));
    };
    place (*quantize);
    place (*dither);
    place (*shaping);
    ledArea = shaping->getBounds().withSize (8, 8).translated (shaping->getWidth() - 8, -13);
}

void BandLimiterEditor::resized()
{
    auto area = getLocalBounds().reduced (margin);
    area.removeFromTop (headerH - margin);

    // Level column
    levelPanel = area.removeFromLeft (156);
    {
        auto inner = levelPanel.reduced (8);
        auto boxes = inner.removeFromBottom (74);
        auto row = boxes.removeFromTop (46).withTrimmedTop (16);
        thresholdBox->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
        ceilingBox->setBounds (row.reduced (2, 0));
        levelLink.setBounds (boxes.reduced (30, 3));
        inner.removeFromTop (16);
        const int w = inner.getWidth() / 3;
        inputMeter->setBounds (inner.removeFromLeft (w));
        attenuation.setBounds (inner.removeFromLeft (w));
        outputMeter->setBounds (inner);
    }
    area.removeFromLeft (margin);

    outPanel = area.removeFromBottom (58);
    area.removeFromBottom (margin);
    layoutSelectors (outPanel.reduced (4, 2));

    if (profileLayout)
    {
        xoPanel = area;
        auto inner = xoPanel.reduced (12);
        inner.removeFromTop (22);
        profile->setBounds (inner.removeFromTop (34));
        inner.removeFromTop (34);
        multiplierBox->setBounds (inner.removeFromTop (28).withWidth (120));
        return;
    }

    display->setBounds (area.removeFromTop (220));
    area.removeFromTop (margin);

    auto xoRow = area.removeFromBottom (60);
    area.removeFromBottom (margin);
    xoPanel = xoRow;
    {
        auto inner = xoRow.reduced (8, 6).withTrimmedTop (16);
        const int cells = behaviour != nullptr ? 7 : 5;
        const int w = inner.getWidth() / cells;
        for (auto& box : crossoverBoxes)
            box->setBounds (inner.removeFromLeft (w).reduced (4, 2));
        separationBox->setBounds (inner.removeFromLeft (w).reduced (4, 2));
        if (behaviour != nullptr)
            behaviour->setBounds (inner.reduced (4, 2));
    }

    const int stripW = (area.getWidth() - 4 * 6) / dsp::numBands;
    for (int b = 0; b < dsp::numBands; ++b)
    {
        auto& s = strips[size_t (b)];
        s.area = area.removeFromLeft (stripW);
        if (b < dsp::numBands - 1)
            area.removeFromLeft (6);
        auto inner = s.area.reduced (6);
        inner.removeFromTop (16);
        s.meterArea = inner.removeFromRight (12);
        inner.removeFromRight (4);
        auto buttons = inner.removeFromTop (22);
        s.solo->setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
        s.link->setBounds (buttons.reduced (1, 0));
        const int h = inner.getHeight() / 3;
        auto cell = [h, &inner] { return inner.removeFromTop (h).withTrimmedTop (15).withTrimmedBottom (1); };
        s.gain->setBounds (cell());
        s.priority->setBounds (cell());
        s.release->setBounds (cell());
    }
}

} // namespace craft::band_limiter
