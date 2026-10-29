#include "PluginEditor.h"

namespace craft::vocal
{

using namespace ui;

namespace
{
constexpr int headerH = 40, footerH = 22, margin = 12, gap = 10, bodyH = 400;
constexpr int sideW = 128, centreW = 168;
} // namespace

VocalCompressorEditor::VocalCompressorEditor (VocalCompressorProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      gate (p.getState(), ids::gate, "Gate", FaderStrip::Meter::inputEnergy, colours::gate),
      compression (p.getState(), ids::compression, "Compression", FaderStrip::Meter::gainReduction, colours::comp),
      gain (p.getState(), ids::gain, "Gain", FaderStrip::Meter::outputPeaks, colours::signal)
{
    setLookAndFeel (&lnf);
    compression.setLinkPartner (p.getState().getParameter (ids::gain));
    gain.setLinkPartner (p.getState().getParameter (ids::compression));
    addAndMakeVisible (gate);
    addAndMakeVisible (compression);
    addAndMakeVisible (gain);
    gain.setChannels (proc.getMeterChannels());

    setSize (margin + sideW + gap + centreW + gap + sideW + margin, headerH + margin + bodyH + footerH + margin);
    startTimerHz (30);
}

VocalCompressorEditor::~VocalCompressorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VocalCompressorEditor::timerCallback()
{
    gate.refresh();
    compression.refresh();
    gain.refresh();

    const auto m = proc.takeMeters();
    gain.setChannels (m.channels);
    gate.pushInputEnergy (m.inputEnergy);
    compression.pushGainReduction (m.minGain);
    gain.pushOutputPeaks (m.output[0], m.channels == 2 ? m.output[1] : m.output[0]);
}

void VocalCompressorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::comp);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);

    // logo mark: a voice waveform whose peaks are pressed flat by a bar
    auto mark = h.removeFromLeft (28).toFloat().withSizeKeepingCentre (24, 20);
    juce::Path wave;
    const float cy = mark.getCentreY();
    wave.startNewSubPath (mark.getX(), cy);
    const float amps[] = { 3.0f, 7.0f, 8.0f, 8.0f, 6.0f, 3.0f };
    for (int i = 0; i < 6; ++i)
    {
        const float x0 = mark.getX() + 4.0f * float (i);
        wave.quadraticTo (x0 + 1.0f, cy - amps[i], x0 + 2.0f, cy);
        wave.quadraticTo (x0 + 3.0f, cy + amps[i], x0 + 4.0f, cy);
    }
    g.setColour (colours::signal);
    g.strokePath (wave, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::comp);
    g.fillRect (juce::Rectangle<float> (mark.getX() + 4.0f, mark.getY() + 1.0f, 16.0f, 2.0f));
    g.fillRect (juce::Rectangle<float> (mark.getX() + 4.0f, mark.getBottom() - 3.0f, 16.0f, 2.0f));

    h.removeFromLeft (6);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText ("CRAFT VOCAL COMPRESSOR", h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" CRAFT_VERSION_STRING, h, juce::Justification::centredRight);

    drawPanel (g, gateBlock.toFloat());
    drawPanel (g, compBlock.toFloat());
    drawPanel (g, gainBlock.toFloat());

    // footer: hint + look-ahead
    auto footer = getLocalBounds().removeFromBottom (footerH + margin).withTrimmedBottom (margin / 2).reduced (margin, 0);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f)));
    g.drawText ("Ctrl/Cmd-drag: link Compression + Gain",
                footer, juce::Justification::centredLeft);
    const double fs = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    g.drawText ("LOOK-AHEAD " + juce::String (proc.getLatencySamples()) + " SMP @ " + juce::String (fs / 1000.0, 1) + " kHz",
                footer, juce::Justification::centredRight);
}

void VocalCompressorEditor::resized()
{
    auto r = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    r.removeFromBottom (footerH);
    gateBlock = r.removeFromLeft (sideW);
    r.removeFromLeft (gap);
    compBlock = r.removeFromLeft (centreW);
    r.removeFromLeft (gap);
    gainBlock = r;

    gate.setBounds (gateBlock.reduced (8, 10));
    compression.setBounds (compBlock.reduced (10, 10));
    gain.setBounds (gainBlock.reduced (8, 10));
}

} // namespace craft::vocal
