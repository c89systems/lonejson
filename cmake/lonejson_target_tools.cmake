include_guard(GLOBAL)

# Adapt the single shell discovery surface for CMake packaging consumers.
# Use the cache from the build that produced the artifact, never a host build.
function(lonejson_discover_target_tools)
  if(NOT EXISTS "${LONEJSON_BINARY_DIR}/CMakeCache.txt")
    message(FATAL_ERROR "Target-tool discovery requires the producing build's CMakeCache.txt")
  endif()
  find_program(_bash NAMES bash REQUIRED)
  execute_process(
    COMMAND "${_bash}" -c [=[
set -euo pipefail
resolved_environment=$("$1" --build-dir "$2" --target-id "$3") || exit $?
eval "$resolved_environment"
printf '%s\n' "cc=$CC" "ld=$LINKER" "ar=$AR" "strip=$STRIP" "nm=$NM" "otool=$OTOOL"
]=] target-tools "${LONEJSON_ROOT}/scripts/discover_target_tools.sh"
      "${LONEJSON_BINARY_DIR}" "${LONEJSON_TARGET_ID}"
    RESULT_VARIABLE _status OUTPUT_VARIABLE _description ERROR_VARIABLE _error)
  if(NOT _status EQUAL 0)
    message(FATAL_ERROR "Target-tool discovery failed: ${_error}")
  endif()
  foreach(_pair cc:CMAKE_C_COMPILER ld:CMAKE_LINKER ar:CMAKE_AR
      strip:LONEJSON_STRIP nm:CMAKE_NM otool:CMAKE_OTOOL)
    string(REPLACE ":" ";" _parts "${_pair}")
    list(GET _parts 0 _key)
    list(GET _parts 1 _variable)
    string(REGEX MATCH "(^|\n)${_key}=([^\r\n]*)" _match "${_description}")
    if(NOT _match)
      message(FATAL_ERROR "Target-tool discovery did not report ${_key}")
    endif()
    set(${_variable} "${CMAKE_MATCH_2}" PARENT_SCOPE)
  endforeach()
endfunction()
