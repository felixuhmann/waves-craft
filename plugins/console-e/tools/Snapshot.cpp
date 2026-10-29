// Renders the editor to PNG (development aid, not shipped).
// Usage: console_e_snapshot <output directory>
#include <cmath>
#include <iostream>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    auto set = [] (juce::AudioProcessorValueTreeState& s, const char* id, float norm) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (norm);
    };

    for (const bool active : { false, true })
    {
        craft::consolee::ConsoleProcessor proc;
        auto& st = proc.getState();
        if (active)
        {
            namespace ids = craft::consolee::ids;
            set (st, ids::compRatio, 0.45f);
            set (st, ids::compThreshold, 0.7f);
            set (st, ids::expRange, 0.6f);
            set (st, ids::expThreshold, 0.3f);
            set (st, ids::hp, 0.4f);
            set (st, ids::lmfGain, 0.8f);
            set (st, ids::hfGain, 0.3f);
            set (st, ids::hfBell, 1.0f);
            set (st, ids::analogue, 1.0f);
            set (st, ids::meterOutput, 1.0f);
        }
        proc.prepareToPlay (48000.0, 4800);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        juce::MidiBuffer midi;
        for (int round = 0; round < 8; ++round)
        {
            juce::AudioBuffer<float> buf (2, 4800);
            for (int i = 0; i < 4800; ++i)
            {
                const float v = active ? 0.4f * std::sin (2.0f * 3.14159265f * 220.0f * float (i) / 48000.0f) : 0.0f;
                buf.setSample (0, i, v);
                buf.setSample (1, i, 0.7f * v);
            }
            proc.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
        auto f = outDir.getChildFile (active ? "console-e-active.png" : "console-e-default.png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
