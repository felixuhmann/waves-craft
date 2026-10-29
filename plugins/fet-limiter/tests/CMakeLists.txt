add_executable (fet_limiter_tests
    GainStagingTests.cpp
    StaticLawTests.cpp
    DynamicsTests.cpp
    ColourationTests.cpp
    EngineTests.cpp
    ParameterTests.cpp)
target_link_libraries (fet_limiter_tests PRIVATE fet_limiter_dsp Catch2::Catch2WithMain)
target_compile_features (fet_limiter_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (fet_limiter_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (fet_limiter_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (fet_limiter_tests TEST_PREFIX "fet-limiter/")
