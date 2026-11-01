# Craft Audio headless plug-in stress host (development aid, not shipped). Enabled with -DCRAFT_BUILD_HOSTSTRESS=ON.
# Usage: craft_hoststress "<path>/Craft Hybrid Delay.vst3" --seconds 600 [--seed 1] [--editor]
juce_add_console_app (craft_hoststress PRODUCT_NAME "craft_hoststress")
target_sources (craft_hoststress PRIVATE HostStress.cpp)
target_compile_definitions (craft_hoststress PRIVATE
    JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=0 JUCE_PLUGINHOST_LADSPA=0 JUCE_PLUGINHOST_LV2=0
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries (craft_hoststress PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_basics
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
