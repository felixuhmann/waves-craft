add_executable (vocal_compressor_tests
    PrimitiveTests.cpp
    StaticTests.cpp
    GateTests.cpp
    ReleaseTests.cpp
    ParameterTests.cpp)
target_link_libraries (vocal_compressor_tests PRIVATE vocal_compressor_dsp Catch2::Catch2WithMain)
target_compile_features (vocal_compressor_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (vocal_compressor_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (vocal_compressor_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (vocal_compressor_tests TEST_PREFIX "vocal-compressor/")
