add_executable (band_limiter_tests
    SplitterTests.cpp
    LimiterTests.cpp
    ReleaseTests.cpp
    RequantiserTests.cpp)
target_link_libraries (band_limiter_tests PRIVATE band_limiter_dsp Catch2::Catch2WithMain)
target_compile_features (band_limiter_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (band_limiter_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (band_limiter_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (band_limiter_tests TEST_PREFIX "band-limiter/")
