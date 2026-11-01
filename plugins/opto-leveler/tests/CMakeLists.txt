add_executable (opto_leveler_tests
    TablesTests.cpp
    StaticTests.cpp
    DynamicsTests.cpp
    DetectorTests.cpp
    OutputTests.cpp)
target_include_directories (opto_leveler_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries (opto_leveler_tests PRIVATE opto_leveler_dsp Catch2::Catch2WithMain)
target_compile_features (opto_leveler_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (opto_leveler_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (opto_leveler_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (opto_leveler_tests TEST_PREFIX "opto-leveler/")
