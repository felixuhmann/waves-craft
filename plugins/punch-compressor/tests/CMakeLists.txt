add_executable (punch_compressor_tests
    ParameterTests.cpp
    StaticCurveTests.cpp
    DynamicsTests.cpp
    AnalogTests.cpp
    EngineTests.cpp)
target_link_libraries (punch_compressor_tests PRIVATE punch_compressor_dsp Catch2::Catch2WithMain)
target_compile_features (punch_compressor_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (punch_compressor_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (punch_compressor_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (punch_compressor_tests TEST_PREFIX "punch-compressor/")
