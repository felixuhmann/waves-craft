add_executable (stereo_imager_tests
    MatrixTests.cpp
    ShufflerTests.cpp
    BehaviourTests.cpp)
target_link_libraries (stereo_imager_tests PRIVATE stereo_imager_dsp Catch2::Catch2WithMain)
target_compile_features (stereo_imager_tests PRIVATE cxx_std_17)
if (MSVC)
    target_compile_options (stereo_imager_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options (stereo_imager_tests PRIVATE -Wall -Wextra)
endif()

# Catch2 is fetched by the root CMakeLists.txt, which also calls enable_testing().
include (Catch)
catch_discover_tests (stereo_imager_tests TEST_PREFIX "stereo-imager/")
