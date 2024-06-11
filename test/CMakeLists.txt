include(FetchContent)

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG v1.14.0
)
# Recommended by GoogleTest docs to support Windows environments
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(run_tests
    gtest_test.cpp
)

target_link_libraries(run_tests PRIVATE
  gtest_main
  ${PROJECT_NAME}-lib
)

include(GoogleTest)
gtest_discover_tests(run_tests)

target_compile_options(run_tests PRIVATE --coverage -O0)
target_link_libraries(run_tests PRIVATE --coverage)
add_custom_target(TARGET ${PROJECT_NAME}-lib PRE_BUILD COMMAND find ${CMAKE_BINARY_DIR} -type f -name '*.gcda' -exec rm {} +)

option(COVERAGE "Enable coverage reporting" ON)
if (COVERAGE)
    include(AddCoverage)
    AddCoverage(${PROJECT_NAME}-lib)
    AddCoverage(run_tests)
endif()

option(PROFILING "Enable profiling (valgrind, memcheck)" ON)
if (PROFILING)
    include(AddMemCheck)
    AddMemCheck(run_tests)

    include(AddCppCheck)
    AddCppCheck(run_tests)
endif()
