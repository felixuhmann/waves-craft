add_executable (h_delay_tests
    ParameterTests.cpp
    CoreTests.cpp
    FilterTests.cpp
    ModulationTests.cpp
    AnalogTests.cpp)
target_link_libraries (h_delay_tests PRIVATE h_delay_dsp Catch2::Catch2WithMain)
target_compile_features (h_delay_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (h_delay_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (h_delay_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (h_delay_tests TEST_PREFIX "h-delay/")
