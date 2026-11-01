add_executable (opto_compressor_tests OptoTests.cpp)
target_link_libraries (opto_compressor_tests PRIVATE opto_compressor_dsp Catch2::Catch2WithMain)
target_compile_features (opto_compressor_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (opto_compressor_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (opto_compressor_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (opto_compressor_tests TEST_PREFIX "opto-compressor/" DISCOVERY_TIMEOUT 60)
