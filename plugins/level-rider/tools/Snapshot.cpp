// Renders the editor to PNG (development aid, not shipped).
// Usage: level_rider_snapshot <output directory>
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

    struct Case
    {
        const char* name;
        bool live, active;
    };
    for (const Case c : { Case { "level-rider-default", false, false }, Case { "level-rider-live-active", true, true } })
    {
        craft::levelrider::LevelRiderProcessor proc (c.live);
        if (c.active)
            if (auto* p = proc.getState().getParameter (craft::levelrider::ids::rangeMax))
                p->setValueNotifyingHost (p->convertTo0to1 (9.0f));
        proc.prepareToPlay (48000.0, 4800);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        juce::MidiBuffer midi;
        for (int round = 0; round < 30; ++round)
        {
            juce::AudioBuffer<float> buf (2, 4800);
            for (int i = 0; i < 4800; ++i)
            {
                const float v = c.active ? 0.05f * std::sin (2.0f * 3.14159265f * 440.0f * float (i) / 48000.0f) : 0.0f;
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
            proc.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile (juce::String (c.name) + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
