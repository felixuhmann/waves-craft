# Craft Opto Compressor (spec r1). Added automatically by the root CMakeLists.txt.
project (CraftOptoCompressor VERSION 0.1.0 LANGUAGES C CXX)

add_library (opto_compressor_dsp INTERFACE)
target_include_directories (opto_compressor_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (opto_compressor_dsp INTERFACE cxx_std_17)

if (CRAFT_BUILD_PLUGINS)
    set (OPTO_COMPRESSOR_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h)

    juce_add_plugin (CraftOptoCompressor
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Craft Audio"
        COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
        PLUGIN_MANUFACTURER_CODE Crft
        PLUGIN_CODE COcp
        PRODUCT_NAME "Craft Opto Compressor"
        BUNDLE_ID "audio.craft.COcp"
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Dynamics
        VST3_AUTO_MANIFEST TRUE)

    target_sources (CraftOptoCompressor PRIVATE ${OPTO_COMPRESSOR_SOURCES})
    target_include_directories (CraftOptoCompressor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
    target_compile_definitions (CraftOptoCompressor
        PUBLIC
            CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries (CraftOptoCompressor
        PRIVATE
            opto_compressor_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
    clap_juce_extensions_plugin (TARGET CraftOptoCompressor
        CLAP_ID "com.craftaudio.opto-compressor"
        CLAP_FEATURES audio-effect compressor stereo mono)

    # Development aid: renders the editor to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (opto_compressor_snapshot PRODUCT_NAME "opto_compressor_snapshot")
        target_sources (opto_compressor_snapshot PRIVATE tools/Snapshot.cpp ${OPTO_COMPRESSOR_SOURCES})
        target_include_directories (opto_compressor_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (opto_compressor_snapshot PRIVATE
            CRAFT_OPTO_NO_ENTRY=1 CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (opto_compressor_snapshot PRIVATE opto_compressor_dsp juce::juce_audio_utils juce::juce_gui_basics
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
