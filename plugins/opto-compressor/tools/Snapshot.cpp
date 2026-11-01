// Renders the editor to PNG (development aid, not shipped). Usage: opto_compressor_snapshot <output directory>
#include <iostream>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    using namespace craft::opto;
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    OptoProcessor proc;
    auto set = [&] (const char* id, float physical) {
        auto* p = proc.getState().getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };
    set (ids::peakReduction, 7.0f);
    set (ids::gain, 6.5f);
    set (ids::hiFreq, 60.0f);
    proc.setRateAndBufferSizeDetails (48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < 60; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float v = 0.3f * std::sin (float (b * 512 + i) * 0.13f);
            buf.setSample (0, i, v);
            buf.setSample (1, i, 0.8f * v);
        }
        proc.processBlock (buf, midi);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
    auto f = outDir.getChildFile ("opto-compressor.png");
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::cout << "wrote " << f.getFullPathName() << std::endl;
    return 0;
}
