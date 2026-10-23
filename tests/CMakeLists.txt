include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES "*.cpp")
add_executable(mlms_tests ${TEST_SOURCES})
target_link_libraries(mlms_tests PRIVATE mlms_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mlms_tests)