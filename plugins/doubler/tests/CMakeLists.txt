add_executable (doubler_tests
    ParameterTests.cpp
    PathTests.cpp
    EqTests.cpp
    ShifterTests.cpp
    ModulationTests.cpp)
target_link_libraries (doubler_tests PRIVATE doubler_dsp Catch2::Catch2WithMain)
target_compile_features (doubler_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (doubler_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (doubler_tests PRIVATE -Wall -Wextra)
endif()

# The statistical pitch-shifter tests process several seconds of audio; keep them fast in Debug too.
if (NOT MSVC)
    target_compile_options (doubler_tests PRIVATE $<$<CONFIG:Debug>:-O1>)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (doubler_tests TEST_PREFIX "doubler/")
