add_executable (dynamic_distortion_tests
    ParameterTests.cpp
    FilterTests.cpp
    DynamicsTests.cpp
    ToneTableTests.cpp
    EngineTests.cpp)
target_link_libraries (dynamic_distortion_tests PRIVATE dynamic_distortion_dsp Catch2::Catch2WithMain)
target_compile_features (dynamic_distortion_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (dynamic_distortion_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (dynamic_distortion_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (dynamic_distortion_tests TEST_PREFIX "dynamic-distortion/")
