# Craft Punch Compressor (spec r1): stereo-linked compressor with punch, tempo-synced release, analog modes and an
# output peak stage. One product; stereo (2 -> 2) and mono (1 -> 1) bus layouts.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftPunchCompressor VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (punch_compressor_dsp INTERFACE)
target_include_directories (punch_compressor_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (punch_compressor_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (PUNCH_COMPRESSOR_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/PunchLookAndFeel.cpp
        source/ui/PunchLookAndFeel.h
        source/ui/Widgets.cpp
        source/ui/Widgets.h)

    juce_add_plugin (CraftPunchCompressor
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Craft Audio"
        COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
        PLUGIN_MANUFACTURER_CODE Crft
        PLUGIN_CODE CPcm
        PRODUCT_NAME "Craft Punch Compressor"
        BUNDLE_ID "audio.craft.CPcm"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Dynamics
        VST3_AUTO_MANIFEST TRUE)

    target_sources (CraftPunchCompressor PRIVATE ${PUNCH_COMPRESSOR_SOURCES})
    target_include_directories (CraftPunchCompressor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    target_compile_definitions (CraftPunchCompressor
        PUBLIC
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries (CraftPunchCompressor
        PRIVATE
            punch_compressor_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    clap_juce_extensions_plugin (TARGET CraftPunchCompressor
        CLAP_ID "com.craftaudio.punch-compressor"
        CLAP_FEATURES audio-effect compressor stereo mono)

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (punch_compressor_snapshot PRODUCT_NAME "punch_compressor_snapshot")
        target_sources (punch_compressor_snapshot PRIVATE tools/Snapshot.cpp ${PUNCH_COMPRESSOR_SOURCES})
        target_include_directories (punch_compressor_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (punch_compressor_snapshot PRIVATE
            CRAFT_VERSION_STRING="${PROJECT_VERSION}" CRAFT_NO_PLUGIN_ENTRY=1
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (punch_compressor_snapshot PRIVATE punch_compressor_dsp juce::juce_audio_utils
            juce::juce_gui_basics juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
