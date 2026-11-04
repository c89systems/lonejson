# Native Darwin loads LuaRocks modules rather than the Linux embedding runner.
# CTest fixtures prepare that installed SDK/rock even for a direct test selection.
# The ordinary Lua interpreter cannot initialize a sanitizer runtime loaded later
# through the module; sanitizer coverage uses the compiled C tests instead.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND LONEJSON_CAN_RUN_LUA_NATIVE_TESTS
    AND NOT LONEJSON_ENABLE_ASAN AND NOT LONEJSON_ENABLE_TSAN)
  find_program(LONEJSON_MAKE_EXECUTABLE NAMES gmake make REQUIRED)
  add_test(NAME lonejson_lua_native_prepare COMMAND "${LONEJSON_MAKE_EXECUTABLE}"
    -C "${CMAKE_CURRENT_SOURCE_DIR}" lua-rock
    "LUA=${LONEJSON_LUA_EXECUTABLE}" "LUAROCKS=${LONEJSON_LUAROCKS_EXECUTABLE}"
    "LONEJSON_LUA_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(lonejson_lua_native_prepare PROPERTIES
    FIXTURES_SETUP lonejson_native_lua LABELS local TIMEOUT 300)
  add_test(NAME lonejson_lua_native_tests COMMAND bash
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_lua_native.sh"
    "${LONEJSON_LUA_EXECUTABLE}" "${LONEJSON_LUAROCKS_EXECUTABLE}"
    tests/test_lua.lua tests/test_lua_fuzz.lua)
  add_test(NAME lonejson_lua_native_fuzz_tests COMMAND bash
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_lua_native.sh"
    "${LONEJSON_LUA_EXECUTABLE}" "${LONEJSON_LUAROCKS_EXECUTABLE}"
    tests/test_lua_fuzz.lua)
  foreach(case schema_cache encode_stats)
    add_test(NAME lonejson_lua_${case}_tests COMMAND bash
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_${case}.sh"
      "${CMAKE_CURRENT_SOURCE_DIR}" "${LONEJSON_LUA_EXECUTABLE}"
      "${LONEJSON_LUAROCKS_EXECUTABLE}")
  endforeach()
  foreach(test lonejson_lua_native_tests lonejson_lua_native_fuzz_tests
      lonejson_lua_schema_cache_tests lonejson_lua_encode_stats_tests
      lonejson_bench_baseline_history_tests lonejson_bench_retry_confirm_tests)
    if(TEST "${test}")
      set_tests_properties("${test}" PROPERTIES FIXTURES_REQUIRED lonejson_native_lua)
    endif()
  endforeach()
endif()
