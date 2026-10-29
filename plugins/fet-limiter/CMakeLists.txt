# Craft FET limiter (spec r1): Craft FET Limiter (stereo with linked detector, mono via bus layout).
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftFetLimiter VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Normative spec tables, embedded as a generated header (no files are read at run time).
include (${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedTables.cmake)
set (FET_LIMITER_TABLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../spec/fet-limiter/r1/tables)
set (FET_LIMITER_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
fet_limiter_embed_tables (${FET_LIMITER_GENERATED_DIR}/fet_limiter/SpecTables.h
    gainInputCsv=${FET_LIMITER_TABLE_DIR}/gain_input.csv
    gainOutputCsv=${FET_LIMITER_TABLE_DIR}/gain_output.csv
    staticGrCsv=${FET_LIMITER_TABLE_DIR}/static_gr.csv
    timingCsv=${FET_LIMITER_TABLE_DIR}/timing.csv
    releaseTrajectoriesCsv=${FET_LIMITER_TABLE_DIR}/release_trajectories.csv
    freqResponseCsv=${FET_LIMITER_TABLE_DIR}/freq_response.csv
    harmonicsCsv=${FET_LIMITER_TABLE_DIR}/harmonics.csv
    noiseCsv=${FET_LIMITER_TABLE_DIR}/noise.csv)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (fet_limiter_dsp INTERFACE)
target_include_directories (fet_limiter_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source ${FET_LIMITER_GENERATED_DIR})
target_compile_features (fet_limiter_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (FET_LIMITER_PLUGIN_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/FetLookAndFeel.cpp
        source/ui/FetLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/NeedleMeter.cpp
        source/ui/NeedleMeter.h)

    juce_add_plugin (CraftFetLimiter
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Craft Audio"
        COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
        PLUGIN_MANUFACTURER_CODE Crft
        PLUGIN_CODE CFet
        PRODUCT_NAME "Craft FET Limiter"
        BUNDLE_ID "audio.craft.CFet"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Dynamics
        VST3_AUTO_MANIFEST TRUE)

    target_sources (CraftFetLimiter PRIVATE ${FET_LIMITER_PLUGIN_SOURCES})
    target_include_directories (CraftFetLimiter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    target_compile_definitions (CraftFetLimiter
        PUBLIC
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries (CraftFetLimiter
        PRIVATE
            fet_limiter_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    clap_juce_extensions_plugin (TARGET CraftFetLimiter
        CLAP_ID "com.craftaudio.fet-limiter"
        CLAP_FEATURES audio-effect compressor limiter stereo mono)

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (fet_limiter_snapshot PRODUCT_NAME "fet_limiter_snapshot")
        target_sources (fet_limiter_snapshot PRIVATE tools/Snapshot.cpp ${FET_LIMITER_PLUGIN_SOURCES})
        target_include_directories (fet_limiter_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (fet_limiter_snapshot PRIVATE
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (fet_limiter_snapshot PRIVATE fet_limiter_dsp juce::juce_audio_utils
            juce::juce_gui_basics juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
