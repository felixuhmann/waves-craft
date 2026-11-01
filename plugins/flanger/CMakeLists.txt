# Craft flanger (spec r1): Craft Flanger (stereo), Craft Flanger Mono, Craft Flanger Mono-Stereo.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftFlanger VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (flanger_dsp INTERFACE)
target_include_directories (flanger_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (flanger_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (FLANGER_PLUGIN_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/FlangerLookAndFeel.cpp
        source/ui/FlangerLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/LfoDisplay.cpp
        source/ui/LfoDisplay.h
        source/ui/Meters.cpp
        source/ui/Meters.h)

    # flanger_add_variant (<target> <product name> <plugin code> <variant index> <clap id>)
    function (flanger_add_variant target productName pluginCode variantIndex clapId)
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

        target_sources (${target} PRIVATE ${FLANGER_PLUGIN_SOURCES})
        target_include_directories (${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

        target_compile_definitions (${target}
            PUBLIC
                CRAFT_FLANGER_VARIANT=${variantIndex}
                CRAFT_VERSION_STRING="${PROJECT_VERSION}"
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0)

        target_link_libraries (${target}
            PRIVATE
                flanger_dsp
                juce::juce_audio_utils
                juce::juce_audio_processors
                juce::juce_gui_basics
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)

        clap_juce_extensions_plugin (TARGET ${target}
            CLAP_ID "${clapId}"
            CLAP_FEATURES audio-effect flanger chorus)
    endfunction()

    flanger_add_variant (CraftFlanger           "Craft Flanger"             CFlg 0 com.craftaudio.flanger)
    flanger_add_variant (CraftFlangerMono       "Craft Flanger Mono"        CFlm 1 com.craftaudio.flanger.mono)
    flanger_add_variant (CraftFlangerMonoStereo "Craft Flanger Mono-Stereo" CFms 2 com.craftaudio.flanger.mono-stereo)

    # Development aid: renders the editors to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (flanger_snapshot PRODUCT_NAME "flanger_snapshot")
        target_sources (flanger_snapshot PRIVATE tools/Snapshot.cpp ${FLANGER_PLUGIN_SOURCES})
        target_include_directories (flanger_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (flanger_snapshot PRIVATE
            CRAFT_FLANGER_VARIANT=0 CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (flanger_snapshot PRIVATE flanger_dsp juce::juce_audio_utils juce::juce_gui_basics
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
