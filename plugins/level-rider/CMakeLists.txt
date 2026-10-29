# Craft Level Rider (spec r1): Craft Level Rider and Craft Level Rider Live (adds the Spill gate).
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftLevelRider VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (level_rider_dsp INTERFACE)
target_include_directories (level_rider_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (level_rider_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (LEVEL_RIDER_PLUGIN_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/LookAndFeel.cpp
        source/ui/LookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/Faders.cpp
        source/ui/Faders.h)

    # level_rider_add_variant (<target> <product name> <plugin code> <live 0|1> <clap id>)
    function (level_rider_add_variant target productName pluginCode live clapId)
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
            VST3_CATEGORIES Fx Dynamics
            VST3_AUTO_MANIFEST TRUE)

        target_sources (${target} PRIVATE ${LEVEL_RIDER_PLUGIN_SOURCES})
        target_include_directories (${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

        target_compile_definitions (${target}
            PUBLIC
                LEVEL_RIDER_LIVE=${live}
                LEVEL_RIDER_VERSION_STRING="${PROJECT_VERSION}"
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0)

        target_link_libraries (${target}
            PRIVATE
                level_rider_dsp
                juce::juce_audio_utils
                juce::juce_audio_processors
                juce::juce_gui_basics
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)

        clap_juce_extensions_plugin (TARGET ${target}
            CLAP_ID "${clapId}"
            CLAP_FEATURES audio-effect stereo mono utility)
    endfunction()

    level_rider_add_variant (CraftLevelRider     "Craft Level Rider"      CLrd 0 "com.craftaudio.level-rider")
    level_rider_add_variant (CraftLevelRiderLive "Craft Level Rider Live" CLrl 1 "com.craftaudio.level-rider-live")

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (level_rider_snapshot PRODUCT_NAME "level_rider_snapshot")
        target_sources (level_rider_snapshot PRIVATE tools/Snapshot.cpp ${LEVEL_RIDER_PLUGIN_SOURCES})
        target_include_directories (level_rider_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (level_rider_snapshot PRIVATE
            LEVEL_RIDER_LIVE=1 LEVEL_RIDER_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (level_rider_snapshot PRIVATE level_rider_dsp juce::juce_audio_utils juce::juce_gui_basics
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
