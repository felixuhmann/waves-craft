#include "PluginEditor.h"

namespace craft::limiter
{

using namespace ui;

namespace
{
constexpr int headerH = 40, margin = 12, gap = 10, bodyH = 372;
constexpr int faderW = 136, releaseW = 170, grW = 104;
} // namespace

PeakLimiterEditor::PeakLimiterEditor (PeakLimiterProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      threshold (p.getState(), ids::threshold, "Threshold", "IN", colours::accent),
      ceiling (p.getState(), ids::ceiling, "Out Ceiling", "OUT", colours::accent2),
      release (p.getState(), ids::release),
      autoAttachment (p.getState(), ids::autoRelease, autoRelease)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (threshold);
    addAndMakeVisible (ceiling);
    addAndMakeVisible (release);
    autoRelease.setTooltip ("Auto Release: program-dependent recovery, the Release time is its upper bound");
    addAndMakeVisible (autoRelease);
    addAndMakeVisible (grMeter);

    const int channels = proc.getMeterChannels();
    threshold.setChannels (channels);
    ceiling.setChannels (channels);

    setSize (margin + faderW + gap + faderW + gap + releaseW + gap + grW + margin, headerH + margin + bodyH + margin);
    startTimerHz (30);
}

PeakLimiterEditor::~PeakLimiterEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PeakLimiterEditor::timerCallback()
{
    threshold.refresh();
    ceiling.refresh();
    release.refresh();

    const auto m = proc.takeMeters();
    threshold.setChannels (m.channels);
    ceiling.setChannels (m.channels);
    threshold.pushLevels (m.input[0], m.channels == 2 ? m.input[1] : m.input[0]);
    ceiling.pushLevels (m.output[0], m.channels == 2 ? m.output[1] : m.output[0]);
    grMeter.pushGain (m.minGain);
}

void PeakLimiterEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);

    // logo mark: a waveform hitting a flat ceiling
    auto mark = h.removeFromLeft (28).toFloat().withSizeKeepingCentre (24, 18);
    juce::Path wave;
    wave.startNewSubPath (mark.getX(), mark.getCentreY());
    wave.lineTo (mark.getX() + 5.0f, mark.getY() + 3.0f);
    wave.lineTo (mark.getX() + 15.0f, mark.getY() + 3.0f);
    wave.lineTo (mark.getX() + 19.0f, mark.getBottom() - 2.0f);
    wave.lineTo (mark.getRight(), mark.getCentreY());
    g.setColour (colours::accent2);
    g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::accent);
    g.fillRect (juce::Rectangle<float> (mark.getX() - 1.0f, mark.getY(), mark.getWidth() + 2.0f, 2.0f));

    h.removeFromLeft (6);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText ("CRAFT PEAK LIMITER", h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" CRAFT_VERSION_STRING, h, juce::Justification::centredRight);

    drawPanel (g, thresholdBlock.toFloat());
    drawPanel (g, ceilingBlock.toFloat());
    drawPanel (g, releaseBlock.toFloat());
    drawPanel (g, grBlock.toFloat());

    // Auto Release caption and look-ahead note in the release block
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (12.0f, true)));
    g.drawText ("AUTO RELEASE", autoRelease.getBounds().translated (0, -22).withHeight (18).expanded (30, 0),
                juce::Justification::centred);
    const double fs = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    g.setFont (juce::Font (uiFont (10.5f)));
    g.drawText ("LOOK-AHEAD " + juce::String (proc.getLatencySamples()) + " SMP  @ " + juce::String (fs / 1000.0, 1)
                    + " kHz",
                releaseBlock.reduced (8).removeFromBottom (16), juce::Justification::centred);
}

void PeakLimiterEditor::resized()
{
    auto r = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    thresholdBlock = r.removeFromLeft (faderW);
    r.removeFromLeft (gap);
    ceilingBlock = r.removeFromLeft (faderW);
    r.removeFromLeft (gap);
    releaseBlock = r.removeFromLeft (releaseW);
    r.removeFromLeft (gap);
    grBlock = r;

    threshold.setBounds (thresholdBlock.reduced (8, 10));
    ceiling.setBounds (ceilingBlock.reduced (8, 10));

    auto rb = releaseBlock.reduced (8, 10);
    release.setBounds (rb.removeFromTop (rb.getWidth() + 12));
    rb.removeFromTop (34);
    autoRelease.setBounds (rb.removeFromTop (28).withSizeKeepingCentre (84, 28));

    grMeter.setBounds (grBlock.reduced (8, 10));
}

} // namespace craft::limiter
