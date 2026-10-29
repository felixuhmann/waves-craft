// Renders the editor to PNG (development aid, not shipped).
// Usage: color_delay_snapshot <output directory>
#include <iostream>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    auto set = [] (juce::AudioProcessorValueTreeState& s, const char* id, float physical) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };

    struct Case
    {
        const char* name;
        std::function<void (juce::AudioProcessorValueTreeState&)> setup;
    };
    const Case cases[] = {
        { "default", [] (auto&) {} },
        { "ms-effects",
          [&] (auto& s) {
              set (s, craft::ids::mode, 0.0f);
              set (s, craft::ids::delayLink, 0.0f);
              set (s, craft::ids::delayMsL, 376.0f);
              set (s, craft::ids::delayMsR, 512.0f);
              set (s, craft::ids::hp, 150.0f);
              set (s, craft::ids::lp, 8000.0f);
              set (s, craft::ids::verbOn, 1.0f);
              set (s, craft::ids::distOn, 1.0f);
              set (s, craft::ids::phaserOn, 1.0f);
              set (s, craft::ids::monitor, 0.0f);
          } },
    };

    for (const auto& c : cases)
    {
        craft::ColourDelayProcessor proc;
        c.setup (proc.getState());
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
        auto f = outDir.getChildFile (juce::String (c.name) + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
