add_executable (console_e_tests
    EqTests.cpp
    DynamicsTests.cpp
    EngineTests.cpp
    ParameterTests.cpp)
target_link_libraries (console_e_tests PRIVATE console_e_dsp Catch2::Catch2WithMain)
target_compile_features (console_e_tests PRIVATE cxx_std_17)
# The normative tables are read straight from the approved spec.
target_compile_definitions (console_e_tests PRIVATE
    CONSOLE_E_APPENDIX="${CMAKE_CURRENT_SOURCE_DIR}/../../../spec/console-e/r1/APPENDIX.md")
if (MSVC)
    target_compile_options (console_e_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (console_e_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (console_e_tests TEST_PREFIX "console-e/")
