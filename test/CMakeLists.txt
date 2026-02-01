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
    test_gtest.cpp
    rtu/test_rtu_slave.cpp
    rtu/test_rtu_master.cpp
    rtu/test_new_function_codes.cpp
    rtu/test_remaining_function_codes.cpp
    rtu/test_error_handling.cpp
    rtu/test_edge_cases.cpp
    rtu/test_integration.cpp
    rtu/test_master_integration.cpp
    rtu/test_master_errors.cpp
    rtu/test_broadcast.cpp
    rtu/test_fifo_queue.cpp
    rtu/test_event_log.cpp
    rtu/test_rtu_frame_edge_cases.cpp
    rtu/test_rtu_request_edge_cases.cpp
    rtu/test_rtu_master_coverage.cpp
    rtu/test_rtu_slave_coverage.cpp
    tcp/test_tcp_frame.cpp
    tcp/test_tcp_master.cpp
    tcp/test_tcp_slave.cpp
    tcp/test_tcp_frame_edge_cases.cpp
    tcp/test_tcp_request_edge_cases.cpp
    tcp/test_tcp_master_coverage.cpp
    tcp/test_tcp_slave_coverage.cpp
    common/test_common_utilities.cpp
    common/test_crc16.cpp
    transport/test_memory_transport.cpp
)

target_link_libraries(run_tests PRIVATE
  gtest_main
  ${PROJECT_NAME}-lib
)

include(GoogleTest)
gtest_discover_tests(run_tests)

option(COVERAGE "Enable coverage reporting" OFF)
if (COVERAGE)
    # Only add coverage flags when coverage is explicitly enabled
    target_compile_options(run_tests PRIVATE --coverage -O0)
    target_link_libraries(run_tests PRIVATE --coverage)
    add_custom_target(coverage-clean ${PROJECT_NAME}-lib PRE_BUILD COMMAND find ${CMAKE_BINARY_DIR} -type f -name '*.gcda' -exec rm {} +)

    include(AddCoverage)
    AddCoverage(${PROJECT_NAME}-lib)
    AddCoverage(run_tests)
endif()

option(PROFILING "Enable profiling (valgrind, memcheck)" OFF)
if (PROFILING)
    include(AddMemCheck)
    AddMemCheck(run_tests)

    include(AddCppCheck)
    AddCppCheck(run_tests)
endif()
