# Craft dynamics (spec r2): Craft Dynamics, Craft Compressor, Craft Gate, Craft Compressor SC.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftDynamics VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (dynamics_dsp INTERFACE)
target_include_directories (dynamics_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (dynamics_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (DYNAMICS_PLUGIN_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/DynamicsLookAndFeel.cpp
        source/ui/DynamicsLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/TransferGraph.cpp
        source/ui/TransferGraph.h
        source/ui/FilterGraph.cpp
        source/ui/FilterGraph.h
        source/ui/Meters.cpp
        source/ui/Meters.h)

    # dynamics_add_variant (<target> <product name> <plugin code> <variant index> <clap id> <vst3 category>)
    function (dynamics_add_variant target productName pluginCode variantIndex clapId vst3Category)
        juce_add_plugin (${target}
            VERSION ${PROJECT_VERSION}
            COMPANY_NAME "Craft Audio"
            COMPANY_WEBSITE "https://github.com/felixuhmann/waves-craft"
            PLUGIN_MANUFACTURER_CODE Crft
            PLUGIN_CODE ${pluginCode}
            PRODUCT_NAME "${productName}"
            BUNDLE_ID "audio.craft.${pluginCode}"
            FORMATS VST3 Standalone
            IS_SYNTH FALSE
            NEEDS_MIDI_INPUT FALSE
            NEEDS_MIDI_OUTPUT FALSE
            IS_MIDI_EFFECT FALSE
            EDITOR_WANTS_KEYBOARD_FOCUS FALSE
            COPY_PLUGIN_AFTER_BUILD FALSE
            VST3_CATEGORIES Fx Dynamics
            VST3_AUTO_MANIFEST TRUE)

        target_sources (${target} PRIVATE ${DYNAMICS_PLUGIN_SOURCES})
        target_include_directories (${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

        target_compile_definitions (${target}
            PUBLIC
                CRAFT_DYNAMICS_VARIANT=${variantIndex}
                CRAFT_VERSION_STRING="${PROJECT_VERSION}"
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0)

        target_link_libraries (${target}
            PRIVATE
                dynamics_dsp
                juce::juce_audio_utils
                juce::juce_audio_processors
                juce::juce_gui_basics
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)

        clap_juce_extensions_plugin (TARGET ${target}
            CLAP_ID "${clapId}"
            CLAP_FEATURES audio-effect ${vst3Category} stereo mono)
    endfunction()

    dynamics_add_variant (CraftDynamics     "Craft Dynamics"      CDyn 0 com.craftaudio.dynamics      compressor)
    dynamics_add_variant (CraftCompressor   "Craft Compressor"    CCmp 1 com.craftaudio.compressor    compressor)
    dynamics_add_variant (CraftGate         "Craft Gate"          CGat 2 com.craftaudio.gate          gate)
    dynamics_add_variant (CraftCompressorSC "Craft Compressor SC" CCsc 3 com.craftaudio.compressor-sc compressor)

    # Development aid: renders the editors to PNG (not installed / released). Option defined in the root.
    if (CRAFT_BUILD_SNAPSHOT)
        juce_add_console_app (dynamics_snapshot PRODUCT_NAME "dynamics_snapshot")
        target_sources (dynamics_snapshot PRIVATE tools/Snapshot.cpp ${DYNAMICS_PLUGIN_SOURCES})
        target_include_directories (dynamics_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_compile_definitions (dynamics_snapshot PRIVATE
            CRAFT_DYNAMICS_VARIANT=0 CRAFT_VERSION_STRING="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries (dynamics_snapshot PRIVATE dynamics_dsp juce::juce_audio_utils juce::juce_gui_basics
            juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
