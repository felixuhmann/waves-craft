# Craft Opto Leveler (spec r1): optical leveling amplifier, stereo (linked) and mono via bus layout.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftOptoLeveler VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Normative spec tables (T0 ... T8) compiled in: the Markdown tables are converted to a header at configure time
# (re-run automatically when the spec files change), so nothing is read at runtime.
set (OPTO_LEVELER_SPEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../spec/opto-leveler/r1)
set (OPTO_LEVELER_SPEC_TABLES ${OPTO_LEVELER_SPEC_DIR}/TABLES.md ${OPTO_LEVELER_SPEC_DIR}/TABLES_extra.md)
set (OPTO_LEVELER_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set_property (DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${OPTO_LEVELER_SPEC_TABLES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedSpecTables.cmake)
execute_process (
    COMMAND ${CMAKE_COMMAND} -DOUT=${OPTO_LEVELER_GENERATED_DIR}/SpecTables.h "-DINPUTS=${OPTO_LEVELER_SPEC_TABLES}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedSpecTables.cmake
    RESULT_VARIABLE OPTO_LEVELER_EMBED_RESULT)
if (NOT OPTO_LEVELER_EMBED_RESULT EQUAL 0)
    message (FATAL_ERROR "opto-leveler: embedding the spec tables failed")
endif()

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (opto_leveler_dsp INTERFACE)
target_include_directories (opto_leveler_dsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp
    ${OPTO_LEVELER_GENERATED_DIR})
target_compile_features (opto_leveler_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (OPTO_LEVELER_PLUGIN_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/OptoLookAndFeel.cpp
        source/ui/OptoLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/LevelMeter.cpp
        source/ui/LevelMeter.h)

    juce_add_plugin (CraftOptoLeveler
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Craft Audio"
        COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
        PLUGIN_MANUFACTURER_CODE Crft
        PLUGIN_CODE COpt
        PRODUCT_NAME "Craft Opto Leveler"
        BUNDLE_ID "audio.craft.COpt"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Dynamics
        VST3_AUTO_MANIFEST TRUE)

    target_sources (CraftOptoLeveler PRIVATE ${OPTO_LEVELER_PLUGIN_SOURCES})
    target_include_directories (CraftOptoLeveler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    target_compile_definitions (CraftOptoLeveler
        PUBLIC
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries (CraftOptoLeveler
        PRIVATE
            opto_leveler_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    clap_juce_extensions_plugin (TARGET CraftOptoLeveler
        CLAP_ID "com.craftaudio.opto-leveler"
        CLAP_FEATURES audio-effect compressor stereo mono)

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (opto_leveler_snapshot PRODUCT_NAME "opto_leveler_snapshot")
        target_sources (opto_leveler_snapshot PRIVATE tools/Snapshot.cpp ${OPTO_LEVELER_PLUGIN_SOURCES})
        target_include_directories (opto_leveler_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (opto_leveler_snapshot PRIVATE
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (opto_leveler_snapshot PRIVATE opto_leveler_dsp juce::juce_audio_utils
            juce::juce_gui_basics juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
