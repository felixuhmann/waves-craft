cmake_minimum_required (VERSION 3.22)

# Craft Audio monorepo. Shared setup only: every plug-in lives in plugins/<slug>/ with its own
# CMakeLists.txt and is picked up automatically below, so adding a plug-in never edits this file.
project (CraftAudio VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

option (CRAFT_BUILD_PLUGINS "Build the JUCE plugins (VST3 + CLAP + Standalone)" ON)
option (CRAFT_BUILD_TESTS "Build the unit tests" ON)
option (CRAFT_BUILD_SNAPSHOT "Build the editor snapshot tools (development aid, needs CRAFT_BUILD_PLUGINS)" OFF)

if (MSVC)
    # static runtime: the plug-ins and the tests do not depend on an installed VC++ redistributable
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include (FetchContent)

# ---------------------------------------------------------------------------------------------
# Shared third-party dependencies (fetched once for all plug-ins)
if (CRAFT_BUILD_PLUGINS)
    FetchContent_Declare (JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.16-security.zip
        URL_HASH SHA256=7f75cee28ea4dd216fc49ba6fb57f4baa14729ef97529040d3a99dd04910f166
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable (JUCE)

    # CLAP wrapper for JUCE (MIT). Uses git submodules (clap, clap-helpers), so it is pinned by commit.
    FetchContent_Declare (clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
        GIT_SUBMODULES_RECURSE TRUE)
    FetchContent_MakeAvailable (clap-juce-extensions)
endif()

if (CRAFT_BUILD_TESTS)
    FetchContent_Declare (Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.7.1.zip
        URL_HASH SHA256=7d771897398704ecb61eae534912e50c4d3ec6129c4d01c174a55c29657970d7
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable (Catch2)
    list (APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

    # Registered at the top level so a root `ctest` runs the tests of every plug-in.
    enable_testing()
endif()

# ---------------------------------------------------------------------------------------------
# Plug-ins: every plugins/<slug>/ that has a CMakeLists.txt, in sorted order.
file (GLOB CRAFT_PLUGIN_LISTS CONFIGURE_DEPENDS LIST_DIRECTORIES false
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/*/CMakeLists.txt)
list (SORT CRAFT_PLUGIN_LISTS)
foreach (pluginList IN LISTS CRAFT_PLUGIN_LISTS)
    get_filename_component (pluginDir ${pluginList} DIRECTORY)
    get_filename_component (pluginSlug ${pluginDir} NAME)
    message (STATUS "Craft Audio: adding plug-in '${pluginSlug}'")
    add_subdirectory (${pluginDir} plugins/${pluginSlug})
endforeach()
