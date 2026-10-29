add_executable (motion_filter_tests
    ParameterTests.cpp
    FilterTests.cpp
    ModulationTests.cpp
    ChainTests.cpp)
target_link_libraries (motion_filter_tests PRIVATE motion_filter_dsp Catch2::Catch2WithMain)
target_compile_features (motion_filter_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (motion_filter_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (motion_filter_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (motion_filter_tests TEST_PREFIX "motion-filter/")
