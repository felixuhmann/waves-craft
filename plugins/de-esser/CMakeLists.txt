# Craft De-Esser (spec r3): Craft De-Esser (stereo) and Craft De-Esser Mono.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftDeEsser VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (de_esser_dsp INTERFACE)
target_include_directories (de_esser_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (de_esser_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (DE_ESSER_PLUGIN_SOURCES
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
        source/ui/Meters.cpp
        source/ui/Meters.h)

    # de_esser_add_variant (<target> <product name> <plugin code> <channels> <clap id>)
    function (de_esser_add_variant target productName pluginCode channels clapId)
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

        target_sources (${target} PRIVATE ${DE_ESSER_PLUGIN_SOURCES})
        target_include_directories (${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

        target_compile_definitions (${target}
            PUBLIC
                DE_ESSER_CHANNELS=${channels}
                DE_ESSER_VERSION_STRING="${PROJECT_VERSION}"
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0)

        target_link_libraries (${target}
            PRIVATE
                de_esser_dsp
                juce::juce_audio_utils
                juce::juce_audio_processors
                juce::juce_gui_basics
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)

        if (channels EQUAL 1)
            set (layoutFeature mono)
        else()
            set (layoutFeature stereo)
        endif()
        clap_juce_extensions_plugin (TARGET ${target}
            CLAP_ID "${clapId}"
            CLAP_FEATURES audio-effect ${layoutFeature} compressor)
    endfunction()

    de_esser_add_variant (CraftDeEsser     "Craft De-Esser"      CDes 2 "com.craftaudio.de-esser")
    de_esser_add_variant (CraftDeEsserMono "Craft De-Esser Mono" CDsM 1 "com.craftaudio.de-esser.mono")

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (de_esser_snapshot PRODUCT_NAME "de_esser_snapshot")
        target_sources (de_esser_snapshot PRIVATE tools/Snapshot.cpp ${DE_ESSER_PLUGIN_SOURCES})
        target_include_directories (de_esser_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (de_esser_snapshot PRIVATE
            DE_ESSER_CHANNELS=2 DE_ESSER_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (de_esser_snapshot PRIVATE de_esser_dsp juce::juce_audio_utils juce::juce_gui_basics
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
