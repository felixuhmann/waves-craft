// Renders the editor to PNG in a few states (development aid, not shipped).
// Usage: h_delay_snapshot <output directory>
#include <iostream>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    using namespace craft::hdelay;
    struct Case
    {
        const char* name;
        std::function<void (juce::AudioProcessorValueTreeState&)> setup;
    };
    auto set = [] (juce::AudioProcessorValueTreeState& s, const char* id, float physical) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };
    const Case cases[] = {
        { "default", [] (auto&) {} },
        { "ms-dual-pingpong",
          [&] (auto& s) {
              set (s, ids::sync, 2.0f);
              set (s, ids::dual, 1.0f);
              set (s, ids::delayRMs, 520.0f);
              set (s, ids::pingPong, 1.0f);
              set (s, ids::feedback, 85.0f);
              set (s, ids::modDepth, 30.0f);
              set (s, ids::modRate, 1.15f);
              set (s, ids::lofi, 1.0f);
              set (s, ids::phaseR, 1.0f);
              s.getParameter (ids::hiPass)->setValueNotifyingHost (0.3f);
              s.getParameter (ids::loPass)->setValueNotifyingHost (0.7f);
          } },
        { "bpm-fallback",
          [&] (auto& s) {
              set (s, ids::sync, 0.0f);
              set (s, ids::bpm, 60.0f);
              set (s, ids::delayNote, 19.0f);
              set (s, ids::analog, 3.0f);
              set (s, ids::output, -4.5f);
              set (s, ids::mix, 35.0f);
          } },
    };

    for (const auto& c : cases)
    {
        HDelayProcessor proc;
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
