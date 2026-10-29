// Renders the editor of each variant to PNG (development aid, not shipped).
// Usage: craft_ui_snapshot <output directory>
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
        craft::dsp::Variant v;
        const char* name;
        std::function<void (juce::AudioProcessorValueTreeState&)> setup;
    };
    auto set = [] (juce::AudioProcessorValueTreeState& s, const char* id, float physical) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };
    const Case cases[] = {
        { craft::dsp::Variant::Full, "shuffler-neutral", [] (auto&) {} },
        { craft::dsp::Variant::Full, "shuffler-set",
          [&] (auto& s) {
              set (s, craft::ids::width, 2.08f);
              set (s, craft::ids::rotation, 12.5f);
              set (s, craft::ids::gain, -4.5f);
              set (s, craft::ids::shuffle, 2.4f);
              set (s, craft::ids::bassTrim, 1.5f);
              set (s, craft::ids::polarity2, 1.0f);
          } },
        { craft::dsp::Variant::Imager, "imager", [&] (auto& s) { set (s, craft::ids::width, 0.48f); set (s, craft::ids::asymmetry, 30.0f); } },
        { craft::dsp::Variant::MsMatrix, "msmatrix", [] (auto&) {} },
    };

    for (const auto& c : cases)
    {
        craft::CraftProcessor proc (c.v);
        c.setup (proc.getState());
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        // let the editor's timer pick up the parameter state once
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile (juce::String (c.name) + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
