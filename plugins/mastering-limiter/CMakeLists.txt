# Craft mastering limiter (spec r1): Craft Mastering Limiter (stereo, mono via bus layout).
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftMasteringLimiter VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (mastering_limiter_dsp INTERFACE)
target_include_directories (mastering_limiter_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (mastering_limiter_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (MASTERING_LIMITER_PLUGIN_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/LimiterLookAndFeel.cpp
        source/ui/LimiterLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/Meters.cpp
        source/ui/Meters.h)

    juce_add_plugin (CraftMasteringLimiter
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Craft Audio"
        COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
        PLUGIN_MANUFACTURER_CODE Crft
        PLUGIN_CODE CMlm
        PRODUCT_NAME "Craft Mastering Limiter"
        BUNDLE_ID "audio.craft.CMlm"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Dynamics Mastering
        VST3_AUTO_MANIFEST TRUE)

    target_sources (CraftMasteringLimiter PRIVATE ${MASTERING_LIMITER_PLUGIN_SOURCES})
    target_include_directories (CraftMasteringLimiter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    target_compile_definitions (CraftMasteringLimiter
        PUBLIC
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries (CraftMasteringLimiter
        PRIVATE
            mastering_limiter_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    clap_juce_extensions_plugin (TARGET CraftMasteringLimiter
        CLAP_ID "com.craftaudio.mastering-limiter"
        CLAP_FEATURES audio-effect limiter mastering stereo mono)

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (mastering_limiter_snapshot PRODUCT_NAME "mastering_limiter_snapshot")
        target_sources (mastering_limiter_snapshot PRIVATE tools/Snapshot.cpp ${MASTERING_LIMITER_PLUGIN_SOURCES})
        target_include_directories (mastering_limiter_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (mastering_limiter_snapshot PRIVATE
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (mastering_limiter_snapshot PRIVATE mastering_limiter_dsp juce::juce_audio_utils
            juce::juce_gui_basics juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
