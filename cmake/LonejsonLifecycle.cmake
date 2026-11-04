include_guard(GLOBAL)
get_filename_component(_lonejson_lifecycle_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CPKT_LIFECYCLE_SCRIPTS_DIR "${_lonejson_lifecycle_root}/tools/lifecycle/scripts" CACHE PATH
  "Directory containing the complete pinned lifecycle helper collection")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CPKT_LIFECYCLE_SCRIPTS_DIR)
set(_lonejson_toolchain_cache "$ENV{CPKT_TOOLCHAIN_CACHE}")
if(NOT _lonejson_toolchain_cache)
  if(NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
    set(_lonejson_toolchain_cache "$ENV{XDG_CACHE_HOME}/cpkt/toolchains")
  else()
    set(_lonejson_toolchain_cache "$ENV{HOME}/.cache/cpkt/toolchains")
  endif()
endif()
execute_process(COMMAND "${_lonejson_lifecycle_root}/scripts/require_cache_path.sh"
  "${_lonejson_toolchain_cache}" RESULT_VARIABLE _lonejson_cache_status
  ERROR_VARIABLE _lonejson_cache_error)
if(NOT _lonejson_cache_status EQUAL 0)
  message(FATAL_ERROR "${_lonejson_cache_error}")
endif()
