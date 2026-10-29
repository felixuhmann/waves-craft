Include (FetchContent)

FetchContent_Declare (Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.7.1.zip
    URL_HASH SHA256=7d771897398704ecb61eae534912e50c4d3ec6129c4d01c174a55c29657970d7
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable (Catch2)

add_executable (craft_dsp_tests
    MatrixTests.cpp
    ShufflerTests.cpp
    BehaviourTests.cpp)
target_link_libraries (craft_dsp_tests PRIVATE craft_dsp Catch2::Catch2WithMain)
target_compile_features (craft_dsp_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (craft_dsp_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (craft_dsp_tests PRIVATE -Wall -Wextra)
endif()

list (APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include (CTest)
include (Catch)
catch_discover_tests (craft_dsp_tests)
