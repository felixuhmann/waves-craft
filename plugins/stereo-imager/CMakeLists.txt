# Craft stereo imager (spec r1): Craft Shuffler, Craft Imager, Craft MS Matrix.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftStereoImager VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (stereo_imager_dsp INTERFACE)
target_include_directories (stereo_imager_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (stereo_imager_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (CRAFT_PLUGIN_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/CraftLookAndFeel.cpp
        source/ui/CraftLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/VectorDisplay.cpp
        source/ui/VectorDisplay.h
        source/ui/Meters.cpp
        source/ui/Meters.h)

    # stereo_imager_add_variant (<target> <product name> <plugin code> <variant index> <clap id suffix>)
    function (stereo_imager_add_variant target productName pluginCode variantIndex clapName)
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
            VST3_CATEGORIES Fx Spatial
            VST3_AUTO_MANIFEST TRUE)

        target_sources (${target} PRIVATE ${CRAFT_PLUGIN_SOURCES})
        target_include_directories (${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

        target_compile_definitions (${target}
            PUBLIC
                CRAFT_VARIANT=${variantIndex}
                CRAFT_VERSION_STRING="${PROJECT_VERSION}"
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0)

        target_link_libraries (${target}
            PRIVATE
                stereo_imager_dsp
                juce::juce_audio_utils
                juce::juce_audio_processors
                juce::juce_gui_basics
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)

        clap_juce_extensions_plugin (TARGET ${target}
            CLAP_ID "com.craftaudio.${clapName}"
            CLAP_FEATURES audio-effect stereo utility)
    endfunction()

    stereo_imager_add_variant (CraftShuffler  "Craft Shuffler"  CShf 0 shuffler)
    stereo_imager_add_variant (CraftImager    "Craft Imager"    CImg 1 imager)
    stereo_imager_add_variant (CraftMsMatrix  "Craft MS Matrix" CMsm 2 msmatrix)

    # Development aid: renders each editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (stereo_imager_snapshot PRODUCT_NAME "stereo_imager_snapshot")
        target_sources (stereo_imager_snapshot PRIVATE tools/Snapshot.cpp ${CRAFT_PLUGIN_SOURCES})
        target_include_directories (stereo_imager_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (stereo_imager_snapshot PRIVATE
            CRAFT_VARIANT=0 CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (stereo_imager_snapshot PRIVATE stereo_imager_dsp juce::juce_audio_utils juce::juce_gui_basics
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
