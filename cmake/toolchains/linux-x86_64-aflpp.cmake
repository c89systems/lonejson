# Native AFL++ fuzzing is a GCC-plugin mode of the pinned Bootlin x86_64 GNU
# collection. Resolve the complete Bootlin route before CMake enables a
# language, then substitute only AFL++'s compiler wrapper.
include("${CMAKE_CURRENT_LIST_DIR}/lonejson_bootlin.cmake")
lonejson_configure_bootlin_toolchain(
  x86_64-linux-gnu x86_64 x86_64 "" qemu-x86_64)

get_filename_component(_lonejson_source_dir "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(_lonejson_afl_resolver "${CPKT_LIFECYCLE_SCRIPTS_DIR}/cpkt-aflpp.sh")
execute_process(
  COMMAND "${_lonejson_afl_resolver}" discover
  RESULT_VARIABLE _lonejson_afl_result
  OUTPUT_VARIABLE _lonejson_afl_description
  ERROR_VARIABLE _lonejson_afl_error
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE)
if(NOT _lonejson_afl_result EQUAL 0)
  message(FATAL_ERROR "Pinned AFL++ is unavailable. Run `make toolchains-aflpp`.\n${_lonejson_afl_error}")
endif()
foreach(_lonejson_key root version cc cxx afl_fuzz afl_showmap helper)
  string(REGEX MATCH "(^|[\r\n])${_lonejson_key}=([^\r\n]+)" _lonejson_match "${_lonejson_afl_description}")
  if(NOT _lonejson_match)
    message(FATAL_ERROR "AFL++ resolver did not report ${_lonejson_key}")
  endif()
  set(_lonejson_afl_${_lonejson_key} "${CMAKE_MATCH_2}")
endforeach()
set(CMAKE_C_COMPILER "${_lonejson_afl_cc}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_lonejson_afl_cxx}" CACHE FILEPATH "" FORCE)
set(LONEJSON_AFLPP_ROOT "${_lonejson_afl_root}" CACHE PATH "" FORCE)
set(LONEJSON_AFLPP_VERSION "${_lonejson_afl_version}" CACHE STRING "" FORCE)
set(LONEJSON_AFL_FUZZ "${_lonejson_afl_afl_fuzz}" CACHE FILEPATH "" FORCE)
set(LONEJSON_AFL_SHOWMAP "${_lonejson_afl_afl_showmap}" CACHE FILEPATH "" FORCE)
set(ENV{AFL_PATH} "${_lonejson_afl_helper}")
set(ENV{AFL_CC} "")
set(ENV{AFL_CXX} "")
