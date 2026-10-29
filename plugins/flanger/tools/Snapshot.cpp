// Renders the editor of each variant to PNG (development aid, not shipped).
// Usage: flanger_snapshot <output directory>
#include <iostream>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    using namespace craft::flanger;
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    auto set = [] (juce::AudioProcessorValueTreeState& s, const char* id, float physical) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };
    struct Case
    {
        dsp::Variant v;
        const char* name;
        std::function<void (juce::AudioProcessorValueTreeState&)> setup;
        int blocks;
    };
    const Case cases[] = {
        { dsp::Variant::Stereo, "flanger-default", [] (auto&) {}, 0 },
        { dsp::Variant::Stereo, "flanger-set",
          [&] (auto& s) {
              set (s, ids::stereo, 90.0f);
              set (s, ids::rate, 1.0f);
              set (s, ids::filterOn, 1.0f);
              set (s, ids::filterType, 0.0f);
              set (s, ids::filterFreq, 3500.0f);
              set (s, ids::tape, 1.0f);
              set (s, ids::link, 1.0f);
              set (s, ids::wetInvert, 1.0f);
              set (s, ids::waveform, 1.0f);
              set (s, ids::gain, 4.5f);
          },
          40 },
        { dsp::Variant::Mono, "flanger-mono", [&] (auto& s) { set (s, ids::tempoSync, 1.0f); set (s, ids::rate, 0.5f); set (s, ids::stop, 1.0f); }, 10 },
        { dsp::Variant::MonoToStereo, "flanger-mono-stereo", [&] (auto& s) { set (s, ids::stereo, 45.0f); }, 20 },
    };

    for (const auto& c : cases)
    {
        FlangerProcessor proc (c.v);
        c.setup (proc.getState());
        proc.setRateAndBufferSizeDetails (48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < c.blocks; ++b)
        {
            for (int i = 0; i < 512; ++i)
                buf.setSample (0, i, 0.9f * std::sin (float (b * 512 + i) * 0.05f)), buf.setSample (1, i, buf.getSample (0, i));
            proc.processBlock (buf, midi);
        }
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile (juce::String (c.name) + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
