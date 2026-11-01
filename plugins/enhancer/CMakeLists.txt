# Craft Enhancer (spec r1): five-band dynamic harmonic enhancer with per-band width.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftEnhancer VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Normative spec tables -> generated header (configure time)
include (${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedTables.cmake)
set (ENHANCER_TABLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../spec/enhancer/r1/tables)
set (ENHANCER_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
enhancer_embed_tables (${ENHANCER_GENERATED_DIR}/enhancer/SpecTables.h
    ${ENHANCER_TABLE_DIR}/crossover_taper.csv
    ${ENHANCER_TABLE_DIR}/xover_bands_default.csv
    ${ENHANCER_TABLE_DIR}/xover_bands_set2.csv
    ${ENHANCER_TABLE_DIR}/lp_prototype.csv
    ${ENHANCER_TABLE_DIR}/direct_group_delay.csv
    ${ENHANCER_TABLE_DIR}/settled_curves.csv
    ${ENHANCER_TABLE_DIR}/band_steps.csv
    ${ENHANCER_TABLE_DIR}/harmonics.csv)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (enhancer_dsp INTERFACE)
target_include_directories (enhancer_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source ${ENHANCER_GENERATED_DIR})
target_compile_features (enhancer_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (ENHANCER_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/EnhancerLookAndFeel.cpp
        source/ui/EnhancerLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/BandDisplay.cpp
        source/ui/BandDisplay.h
        source/ui/Meters.cpp
        source/ui/Meters.h)

    juce_add_plugin (CraftEnhancer
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Craft Audio"
        COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
        PLUGIN_MANUFACTURER_CODE Crft
        PLUGIN_CODE CEnh
        PRODUCT_NAME "Craft Enhancer"
        BUNDLE_ID "audio.craft.CEnh"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Dynamics
        VST3_AUTO_MANIFEST TRUE)

    target_sources (CraftEnhancer PRIVATE ${ENHANCER_SOURCES})
    target_include_directories (CraftEnhancer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    target_compile_definitions (CraftEnhancer
        PUBLIC
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries (CraftEnhancer
        PRIVATE
            enhancer_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    clap_juce_extensions_plugin (TARGET CraftEnhancer
        CLAP_ID "com.craftaudio.enhancer"
        CLAP_FEATURES audio-effect multi-effects stereo mono)

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (enhancer_snapshot PRODUCT_NAME "enhancer_snapshot")
        target_sources (enhancer_snapshot PRIVATE tools/Snapshot.cpp ${ENHANCER_SOURCES})
        target_include_directories (enhancer_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (enhancer_snapshot PRIVATE
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (enhancer_snapshot PRIVATE enhancer_dsp juce::juce_audio_utils juce::juce_gui_basics
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
