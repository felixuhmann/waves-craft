add_executable (dynamics_tests
    StaticCurveTests.cpp
    DetectorTests.cpp
    GateTests.cpp
    FilterTests.cpp
    RoutingTests.cpp
    ParameterTests.cpp)
target_link_libraries (dynamics_tests PRIVATE dynamics_dsp Catch2::Catch2WithMain)
target_compile_features (dynamics_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (dynamics_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (dynamics_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (dynamics_tests TEST_PREFIX "dynamics/")
