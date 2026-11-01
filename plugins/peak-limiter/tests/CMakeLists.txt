add_executable (peak_limiter_tests
    PrimitiveTests.cpp
    LimiterTests.cpp
    AutoReleaseTests.cpp
    ParameterTests.cpp)
target_link_libraries (peak_limiter_tests PRIVATE peak_limiter_dsp Catch2::Catch2WithMain)
target_compile_features (peak_limiter_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (peak_limiter_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (peak_limiter_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (peak_limiter_tests TEST_PREFIX "peak-limiter/")
