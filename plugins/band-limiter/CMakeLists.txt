# Craft band limiter family (spec r1): Craft Band Limiter, Craft Band Limiter Profile, Craft Band Limiter LL,
# Craft Band Limiter LL Profile. One engine (source/dsp), four products selected by CRAFT_VARIANT.
# Added automatically by the root CMakeLists.txt; JUCE, clap-juce-extensions and Catch2 are fetched there.
project (CraftBandLimiter VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------------------------
# Host-independent DSP core (header only); uses the mastering limiter's requantiser headers.
add_library (band_limiter_dsp INTERFACE)
target_include_directories (band_limiter_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features (band_limiter_dsp INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_PLUGINS)
    set (BAND_LIMITER_SOURCES
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
        source/ui/BandDisplay.cpp
        source/ui/BandDisplay.h)

    # band_limiter_add_variant (<target> <product name> <plugin code> <variant index> <clap id>)
    function (band_limiter_add_variant target productName pluginCode variantIndex clapId)
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
            VST3_CATEGORIES Fx Dynamics Mastering
            VST3_AUTO_MANIFEST TRUE)

        target_sources (${target} PRIVATE ${BAND_LIMITER_SOURCES})
        target_include_directories (${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

        target_compile_definitions (${target}
            PUBLIC
                CRAFT_VARIANT=${variantIndex}
                CRAFT_PRODUCT_NAME="${productName}"
                CRAFT_VERSION_STRING="${PROJECT_VERSION}"
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0)

        target_link_libraries (${target}
            PRIVATE
                band_limiter_dsp
                juce::juce_audio_utils
                juce::juce_audio_processors
                juce::juce_gui_basics
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)

        clap_juce_extensions_plugin (TARGET ${target}
            CLAP_ID "${clapId}"
            CLAP_FEATURES audio-effect limiter mastering stereo mono)
    endfunction()

    band_limiter_add_variant (CraftBandLimiter          "Craft Band Limiter"            CBlm 0 com.craftaudio.band-limiter)
    band_limiter_add_variant (CraftBandLimiterProfile   "Craft Band Limiter Profile"    CBlp 1 com.craftaudio.band-limiter-profile)
    band_limiter_add_variant (CraftBandLimiterLL        "Craft Band Limiter LL"         CBll 2 com.craftaudio.band-limiter-ll)
    band_limiter_add_variant (CraftBandLimiterLLProfile "Craft Band Limiter LL Profile" CBlq 3 com.craftaudio.band-limiter-ll-profile)
endif()

# ---------------------------------------------------------------------------------------------
if (CRAFT_BUILD_TESTS)
    add_subdirectory (tests)
endif()
