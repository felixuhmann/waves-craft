add_executable (enhancer_tests
    ParameterTests.cpp
    CrossoverTests.cpp
    PathTests.cpp
    DynamicsTests.cpp)
target_link_libraries (enhancer_tests PRIVATE enhancer_dsp Catch2::Catch2WithMain)
target_compile_features (enhancer_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (enhancer_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (enhancer_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (enhancer_tests TEST_PREFIX "enhancer/")
