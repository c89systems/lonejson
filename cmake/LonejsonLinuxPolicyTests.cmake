# These fixtures execute Linux-hosted Bootlin tools or assert Linux loader/SDK
# behavior. Portable policy tests remain in the main registration on every host.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND
    CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
  foreach(case compiler_selection bootlin_host_arch native_bootlin_target_command
      cpkt_toolchain_resolver cpkt_aflpp_resolver discover_target_tools
      cmake_cpkt_root bootlin_tsan_support cross_sanitizer_matrix
      cmake_fuzz_auth_optional release_archive_verify)
    add_test(NAME lonejson_${case}_tests COMMAND "${LONEJSON_BASH_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${case}.sh" "${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(lonejson_${case}_tests PROPERTIES
      LABELS "local;host-policy" TIMEOUT 120)
  endforeach()
  add_test(NAME lonejson_native_bootlin_target_tests COMMAND "${CMAKE_COMMAND}"
    "-DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_native_bootlin_target.cmake")
  add_test(NAME lonejson_cmake_fuzz_sanitizer_conflict_tests COMMAND "${LONEJSON_BASH_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cmake_fuzz_sanitizer_conflicts.sh" "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME lonejson_lua_native_test_target_filter_tests COMMAND "${LONEJSON_BASH_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_native_test_target_filter.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_COMMAND}" "${CMAKE_GENERATOR}")
  set_tests_properties(lonejson_native_bootlin_target_tests
    lonejson_cmake_fuzz_sanitizer_conflict_tests lonejson_lua_native_test_target_filter_tests
    PROPERTIES LABELS "local;host-policy" TIMEOUT 120)
endif()
