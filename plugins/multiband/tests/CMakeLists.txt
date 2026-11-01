add_executable (multiband_tests
    CrossoverTests.cpp
    GainCurveTests.cpp
    DynamicsTests.cpp
    EngineTests.cpp
    ParameterTests.cpp)
target_link_libraries (multiband_tests PRIVATE multiband_dsp Catch2::Catch2WithMain)
target_compile_features (multiband_tests PRIVATE cxx_std_17)
target_include_directories (multiband_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if (MSVC)
    target_compile_options (multiband_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (multiband_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (multiband_tests TEST_PREFIX "multiband/")
