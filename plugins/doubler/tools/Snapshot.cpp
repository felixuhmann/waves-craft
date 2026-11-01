// Renders the editors to PNG (development aid, not shipped).
// Usage: doubler_snapshot <output directory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    auto set = [] (juce::AudioProcessorValueTreeState& s, const juce::String& id, float physical) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };
    struct Case
    {
        int voices;
        const char* name;
        std::function<void (juce::AudioProcessorValueTreeState&)> setup;
    };
    namespace ids = craft::doubler::ids;
    const Case cases[] = {
        { 4, "doubler4-default", [] (auto&) {} },
        { 4, "doubler4-set",
          [&] (auto& s) {
              set (s, ids::voice (1, ids::on), 1.0f);
              set (s, ids::voice (1, ids::depth), 80.0f);
              set (s, ids::voice (3, ids::on), 1.0f);
              set (s, ids::voice (3, ids::octave), 1.0f);
              set (s, ids::voice (0, ids::feedback), 35.0f);
              set (s, ids::align, 1.0f);
              set (s, ids::range, 1.0f);
              set (s, ids::eqLowGain, -9.0f);
              set (s, ids::eqHighOn, 0.0f);
              set (s, ids::directPan, -20.0f);
              set (s, ids::directGain, -6.0f);
              set (s, ids::routingA, 1.0f);
              set (s, ids::routingB, 2.0f);
          } },
        { 2, "doubler2-default", [] (auto&) {} },
    };

    for (const auto& c : cases)
    {
        craft::doubler::DoublerProcessor proc (c.voices);
        c.setup (proc.getState());
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
        auto f = outDir.getChildFile (juce::String (c.name) + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
