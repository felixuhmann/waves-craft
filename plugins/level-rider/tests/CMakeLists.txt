add_executable (level_rider_tests
    ParameterTests.cpp
    StaticLawTests.cpp
    DetectorTests.cpp
    DynamicsTests.cpp
    GateTests.cpp
    EngineTests.cpp)
target_link_libraries (level_rider_tests PRIVATE level_rider_dsp Catch2::Catch2WithMain)
target_compile_features (level_rider_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (level_rider_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (level_rider_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (level_rider_tests TEST_PREFIX "level-rider/")
