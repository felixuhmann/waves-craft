# Craft Hybrid Delay (spec r1): one product, stereo (2 -> 2) and mono-to-stereo (1 -> 2) bus layouts.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftHybridDelay VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (h_delay_dsp INTERFACE)
target_include_directories (h_delay_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (h_delay_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (H_DELAY_PLUGIN_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/HDelayLookAndFeel.cpp
        source/ui/HDelayLookAndFeel.h
        source/ui/Widgets.cpp
        source/ui/Widgets.h)

    juce_add_plugin (CraftHybridDelay
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Craft Audio"
        COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
        PLUGIN_MANUFACTURER_CODE Crft
        PLUGIN_CODE CHdl
        PRODUCT_NAME "Craft Hybrid Delay"
        BUNDLE_ID "audio.craft.CHdl"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Delay
        VST3_AUTO_MANIFEST TRUE)

    target_sources (CraftHybridDelay PRIVATE ${H_DELAY_PLUGIN_SOURCES})
    target_include_directories (CraftHybridDelay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    target_compile_definitions (CraftHybridDelay
        PUBLIC
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries (CraftHybridDelay
        PRIVATE
            h_delay_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    clap_juce_extensions_plugin (TARGET CraftHybridDelay
        CLAP_ID "com.craftaudio.h-delay"
        CLAP_FEATURES audio-effect delay stereo)

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (h_delay_snapshot PRODUCT_NAME "h_delay_snapshot")
        target_sources (h_delay_snapshot PRIVATE tools/Snapshot.cpp ${H_DELAY_PLUGIN_SOURCES})
        target_include_directories (h_delay_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (h_delay_snapshot PRIVATE
            CRAFT_VERSION_STRING="${PROJECT_VERSION}" CRAFT_NO_PLUGIN_ENTRY=1
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (h_delay_snapshot PRIVATE h_delay_dsp juce::juce_audio_utils juce::juce_gui_basics
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
