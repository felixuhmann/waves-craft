# Craft Vocal Compressor (spec r1): look-ahead gate + saturating compressor with automatic makeup, stereo and mono
# in one plug-in. Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftVocalCompressor VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (vocal_compressor_dsp INTERFACE)
target_include_directories (vocal_compressor_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (vocal_compressor_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (VOCAL_COMPRESSOR_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/VocalLookAndFeel.cpp
        source/ui/VocalLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h)

    juce_add_plugin (CraftVocalCompressor
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Craft Audio"
        COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
        PLUGIN_MANUFACTURER_CODE Crft
        PLUGIN_CODE CVoc
        PRODUCT_NAME "Craft Vocal Compressor"
        BUNDLE_ID "audio.craft.CVoc"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Dynamics
        VST3_AUTO_MANIFEST TRUE)

    target_sources (CraftVocalCompressor PRIVATE ${VOCAL_COMPRESSOR_SOURCES})
    target_include_directories (CraftVocalCompressor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    target_compile_definitions (CraftVocalCompressor
        PUBLIC
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries (CraftVocalCompressor
        PRIVATE
            vocal_compressor_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    clap_juce_extensions_plugin (TARGET CraftVocalCompressor
        CLAP_ID "com.craftaudio.vocal-compressor"
        CLAP_FEATURES audio-effect compressor stereo mono)

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (vocal_compressor_snapshot PRODUCT_NAME "vocal_compressor_snapshot")
        target_sources (vocal_compressor_snapshot PRIVATE tools/Snapshot.cpp ${VOCAL_COMPRESSOR_SOURCES})
        target_include_directories (vocal_compressor_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (vocal_compressor_snapshot PRIVATE
            CRAFT_VERSION_STRING="${PROJECT_VERSION}" CRAFT_NO_PLUGIN_ENTRY=1
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (vocal_compressor_snapshot PRIVATE vocal_compressor_dsp juce::juce_audio_utils
            juce::juce_gui_basics juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
