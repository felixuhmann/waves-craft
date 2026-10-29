add_executable (mastering_limiter_tests
    LimiterTests.cpp
    AdaptiveReleaseTests.cpp
    RequantiserTests.cpp
    NoiseShapingTests.cpp
    ParameterTests.cpp)
target_link_libraries (mastering_limiter_tests PRIVATE mastering_limiter_dsp Catch2::Catch2WithMain)
target_compile_features (mastering_limiter_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (mastering_limiter_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (mastering_limiter_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (mastering_limiter_tests TEST_PREFIX "mastering-limiter/")
