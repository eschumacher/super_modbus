function(AddCoverage target)
    find_program(LCOV_PATH lcov REQUIRED)
    find_program(GENHTML_PATH genhtml REQUIRED)
    add_custom_target(coverage-${target}
        COMMENT "Running coverage for ${target}..."
        COMMAND ${LCOV_PATH} --gcov-tool ${CMAKE_SOURCE_DIR}/cmake/gcov-llvm-wrapper.sh -d . --zerocounters
        COMMAND $<TARGET_FILE:${target}>
        COMMAND ${LCOV_PATH} --gcov-tool ${CMAKE_SOURCE_DIR}/cmake/gcov-llvm-wrapper.sh -d . --capture -o coverage.info
        COMMAND ${LCOV_PATH} --gcov-tool ${CMAKE_SOURCE_DIR}/cmake/gcov-llvm-wrapper.sh -r coverage.info '/usr/include/*' -o filtered-${target}.info
        COMMAND ${GENHTML_PATH} -o coverage-${target} filtered-${target}.info --legend
        COMMAND rm -rf coverage.info
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endfunction()
