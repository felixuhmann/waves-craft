# Craft doubler (spec r1): Craft Doubler 4 and Craft Doubler 2.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftDoubler VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (doubler_dsp INTERFACE)
target_include_directories (doubler_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (doubler_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (DOUBLER_PLUGIN_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/DoublerLookAndFeel.cpp
        source/ui/DoublerLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/Displays.cpp
        source/ui/Displays.h
        source/ui/Meters.cpp
        source/ui/Meters.h)

    # doubler_add_variant (<target> <product name> <plugin code> <number of voices> <clap id>)
    function (doubler_add_variant target productName pluginCode numVoices clapId)
        juce_add_plugin (${target}
            VERSION ${PROJECT_VERSION}
            COMPANY_NAME "Craft Audio"
            COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
            PLUGIN_MANUFACTURER_CODE Crft
            PLUGIN_CODE ${pluginCode}
            PRODUCT_NAME "${productName}"
            BUNDLE_ID "audio.craft.${pluginCode}"
            FORMATS VST3 Standalone
            IS_SYNTH FALSE
            NEEDS_MIDI_INPUT FALSE
            NEEDS_MIDI_OUTPUT FALSE
            IS_MIDI_EFFECT FALSE
            EDITOR_WANTS_KEYBOARD_FOCUS FALSE
            COPY_PLUGIN_AFTER_BUILD FALSE
            VST3_CATEGORIES Fx Modulation
            VST3_AUTO_MANIFEST TRUE)

        target_sources (${target} PRIVATE ${DOUBLER_PLUGIN_SOURCES})
        target_include_directories (${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

        target_compile_definitions (${target}
            PUBLIC
                DOUBLER_VOICES=${numVoices}
                DOUBLER_VERSION_STRING="${PROJECT_VERSION}"
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0)

        target_link_libraries (${target}
            PRIVATE
                doubler_dsp
                juce::juce_audio_utils
                juce::juce_audio_processors
                juce::juce_gui_basics
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)

        clap_juce_extensions_plugin (TARGET ${target}
            CLAP_ID "${clapId}"
            CLAP_FEATURES audio-effect stereo chorus pitch-shifter)
    endfunction()

    doubler_add_variant (CraftDoubler4 "Craft Doubler 4" CDb4 4 com.craftaudio.doubler4)
    doubler_add_variant (CraftDoubler2 "Craft Doubler 2" CDb2 2 com.craftaudio.doubler2)

    # Development aid: renders the editors to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (doubler_snapshot PRODUCT_NAME "doubler_snapshot")
        target_sources (doubler_snapshot PRIVATE tools/Snapshot.cpp ${DOUBLER_PLUGIN_SOURCES})
        target_include_directories (doubler_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (doubler_snapshot PRIVATE
            DOUBLER_VOICES=4 DOUBLER_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (doubler_snapshot PRIVATE doubler_dsp juce::juce_audio_utils juce::juce_gui_basics
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
