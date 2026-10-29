// Renders the editor to PNG with some programme running through it (development aid, not shipped).
// Usage: console_g_snapshot <output directory>
#include <iostream>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    namespace ids = craft::consoleg::ids;
    craft::consoleg::ConsoleGProcessor proc;
    auto set = [&proc] (const char* id, float r) {
        if (auto* p = proc.getState().getParameter (id))
            p->setValueNotifyingHost (r);
    };
    set (ids::compRatio, 0.45f);
    set (ids::compThreshold, 0.7f);
    set (ids::expRange, 0.4f);
    set (ids::hpFreq, 0.3f);
    set (ids::hmfGain, 0.7f);
    set (ids::lfGain, 0.65f);

    proc.setRateAndBufferSizeDetails (48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);

    juce::Random rng (7);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 60; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * (ch == 0 ? 0.5f : 0.3f));
        proc.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (12);
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
    auto f = outDir.getChildFile ("console-g.png");
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    return 0;
}
