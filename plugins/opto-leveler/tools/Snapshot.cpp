// Renders the editor to PNG (development aid, not shipped).
// Usage: opto_leveler_snapshot <output directory>
#include <functional>
#include <iostream>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    using namespace craft::opto_leveler;
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
        std::function<void (OptoLevelerProcessor&)> setup;
        int blocks;
        bool mono;
    };
    const Case cases[] = {
        { "opto-leveler-default", [] (auto&) {}, 0, false },
        { "opto-leveler-active",
          [&] (auto& p) {
              auto& s = p.getState();
              set (s, ids::peakReduction, 62.0f);
              set (s, ids::gain, 47.5f);
              set (s, ids::mode, 1.0f);
              set (s, ids::hiFreq, 100.0f);
              set (s, ids::analog, 1.0f);
              set (s, ids::mix, 85.0f);
              set (s, ids::trim, -2.5f);
              set (s, ids::autoMakeup, 1.0f);
          },
          90, false },
        { "opto-leveler-out-meter", [&] (auto& p) { set (p.getState(), ids::vu, 2.0f); }, 60, true },
    };

    for (const auto& c : cases)
    {
        OptoLevelerProcessor proc;
        if (c.mono)
        {
            auto layout = proc.getBusesLayout();
            layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
            layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
            proc.setBusesLayout (layout);
        }
        c.setup (proc);
        proc.setRateAndBufferSizeDetails (48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < c.blocks; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float t = float (b * 512 + i);
                buf.setSample (0, i, 0.5f * std::sin (t * 0.031f) * (0.6f + 0.4f * std::sin (t * 0.0007f)));
                buf.setSample (1, i, 0.4f * std::sin (t * 0.017f));
            }
            proc.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile (juce::String (c.name) + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
