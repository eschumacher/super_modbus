function(AddCppCheck target)
  find_program(CPPCHECK_PATH cppcheck)
  if(CPPCHECK_PATH)
    set_target_properties(${target}
      PROPERTIES CXX_CPPCHECK
      "${CPPCHECK_PATH};--enable=warning;--error-exitcode=10;--library=googletest;--suppress=uninitvar"
    )
    message(STATUS "cppcheck found: ${CPPCHECK_PATH}")
  else()
    message(STATUS "cppcheck not found, skipping static analysis")
  endif()
endfunction()
