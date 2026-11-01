# Craft Dynamic Distortion (spec r1): pre-distortion tone EQ, drive, envelope-controlled expansion, anti-aliased
# clipper, post filters and a parallel direct path.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftDynamicDistortion VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (dynamic_distortion_dsp INTERFACE)
target_include_directories (dynamic_distortion_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (dynamic_distortion_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (DYNAMIC_DISTORTION_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/DistortionLookAndFeel.cpp
        source/ui/DistortionLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/ToneDisplay.cpp
        source/ui/ToneDisplay.h)

    juce_add_plugin (CraftDynamicDistortion
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Craft Audio"
        COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
        PLUGIN_MANUFACTURER_CODE Crft
        PLUGIN_CODE CDds
        PRODUCT_NAME "Craft Dynamic Distortion"
        BUNDLE_ID "audio.craft.CDds"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Distortion
        VST3_AUTO_MANIFEST TRUE)

    target_sources (CraftDynamicDistortion PRIVATE ${DYNAMIC_DISTORTION_SOURCES})
    target_include_directories (CraftDynamicDistortion PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    target_compile_definitions (CraftDynamicDistortion
        PUBLIC
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries (CraftDynamicDistortion
        PRIVATE
            dynamic_distortion_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    clap_juce_extensions_plugin (TARGET CraftDynamicDistortion
        CLAP_ID "com.craftaudio.dynamic-distortion"
        CLAP_FEATURES audio-effect distortion stereo mono)

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (dynamic_distortion_snapshot PRODUCT_NAME "dynamic_distortion_snapshot")
        target_sources (dynamic_distortion_snapshot PRIVATE tools/Snapshot.cpp ${DYNAMIC_DISTORTION_SOURCES})
        target_include_directories (dynamic_distortion_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (dynamic_distortion_snapshot PRIVATE
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (dynamic_distortion_snapshot PRIVATE dynamic_distortion_dsp juce::juce_audio_utils
            juce::juce_gui_basics juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)

    # Development aid: expansion-curve calibration and spec report (not registered with CTest).
    add_executable (dynamic_distortion_calibrate tools/Calibrate.cpp)
    target_link_libraries (dynamic_distortion_calibrate PRIVATE dynamic_distortion_dsp)
    target_compile_features (dynamic_distortion_calibrate PRIVATE cxx_std_17)
endif()
