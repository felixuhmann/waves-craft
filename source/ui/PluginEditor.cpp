#include "PluginEditor.h"

namespace craft
{

using namespace ui;

namespace
{
constexpr int headerH = 40, margin = 12, faderW = 74;
}

CraftEditor::CraftEditor (CraftProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.getState();

    if (msOnly())
    {
        setSize (420, 220);
        return;
    }

    faders.push_back (std::make_unique<Fader> (st, ids::gain, "Gain",
                                               std::vector<ScaleMark> { { 0.0, "0" }, { -12.0, "-12" }, { -24.0, "-24" } }));
    faders.push_back (std::make_unique<Fader> (st, ids::width, "Width",
                                               std::vector<ScaleMark> { { 3.0, "3.0" }, { 2.0, "2.0" }, { 1.0, "1.0" }, { 0.0, "0.0" } }));
    if (full())
    {
        faders.push_back (std::make_unique<Fader> (st, ids::shuffle, "Shuffle",
                                                   std::vector<ScaleMark> { { 3.0, "3.0" }, { 2.0, "2.0" }, { 1.0, "1.0" } }));
        faders.push_back (std::make_unique<Fader> (st, ids::frequency, "Freq",
                                                   std::vector<ScaleMark> { { 1400.0, "1.4k" }, { 650.0, "650" }, { 350.0, "350" } }));
        bassTrim = std::make_unique<ValueBox> (*st.getParameter (ids::bassTrim), ids::bassTrim);
        addAndMakeVisible (*bassTrim);
    }
    for (auto& f : faders)
        addAndMakeVisible (*f);

    display = std::make_unique<VectorDisplay> (st, proc.getVariant());
    addAndMakeVisible (*display);
    asymmetry = std::make_unique<HSlider> (st, ids::asymmetry, "Asymmetry",
                                           std::vector<ScaleMark> { { -90.0, "-90" }, { 0.0, "0" }, { 90.0, "90" } });
    rotation = std::make_unique<HSlider> (st, ids::rotation, "Rotation",
                                          std::vector<ScaleMark> { { -45.0, "-45" }, { 0.0, "0" }, { 45.0, "45" } });
    addAndMakeVisible (*asymmetry);
    addAndMakeVisible (*rotation);

    auto attach = [&] (const char* id, juce::Button& b, const juce::String& tip) {
        b.setTooltip (tip);
        addAndMakeVisible (b);
        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, id, b));
    };
    attach (ids::inputMode, inputMode, "Input mode: L-R (stereo) or M-S (mid/side encoded)");
    attach (ids::polarity1, polarity1, "Polarity of input channel 1");
    attach (ids::swap, swap, "Swap input channels");
    attach (ids::polarity2, polarity2, "Polarity of input channel 2");

    resetButton.setTooltip ("Reset Width, Asymmetry, Rotation and Shuffle");
    resetButton.onClick = [this] { resetImage(); };
    addAndMakeVisible (resetButton);

    meters = std::make_unique<MeterPanel>();
    meters->setMode (static_cast<int> (st.state.getProperty (ids::meterMode, 0)) != 0);
    meters->onModeChange = [this] (bool ms) { proc.getState().state.setProperty (ids::meterMode, ms ? 1 : 0, nullptr); };
    addAndMakeVisible (*meters);

    const int leftW = int (faders.size()) * faderW + 16;
    setSize (margin + leftW + margin + 330 + margin + 150 + margin, headerH + 380);
    startTimerHz (30);
}

CraftEditor::~CraftEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CraftEditor::resetImage()
{
    for (const char* id : { ids::width, ids::asymmetry, ids::rotation, ids::shuffle })
        if (auto* p = proc.getState().getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
}

void CraftEditor::timerCallback()
{
    for (auto& f : faders)
        f->refresh();
    if (asymmetry) asymmetry->refresh();
    if (rotation) rotation->refresh();
    if (bassTrim) bassTrim->refresh();
    if (display) display->setSettings (proc.readSettings());
    if (meters)
    {
        const bool ms = static_cast<int> (proc.getState().state.getProperty (ids::meterMode, 0)) != 0;
        if (ms != meters->getMode())
            meters->setMode (ms);
        const auto pk = proc.takePeaks (ms ? 1 : 0);
        meters->pushPeaks (pk[0], pk[1]);
    }
}

void CraftEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);
    // logo mark: two overlapping rings
    auto mark = h.removeFromLeft (28).toFloat().withSizeKeepingCentre (24, 16);
    g.setColour (colours::accent);
    g.drawEllipse (mark.withWidth (16), 2.0f);
    g.setColour (colours::accent2);
    g.drawEllipse (mark.withTrimmedLeft (8), 2.0f);
    h.removeFromLeft (6);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText (proc.getName().toUpperCase(), h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" CRAFT_VERSION_STRING, h.withTrimmedRight (msOnly() ? 0 : 90),
                juce::Justification::centredRight);

    if (msOnly())
    {
        auto body = getLocalBounds().withTrimmedTop (headerH).reduced (margin).toFloat();
        drawPanel (g, body);
        auto r = body.reduced (16.0f);
        g.setColour (colours::text);
        g.setFont (juce::Font (uiFont (14.0f, true)));
        g.drawText ("L-R  to  M-S matrix", r.removeFromTop (22), juce::Justification::centredLeft);
        g.setFont (juce::Font (uiFont (13.0f)));
        g.setColour (colours::textDim);
        g.drawText ("out 1  =  (in 1 + in 2) / 2", r.removeFromTop (22), juce::Justification::centredLeft);
        g.drawText ("out 2  =  (in 1 - in 2) / 2", r.removeFromTop (22), juce::Justification::centredLeft);
        r.removeFromTop (10);
        g.drawFittedText ("Fixed sum/difference encoder. No controls, zero latency. Feeding the result into the "
                          "same matrix again restores L-R at half level.",
                          r.toNearestInt(), juce::Justification::topLeft, 3);
        return;
    }

    drawPanel (g, leftBlock.toFloat());
    drawPanel (g, centreBlock.toFloat());
    drawPanel (g, rightBlock.toFloat());

    // under-fader labels
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.drawText ("INPUT MODE", ioRow.withHeight (14).withWidth (inputMode.getRight() - ioRow.getX()), juce::Justification::centred);
    if (bassTrim)
        g.drawText ("BASS TRIM", bassTrim->getBounds().withY (ioRow.getY()).withHeight (14).expanded (10, 0),
                    juce::Justification::centred);
}

void CraftEditor::resized()
{
    if (msOnly())
        return;

    auto r = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    resetButton.setBounds (getLocalBounds().removeFromTop (headerH).removeFromRight (margin + 70).withTrimmedRight (margin)
                               .withSizeKeepingCentre (70, 24));

    leftBlock = r.removeFromLeft (int (faders.size()) * faderW + 16);
    r.removeFromLeft (margin);
    rightBlock = r.removeFromRight (150);
    r.removeFromRight (margin);
    centreBlock = r;

    // left block: faders + I/O row
    auto lb = leftBlock.reduced (8);
    ioRow = lb.removeFromBottom (64);
    for (auto& f : faders)
        f->setBounds (lb.removeFromLeft (faderW));

    auto io = ioRow;
    auto modeArea = io.removeFromLeft (std::min (io.getWidth(), 140));
    inputMode.setBounds (modeArea.withTrimmedTop (16).removeFromTop (22).reduced (6, 0));
    auto buttons = modeArea.withTrimmedTop (42).withHeight (22);
    const int bw = 30;
    const int bx = buttons.getCentreX() - (3 * bw + 2 * 6) / 2;
    polarity1.setBounds (bx, buttons.getY(), bw, 22);
    swap.setBounds (bx + bw + 6, buttons.getY(), bw, 22);
    polarity2.setBounds (bx + 2 * (bw + 6), buttons.getY(), bw, 22);
    if (bassTrim)
        bassTrim->setBounds (io.withTrimmedTop (16).removeFromTop (22).withSizeKeepingCentre (56, 22));

    // centre: display + two sliders
    auto cb = centreBlock.reduced (10);
    rotation->setBounds (cb.removeFromBottom (36));
    asymmetry->setBounds (cb.removeFromBottom (36));
    cb.removeFromBottom (6);
    display->setBounds (cb);

    meters->setBounds (rightBlock.reduced (8));
}

} // namespace craft
