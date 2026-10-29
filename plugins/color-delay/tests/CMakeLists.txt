add_executable (color_delay_tests
    ParameterTests.cpp
    PathTests.cpp
    CompressorTests.cpp
    ColourTests.cpp)
target_link_libraries (color_delay_tests PRIVATE color_delay_dsp Catch2::Catch2WithMain)
target_compile_features (color_delay_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (color_delay_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (color_delay_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (color_delay_tests TEST_PREFIX "color-delay/")
