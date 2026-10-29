add_executable (flanger_tests
    TestVectorTests.cpp
    ModulationTests.cpp
    ControlTests.cpp)
target_link_libraries (flanger_tests PRIVATE flanger_dsp Catch2::Catch2WithMain)
target_compile_features (flanger_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (flanger_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (flanger_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (flanger_tests TEST_PREFIX "flanger/")
