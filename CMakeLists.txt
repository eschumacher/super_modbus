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
    src/rtu/rtu_slave.cpp
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

# target_include_directories(${PROJECT_NAME} PRIVATE
# )

###################################
# Testing and Coverage
###################################
option(BUILD_TESTS "Build tests" ON)
if (BUILD_TESTS)
    if (NOT CMAKE_BUILD_TYPE STREQUAL Debug)
        message(WARNING "Must set CMAKE_BUILD_TYPE to Debug in order to build tests! Tests will not be built.")
    else()
        enable_testing()
        add_subdirectory(test)

        target_compile_options(${PROJECT_NAME}-lib PRIVATE --coverage -O0)
        target_link_libraries(${PROJECT_NAME}-lib PRIVATE --coverage)
    endif()
endif()

###############################
# CPack
###############################
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

include(CPack)
