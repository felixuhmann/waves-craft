add_executable (console_g_tests
    FilterEqTests.cpp
    DynamicsTests.cpp
    StripTests.cpp)
target_link_libraries (console_g_tests PRIVATE console_g_dsp Catch2::Catch2WithMain)
target_compile_features (console_g_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (console_g_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (console_g_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (console_g_tests TEST_PREFIX "console-g/" DISCOVERY_TIMEOUT 60)
