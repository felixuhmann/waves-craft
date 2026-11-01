// Renders the editor to PNG (development aid, not shipped). Usage: enhancer_snapshot <output directory>
#include <iostream>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    namespace ids = craft::enhancer::ids;
    craft::enhancer::EnhancerProcessor proc;
    auto set = [&] (const juce::String& id, float v) {
        auto* p = proc.getState().getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    set (ids::bandGain (1), 24.0f);
    set (ids::bandGain (3), 12.0f);
    set (ids::width (4), 2.0f);
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < 40; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            buf.setSample (0, i, 0.2f * std::sin (0.05f * float (b * 512 + i)));
            buf.setSample (1, i, 0.15f * std::sin (0.05f * float (b * 512 + i)));
        }
        proc.processBlock (buf, midi);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
    auto f = outDir.getChildFile ("enhancer.png");
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::cout << "wrote " << f.getFullPathName() << std::endl;
    return 0;
}
