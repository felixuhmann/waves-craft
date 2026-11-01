# Craft Motion Filter (spec r1): modulated multimode filter with LFO / step sequencer / envelope follower,
# comb and amp modes, drive, crush and a tempo-synced delay.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftMotionFilter VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (motion_filter_dsp INTERFACE)
target_include_directories (motion_filter_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (motion_filter_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (MOTION_FILTER_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/MotionLookAndFeel.cpp
        source/ui/MotionLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/ResponseDisplay.cpp
        source/ui/ResponseDisplay.h
        source/ui/StepEditor.cpp
        source/ui/StepEditor.h)

    juce_add_plugin (CraftMotionFilter
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Craft Audio"
        COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
        PLUGIN_MANUFACTURER_CODE Crft
        PLUGIN_CODE CMfl
        PRODUCT_NAME "Craft Motion Filter"
        BUNDLE_ID "audio.craft.CMfl"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Filter
        VST3_AUTO_MANIFEST TRUE)

    target_sources (CraftMotionFilter PRIVATE ${MOTION_FILTER_SOURCES})
    target_include_directories (CraftMotionFilter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    target_compile_definitions (CraftMotionFilter
        PUBLIC
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries (CraftMotionFilter
        PRIVATE
            motion_filter_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    clap_juce_extensions_plugin (TARGET CraftMotionFilter
        CLAP_ID "com.craftaudio.motion-filter"
        CLAP_FEATURES audio-effect filter stereo)

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (motion_filter_snapshot PRODUCT_NAME "motion_filter_snapshot")
        target_sources (motion_filter_snapshot PRIVATE tools/Snapshot.cpp ${MOTION_FILTER_SOURCES})
        target_include_directories (motion_filter_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (motion_filter_snapshot PRIVATE
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (motion_filter_snapshot PRIVATE motion_filter_dsp juce::juce_audio_utils juce::juce_gui_basics
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
