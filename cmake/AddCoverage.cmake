function(AddCoverage target)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)

    # Check if gcov wrapper exists and is executable
    set(WRAPPER_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/gcov-llvm-wrapper.sh")
    if(EXISTS "${WRAPPER_SCRIPT}")
        set(WRAPPER_EXISTS TRUE)
    else()
        set(WRAPPER_EXISTS FALSE)
    endif()

    if(LCOV_PATH AND GENHTML_PATH AND WRAPPER_EXISTS)
        # Determine which gcov tool to use based on compiler
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(GCOV_TOOL "${WRAPPER_SCRIPT}")
        else()
            # For GCC, use gcov so .gcda from GCC is read correctly (llvm-cov can misread GCC format)
            find_program(GCOV_PATH gcov)
            if(GCOV_PATH)
                set(GCOV_TOOL "${GCOV_PATH}")
            else()
                find_program(LLVM_COV_PATH llvm-cov)
                if(LLVM_COV_PATH)
                    set(GCOV_TOOL "${WRAPPER_SCRIPT}")
                else()
                    set(GCOV_TOOL "${WRAPPER_SCRIPT}")
                endif()
            endif()
        endif()

        add_custom_target(coverage-${target}
            COMMENT "Running coverage for ${target}..."
            COMMAND ${LCOV_PATH} --gcov-tool ${GCOV_TOOL} -d . --zerocounters
            COMMAND $<TARGET_FILE:${target}>
            COMMAND ${LCOV_PATH} --gcov-tool ${GCOV_TOOL} -d . --capture -o coverage.info
            COMMAND ${LCOV_PATH} --gcov-tool ${GCOV_TOOL} -r coverage.info '/usr/include/*' -o filtered-${target}.info
            COMMAND ${GENHTML_PATH} -o coverage-${target} filtered-${target}.info --legend
            COMMAND rm -rf coverage.info
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        message(STATUS "Coverage enabled for ${target}: lcov=${LCOV_PATH}, genhtml=${GENHTML_PATH}, gcov-tool=${GCOV_TOOL}")
    else()
        if(NOT LCOV_PATH)
            message(STATUS "lcov not found, skipping coverage for ${target}")
        elseif(NOT GENHTML_PATH)
            message(STATUS "genhtml not found (usually comes with lcov package), skipping coverage for ${target}")
        elseif(NOT WRAPPER_EXISTS)
            message(STATUS "gcov wrapper script not found at ${WRAPPER_SCRIPT}, skipping coverage for ${target}")
        endif()
    endif()
endfunction()
