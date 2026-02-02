cmake_minimum_required(VERSION 3.15.0)
project(super-modbus VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(CTest)

include(FetchContent)

###############################
# super-modbus-lib
###############################
add_library(${PROJECT_NAME}-lib
    STATIC
    src/super_modbus.cpp
    src/rtu/rtu_request.cpp
    src/rtu/rtu_slave.cpp
    src/rtu/rtu_frame.cpp
    src/rtu/rtu_master.cpp
    src/tcp/tcp_request.cpp
    src/tcp/tcp_slave.cpp
    src/tcp/tcp_frame.cpp
    src/tcp/tcp_master.cpp
)

# target_link_libraries(${PROJECT_NAME}-lib PRIVATE
# )

target_include_directories(${PROJECT_NAME}-lib
    INTERFACE
    include/
)

target_include_directories(${PROJECT_NAME}-lib
    PRIVATE
    include/super_modbus/
)

###################################
# super-modbus executable
###################################
add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${PROJECT_NAME}-lib
)

###################################
# Examples
###################################
option(BUILD_EXAMPLES "Build example programs" ON)
if (BUILD_EXAMPLES)
    add_executable(example_master examples/example_master.cpp)
    target_link_libraries(example_master PRIVATE ${PROJECT_NAME}-lib)

    add_executable(example_slave examples/example_slave.cpp)
    target_link_libraries(example_slave PRIVATE ${PROJECT_NAME}-lib)

    add_executable(example_loopback examples/example_master_slave_loopback.cpp)
    target_link_libraries(example_loopback PRIVATE ${PROJECT_NAME}-lib)

    add_executable(testable_slave examples/testable_slave.cpp)
    target_link_libraries(testable_slave PRIVATE ${PROJECT_NAME}-lib)

    add_executable(example_virtual_port_test examples/example_virtual_port_test.cpp)
    target_link_libraries(example_virtual_port_test PRIVATE ${PROJECT_NAME}-lib)
    target_include_directories(example_virtual_port_test PRIVATE examples/)

    add_executable(test_master_comprehensive examples/test_master_comprehensive.cpp)
    target_link_libraries(test_master_comprehensive PRIVATE ${PROJECT_NAME}-lib)
    target_include_directories(test_master_comprehensive PRIVATE examples/)

    # Note: example_serial_transport.cpp is a template, not meant to be compiled
    # Note: serial_transport.hpp is a header-only implementation for use in examples
endif()

###################################
# Testing and Coverage
###################################
option(BUILD_TESTS "Build tests" ON)
option(COVERAGE "Enable coverage reporting" OFF)
if (BUILD_TESTS)
    if (NOT CMAKE_BUILD_TYPE STREQUAL Debug)
        message(WARNING "Must set CMAKE_BUILD_TYPE to Debug in order to build tests! Tests will not be built.")
    else()
        enable_testing()
        add_subdirectory(test)

        # Only add coverage flags when coverage is explicitly enabled
        if (COVERAGE)
            target_compile_options(${PROJECT_NAME}-lib PRIVATE --coverage -O0)
            target_link_libraries(${PROJECT_NAME}-lib PRIVATE --coverage)
        endif()
    endif()
endif()

###############################
# CPack
###############################
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

include(CPack)
