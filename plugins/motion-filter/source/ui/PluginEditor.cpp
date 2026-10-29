#include "PluginEditor.h"

#include "dsp/Parameters.h"

namespace craft::mfp
{

using namespace ui;

namespace
{
constexpr int editorW = 1000, editorH = 640, headerH = 44, margin = 12;
const char* const sourceLabels[] = { "LFO", "SEQ", "FOLLOW" };

// Icons of the six filter types: a miniature magnitude sketch.
void paintTypeIcon (juce::Graphics& g, juce::Rectangle<float> r, int index, juce::Colour ink)
{
    static const char* names[] = { "LP", "HP", "BP", "BR", "COMB", "AMP" };
    auto label = r.removeFromBottom (11.0f);
    g.setColour (ink);
    g.setFont (juce::Font (uiFont (9.5f, true)));
    g.drawText (names[index], label, juce::Justification::centred);
    auto a = r.reduced (4.0f, 1.0f);
    juce::Path p;
    const float x0 = a.getX(), x1 = a.getRight(), yT = a.getY() + 1.0f, yB = a.getBottom(), xm = a.getCentreX();
    switch (index)
    {
        case 0: p.startNewSubPath (x0, yT + 3); p.lineTo (xm - 2, yT + 3); p.quadraticTo (xm + 2, yT - 1, xm + 4, yT + 4); p.lineTo (x1, yB); break;
        case 1: p.startNewSubPath (x0, yB); p.lineTo (xm - 4, yT + 4); p.quadraticTo (xm - 2, yT - 1, xm + 2, yT + 3); p.lineTo (x1, yT + 3); break;
        case 2: p.startNewSubPath (x0, yB); p.quadraticTo (xm - 3, yB - 1, xm, yT); p.quadraticTo (xm + 3, yB - 1, x1, yB); break;
        case 3: p.startNewSubPath (x0, yT + 2); p.lineTo (xm - 4, yT + 2); p.quadraticTo (xm, yT + 2, xm, yB); p.quadraticTo (xm, yT + 2, xm + 4, yT + 2); p.lineTo (x1, yT + 2); break;
        case 4:
        {
            p.startNewSubPath (x0, yB - 2);
            const int teeth = 5;
            for (int k = 0; k < teeth; ++k)
            {
                const float s = x0 + (x1 - x0) * float (k) / teeth, e = x0 + (x1 - x0) * float (k + 1) / teeth;
                p.quadraticTo ((s + e) / 2, yT - 2, e, yB - 2);
            }
            break;
        }
        default: p.startNewSubPath (x0, (yT + yB) / 2); p.lineTo (x1, (yT + yB) / 2);
                 p.startNewSubPath (xm - 3, yT + 1); p.lineTo (xm + 3, yT + 1); p.startNewSubPath (xm, yT - 2); p.lineTo (xm, yT + 4); break;
    }
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// Icons of the five LFO shapes.
void paintShapeIcon (juce::Graphics& g, juce::Rectangle<float> r, int index, juce::Colour ink)
{
    auto a = r.reduced (2.0f, 4.0f);
    const float x0 = a.getX(), w = a.getWidth(), yc = a.getCentreY(), h = a.getHeight() / 2;
    juce::Path p;
    auto at = [&] (float t, float v) { return juce::Point<float> (x0 + w * t, yc - h * v); };
    switch (index)
    {
        case 0:
            for (int k = 0; k <= 32; ++k)
            {
                const float t = float (k) / 32.0f;
                const auto pt = at (t, std::sin (6.2831853f * t));
                if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            break;
        case 1: p.startNewSubPath (at (0, 0)); p.lineTo (at (0.25f, 1)); p.lineTo (at (0.75f, -1)); p.lineTo (at (1, 0)); break;
        case 2: p.startNewSubPath (at (0, 1)); p.lineTo (at (1, -1)); p.startNewSubPath (at (1, -1)); p.lineTo (at (1, 1)); break;
        case 3: p.startNewSubPath (at (0, 1)); p.lineTo (at (0.5f, 1)); p.lineTo (at (0.5f, -1)); p.lineTo (at (1, -1)); break;
        default:
        {
            const float v[] = { 0.4f, -0.7f, 0.9f, -0.2f, 0.6f };
            p.startNewSubPath (at (0, v[0]));
            for (int k = 0; k < 5; ++k)
            {
                p.lineTo (at (float (k + 1) / 5.0f, v[k]));
                if (k < 4) p.lineTo (at (float (k + 1) / 5.0f, v[k + 1]));
            }
            break;
        }
    }
    g.setColour (ink);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
}
} // namespace

MotionFilterEditor::MotionFilterEditor (MotionFilterProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.getState();

    auto knob = [&] (std::unique_ptr<Knob>& k, const char* id, const juce::String& title, juce::Colour c) {
        k = std::make_unique<Knob> (st, id, title, c);
        addAndMakeVisible (*k);
        knobs.push_back (k.get());
    };
    auto toggle = [&] (std::unique_ptr<SegmentToggle>& t, const char* id, const juce::String& off, const juce::String& on, juce::Colour c) {
        t = std::make_unique<SegmentToggle> (st, id, off, on, c);
        addAndMakeVisible (*t);
    };
    auto mods = [&] (std::array<std::unique_ptr<ModSlider>, 3>& arr, int dest) {
        for (int s = 0; s < 3; ++s)
        {
            arr[size_t (s)] = std::make_unique<ModSlider> (st, ids::mod (s, dest), sourceLabels[s], sourceColour (s));
            addAndMakeVisible (*arr[size_t (s)]);
            modSliders.push_back (arr[size_t (s)].get());
        }
    };

    // filter
    typeStrip = std::make_unique<ChoiceStrip> (st, ids::type, juce::StringArray { "LP", "HP", "BP", "BR", "Comb", "Amp" },
                                               colours::accent);
    typeStrip->setIconPainter (paintTypeIcon);
    addAndMakeVisible (*typeStrip);
    knob (freqKnob, ids::freq, "Freq", colours::accent);
    knob (resKnob, ids::res, "Reso", colours::accent);
    mods (freqMods, mf::destFreq);
    mods (resMods, mf::destRes);
    toggle (slope, ids::slope, "12", "24", colours::accent);
    toggle (kbd, ids::kbd, "OFF", "KBD", colours::accent);

    // global, mixer, follower
    knob (spread, ids::spread, "Spread", colours::accent);
    knob (smooth, ids::smooth, "Smooth", colours::accent);
    knob (drive, ids::drive, "Drive", colours::delay);
    knob (crush, ids::crush, "Crush", colours::delay);
    knob (mix, ids::mix, "Mix", colours::text);
    knob (input, ids::input, "Input", colours::text);
    knob (output, ids::output, "Output", colours::text);
    toggle (folSource, ids::folSource, "INPUT", "SC", colours::follower);

    display = std::make_unique<ResponseDisplay> (proc);
    addAndMakeVisible (*display);

    // LFO
    knob (lfoRate, ids::lfoRate, "Rate", colours::lfo);
    lfoRate->setAlternate (st, ids::lfoSyncRate, ids::lfoSync);
    toggle (lfoSync, ids::lfoSync, "FREE", "SYNC", colours::lfo);
    lfoShape = std::make_unique<ChoiceStrip> (st, ids::lfoShape, juce::StringArray { "Sine", "Tri", "Saw", "Pulse", "S&H" }, colours::lfo);
    lfoShape->setIconPainter (paintShapeIcon);
    addAndMakeVisible (*lfoShape);

    // sequencer
    steps = std::make_unique<StepEditor> (st);
    addAndMakeVisible (*steps);
    seqLast = std::make_unique<DragValue> (st, ids::seqLast, "Last", colours::seq);
    addAndMakeVisible (*seqLast);
    knob (swing, ids::swing, "Swing", colours::seq);
    knob (seqRate, ids::seqRate, "Rate", colours::seq);
    seqRate->setAlternate (st, ids::seqSyncRate, ids::seqSync);
    toggle (seqSync, ids::seqSync, "FREE", "SYNC", colours::seq);
    drawMode = std::make_unique<SegmentToggle> (st, juce::String(), "DRAG", "DRAW", colours::seq);
    drawMode->setToggleState (static_cast<int> (st.state.getProperty (ids::drawMode, 0)) != 0, juce::dontSendNotification);
    drawMode->setTooltip ("Step edit mode: Drag moves one step, Draw paints across steps");
    drawMode->onClick = [this] {
        proc.getState().state.setProperty (ids::drawMode, drawMode->getToggleState() ? 1 : 0, nullptr);
        steps->setDrawMode (drawMode->getToggleState());
    };
    steps->setDrawMode (drawMode->getToggleState());
    addAndMakeVisible (*drawMode);

    // delay
    knob (dlyTime, ids::dlyTime, "Time", colours::delay);
    dlyTime->setAlternate (st, ids::dlySyncRate, ids::dlySync);
    mods (timeMods, mf::destTime);
    knob (dlyFeedback, ids::dlyFeedback, "Feedback", colours::delay);
    knob (dlyMix, ids::dlyMix, "Dry/Wet", colours::delay);
    dlyRatio = std::make_unique<ChoiceStrip> (st, ids::dlyRatio, juce::StringArray { "x1", "x1.5", "x2" }, colours::delay);
    addAndMakeVisible (*dlyRatio);
    toggle (dlySync, ids::dlySync, "FREE", "SYNC", colours::delay);
    toggle (dlyMode, ids::dlyMode, "DIGITAL", "ANALOG", colours::delay);
    toggle (dlyRoute, ids::dlyRoute, "PRE", "FBK", colours::delay);

    toggle (bypass, ids::bypass, "ON", "BYPASS", colours::textDim);

    setSize (editorW, editorH);
    timerCallback();
    startTimerHz (30);
}

MotionFilterEditor::~MotionFilterEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MotionFilterEditor::timerCallback()
{
    for (auto* k : knobs)
        k->refresh();
    for (auto* m : modSliders)
        m->refresh();

    const auto s = proc.readSettings();
    switch (s.type)
    {
        case mf::FilterType::Comb: freqKnob->setTitle ("Delay"); resKnob->setTitle ("Feedback"); break;
        case mf::FilterType::Amp:  freqKnob->setTitle ("Gain");  resKnob->setTitle ("Reso"); break;
        default:                   freqKnob->setTitle ("Freq");  resKnob->setTitle ("Reso"); break;
    }
    resKnob->setEnabled (s.type != mf::FilterType::Amp);
    display->update();
    steps->setLastStep (s.lastStep);
    steps->setPlayingStep (proc.live.step.load());
}

void MotionFilterEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH - 2).reduced (margin, 0);
    // logo mark: a resonant curve sweeping through three source dots
    auto mark = h.removeFromLeft (34).toFloat().withSizeKeepingCentre (30, 20);
    juce::Path wave;
    wave.startNewSubPath (mark.getX(), mark.getCentreY() + 4);
    wave.cubicTo (mark.getX() + 10, mark.getCentreY() + 4, mark.getCentreX() - 2, mark.getY() - 2, mark.getCentreX() + 2,
                  mark.getCentreY() + 2);
    wave.lineTo (mark.getRight(), mark.getBottom());
    g.setColour (colours::accent);
    g.strokePath (wave, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    for (int s = 0; s < 3; ++s)
    {
        g.setColour (sourceColour (s));
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre ({ mark.getX() + 6.0f + 9.0f * float (s), mark.getBottom() + 1.0f }));
    }
    h.removeFromLeft (8);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (18.0f, true)).withExtraKerningFactor (0.05f));
    g.drawText ("CRAFT MOTION FILTER", h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" CRAFT_VERSION_STRING, h.withTrimmedRight (130), juce::Justification::centredRight);

    drawSection (g, filterArea, "Filter", colours::accent);
    drawSection (g, displayArea, {}, colours::accent);
    drawSection (g, globalArea, "Character", colours::delay);
    drawSection (g, mixerArea, "Mixer", colours::text);
    drawSection (g, followerArea, "Follower source", colours::follower);
    drawSection (g, lfoArea, "LFO", colours::lfo);
    drawSection (g, seqArea, "Sequencer", colours::seq);
    drawSection (g, delayArea, "Delay", colours::delay);

    g.setFont (juce::Font (uiFont (9.5f, true)).withExtraKerningFactor (0.08f));
    g.setColour (colours::textDim);
    for (const auto& [r, text] : captions)
        g.drawText (text, r, juce::Justification::centredLeft);
}

void MotionFilterEditor::resized()
{
    captions.clear();
    auto caption = [this] (juce::Component& c, const juce::String& text) {
        captions.emplace_back (c.getBounds().withHeight (11).translated (0, -12), text);
    };

    bypass->setBounds (getWidth() - margin - 116, (headerH - 24) / 2, 116, 22);

    // areas
    const int topY = headerH + margin, topH = 300, botY = topY + topH + margin, botH = editorH - margin - botY;
    filterArea = { margin, topY, 318, topH };
    displayArea = { filterArea.getRight() + margin, topY, 410, topH };
    const int rightX = displayArea.getRight() + margin, rightW = editorW - margin - rightX;
    globalArea = { rightX, topY, rightW, 112 };
    mixerArea = { rightX, globalArea.getBottom() + 6, rightW, 112 };
    followerArea = { rightX, mixerArea.getBottom() + 6, rightW, topY + topH - mixerArea.getBottom() - 6 };
    lfoArea = { margin, botY, 212, botH };
    seqArea = { lfoArea.getRight() + margin, botY, 430, botH };
    delayArea = { seqArea.getRight() + margin, botY, editorW - margin - seqArea.getRight() - margin, botH };

    // filter
    {
        auto r = filterArea.reduced (10);
        r.removeFromTop (22);
        typeStrip->setBounds (r.removeFromTop (38));
        r.removeFromTop (8);
        auto bottom = r.removeFromBottom (24);
        r.removeFromBottom (14);
        auto left = r.removeFromLeft (r.getWidth() / 2).reduced (4, 0);
        auto right = r.reduced (4, 0);
        auto place = [] (juce::Rectangle<int> col, Knob& k, std::array<std::unique_ptr<ModSlider>, 3>& arr) {
            auto sliders = col.removeFromBottom (3 * 18);
            k.setBounds (col.withSizeKeepingCentre (std::min (col.getWidth(), 96), col.getHeight()));
            for (auto& m : arr)
                m->setBounds (sliders.removeFromTop (18));
        };
        place (left, *freqKnob, freqMods);
        place (right, *resKnob, resMods);
        slope->setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (4, 0).withSizeKeepingCentre (96, 22));
        kbd->setBounds (bottom.reduced (4, 0).withSizeKeepingCentre (96, 22));
        caption (*slope, "SLOPE dB/oct");
        caption (*kbd, "KEY TRACK");
    }

    display->setBounds (displayArea.reduced (4));

    // character (global) and mixer
    {
        auto r = globalArea.reduced (8);
        r.removeFromTop (18);
        const int w = r.getWidth() / 4;
        for (auto* k : { spread.get(), smooth.get(), drive.get(), crush.get() })
            k->setBounds (r.removeFromLeft (w));
        auto m = mixerArea.reduced (8);
        m.removeFromTop (18);
        const int mw = m.getWidth() / 3;
        for (auto* k : { mix.get(), input.get(), output.get() })
            k->setBounds (m.removeFromLeft (mw).reduced (4, 0));
        auto f = followerArea.reduced (10);
        f.removeFromTop (18);
        folSource->setBounds (f.withSizeKeepingCentre (std::min (f.getWidth(), 150), 22));
    }

    // LFO
    {
        auto r = lfoArea.reduced (10);
        r.removeFromTop (22);
        lfoShape->setBounds (r.removeFromBottom (32));
        r.removeFromBottom (16);
        caption (*lfoShape, "SHAPE");
        auto syncRow = r.removeFromBottom (22);
        r.removeFromBottom (14);
        lfoSync->setBounds (syncRow.withSizeKeepingCentre (120, 22));
        lfoRate->setBounds (r.withSizeKeepingCentre (110, r.getHeight()));
    }

    // sequencer
    {
        auto r = seqArea.reduced (10);
        r.removeFromTop (22);
        steps->setBounds (r.removeFromTop (132));
        r.removeFromTop (6);
        auto row = r;
        seqLast->setBounds (row.removeFromLeft (60).withSizeKeepingCentre (54, 42));
        row.removeFromLeft (6);
        swing->setBounds (row.removeFromLeft (70));
        seqRate->setBounds (row.removeFromLeft (76));
        row.removeFromLeft (8);
        auto toggles = row.withTrimmedTop (14);
        seqSync->setBounds (toggles.removeFromTop (22).withWidth (std::min (toggles.getWidth(), 130)));
        toggles.removeFromTop (16);
        drawMode->setBounds (toggles.removeFromTop (22).withWidth (std::min (toggles.getWidth(), 130)));
        caption (*seqSync, "CLOCK");
        caption (*drawMode, "EDIT");
    }

    // delay
    {
        auto r = delayArea.reduced (10);
        r.removeFromTop (22);
        auto left = r.removeFromLeft (r.getWidth() / 2).reduced (2, 0);
        auto right = r.reduced (2, 0);
        dlyTime->setBounds (left.removeFromTop (88).withSizeKeepingCentre (std::min (left.getWidth(), 92), 88));
        for (auto& m : timeMods)
            m->setBounds (left.removeFromTop (18));
        left.removeFromTop (14);
        dlySync->setBounds (left.removeFromTop (22));
        left.removeFromTop (14);
        dlyRatio->setBounds (left.removeFromTop (22));
        caption (*dlySync, "CLOCK");
        caption (*dlyRatio, "RIGHT x");

        auto knobsRow = right.removeFromTop (88);
        dlyFeedback->setBounds (knobsRow.removeFromLeft (knobsRow.getWidth() / 2));
        dlyMix->setBounds (knobsRow);
        right.removeFromTop (14 + 54 - 40);
        dlyMode->setBounds (right.removeFromTop (22));
        right.removeFromTop (14);
        dlyRoute->setBounds (right.removeFromTop (22));
        caption (*dlyMode, "MODE");
        caption (*dlyRoute, "ROUTE");
    }
}

} // namespace craft::mfp
