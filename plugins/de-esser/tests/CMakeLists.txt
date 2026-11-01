add_executable (de_esser_tests
    FilterTests.cpp
    DynamicsTests.cpp
    BehaviourTests.cpp)
target_link_libraries (de_esser_tests PRIVATE de_esser_dsp Catch2::Catch2WithMain)
target_compile_features (de_esser_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (de_esser_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (de_esser_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (de_esser_tests TEST_PREFIX "de-esser/")
