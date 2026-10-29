cmake_minimum_required (VERSION 3.22)

project (CraftImager VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

option (CRAFT_BUILD_PLUGINS "Build the JUCE plugins (VST3 + Standalone)" ON)
option (CRAFT_BUILD_TESTS "Build the DSP unit tests" ON)

if (MSVC)
    # static runtime: the plug-ins and the tests do not depend on an installed VC++ redistributable
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include (FetchContent)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only)
add_library (craft_dsp INTERFACE)
target_include_directories (craft_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (craft_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    FetchContent_Declare (JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.16-security.zip
        URL_HASH SHA256=7f75cee28ea4dd216fc49ba6fb57f4baa14729ef97529040d3a99dd04910f166
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable (JUCE)

    set (CRAFT_PLUGIN_SOURCES
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginProcessor.h
        source/plugin/ParameterLayout.cpp
        source/plugin/ParameterLayout.h
        source/ui/PluginEditor.cpp
        source/ui/PluginEditor.h
        source/ui/CraftLookAndFeel.cpp
        source/ui/CraftLookAndFeel.h
        source/ui/Controls.cpp
        source/ui/Controls.h
        source/ui/VectorDisplay.cpp
        source/ui/VectorDisplay.h
        source/ui/Meters.cpp
        source/ui/Meters.h)

    # craft_add_variant (<target> <product name> <plugin code> <variant index>)
    function (craft_add_variant target productName pluginCode variantIndex)
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
            VST3_CATEGORIES Fx Spatial
            VST3_AUTO_MANIFEST TRUE)

        target_sources (${target} PRIVATE ${CRAFT_PLUGIN_SOURCES})
        target_include_directories (${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

        target_compile_definitions (${target}
            PUBLIC
                CRAFT_VARIANT=${variantIndex}
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0
                JUCE_DISPLAY_SPLASH_SCREEN=0)

        target_link_libraries (${target}
            PRIVATE
                craft_dsp
                juce::juce_audio_utils
                juce::juce_audio_processors
                juce::juce_gui_basics
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)
    endfunction()

    craft_add_variant (CraftShuffler  "Craft Shuffler"  CShf 0)
    craft_add_variant (CraftImager    "Craft Imager"    CImg 1)
    craft_add_variant (CraftMsMatrix  "Craft MS Matrix" CMsm 2)
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    enable_testing()
    add_subdirectory (tests)
endif()
