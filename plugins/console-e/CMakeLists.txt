# Craft Console E (spec r1): channel strip with filters, 4-band EQ, compressor/limiter and expander/gate.
# One product; stereo (one linked detector) or mono through the bus layout.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftConsoleE VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (console_e_dsp INTERFACE)
target_include_directories (console_e_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (console_e_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (CONSOLE_E_PLUGIN_SOURCES
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

    juce_add_plugin (CraftConsoleE
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Craft Audio"
        COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
        PLUGIN_MANUFACTURER_CODE Crft
        PLUGIN_CODE CCnE
        PRODUCT_NAME "Craft Console E"
        BUNDLE_ID "audio.craft.CCnE"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx EQ Dynamics
        VST3_AUTO_MANIFEST TRUE)

    target_sources (CraftConsoleE PRIVATE ${CONSOLE_E_PLUGIN_SOURCES})
    target_include_directories (CraftConsoleE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    target_compile_definitions (CraftConsoleE
        PUBLIC
            CONSOLE_E_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries (CraftConsoleE
        PRIVATE
            console_e_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    clap_juce_extensions_plugin (TARGET CraftConsoleE
        CLAP_ID "com.craftaudio.console-e"
        CLAP_FEATURES audio-effect equalizer compressor stereo mono)

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (console_e_snapshot PRODUCT_NAME "console_e_snapshot")
        target_sources (console_e_snapshot PRIVATE tools/Snapshot.cpp ${CONSOLE_E_PLUGIN_SOURCES})
        target_include_directories (console_e_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (console_e_snapshot PRIVATE
            CONSOLE_E_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (console_e_snapshot PRIVATE console_e_dsp juce::juce_audio_utils juce::juce_gui_basics
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
