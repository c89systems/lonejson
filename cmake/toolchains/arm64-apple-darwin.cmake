include("${CMAKE_CURRENT_LIST_DIR}/../LonejsonLifecycle.cmake")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  include("${CMAKE_CURRENT_LIST_DIR}/lonejson_native_darwin.cmake")
  return()
endif()

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(DEFINED ENV{OSXCROSS_ROOT} AND NOT "$ENV{OSXCROSS_ROOT}" STREQUAL "")
  set(LONEJSON_OSXCROSS_ROOT "$ENV{OSXCROSS_ROOT}")
elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
  set(LONEJSON_OSXCROSS_ROOT "$ENV{HOME}/.local/cross/osxcross")
else()
  message(FATAL_ERROR "OSXCROSS_ROOT is not set and HOME is unavailable")
endif()

set(LONEJSON_OSXCROSS_HOST "" CACHE STRING "Optional exact osxcross target prefix")
if(LONEJSON_OSXCROSS_HOST)
  set(ENV{CPKT_OSXCROSS_HOST} "${LONEJSON_OSXCROSS_HOST}")
endif()
execute_process(
  COMMAND "${CPKT_LIFECYCLE_SCRIPTS_DIR}/cpkt-toolchains.sh" discover arm64-apple-darwin
  RESULT_VARIABLE _darwin_status OUTPUT_VARIABLE _darwin_description ERROR_VARIABLE _darwin_error)
if(NOT _darwin_status EQUAL 0 OR NOT _darwin_description MATCHES "(^|\n)status=ready(\n|$)")
  message(FATAL_ERROR "Darwin lifecycle toolchain unavailable; run scripts/cpkt-toolchains.sh ensure arm64-apple-darwin before configuring: ${_darwin_error}\n${_darwin_description}")
endif()
foreach(_pair root:LONEJSON_OSXCROSS_ROOT prefix:LONEJSON_OSXCROSS_HOST
    cc:CMAKE_C_COMPILER cxx:CMAKE_CXX_COMPILER ld:CMAKE_LINKER ar:CMAKE_AR
    ranlib:CMAKE_RANLIB strip:CMAKE_STRIP nm:CMAKE_NM otool:CMAKE_OTOOL)
  string(REPLACE ":" ";" _parts "${_pair}")
  list(GET _parts 0 _key)
  list(GET _parts 1 _variable)
  string(REGEX MATCH "(^|\n)${_key}=([^\r\n]+)" _match "${_darwin_description}")
  if(NOT _match)
    message(FATAL_ERROR "Darwin lifecycle resolver did not report ${_key}")
  endif()
  set(${_variable} "${CMAKE_MATCH_2}" CACHE STRING "" FORCE)
endforeach()
set(LONEJSON_OSXCROSS_BIN_DIR "${LONEJSON_OSXCROSS_ROOT}/bin")
set(ENV{PATH} "${LONEJSON_OSXCROSS_BIN_DIR}:$ENV{PATH}")
set(CMAKE_INSTALL_NAME_TOOL "${LONEJSON_OSXCROSS_BIN_DIR}/${LONEJSON_OSXCROSS_HOST}-install_name_tool" CACHE FILEPATH "" FORCE)
if(NOT EXISTS "${CMAKE_INSTALL_NAME_TOOL}")
  message(FATAL_ERROR "Missing selected osxcross install_name_tool: ${CMAKE_INSTALL_NAME_TOOL}")
endif()
set(LONEJSON_MACOS_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum macOS deployment target")
set(CMAKE_OSX_DEPLOYMENT_TARGET "${LONEJSON_MACOS_DEPLOYMENT_TARGET}" CACHE STRING "" FORCE)

set(_lonejson_darwin_linker_flag "--ld-path=${CMAKE_LINKER}")
string(CONCAT _lonejson_legacy_darwin_linker_regex "(^| )-fuse-ld=[^ ]+")
foreach(_lonejson_linker_flags
        CMAKE_EXE_LINKER_FLAGS
        CMAKE_SHARED_LINKER_FLAGS
        CMAKE_MODULE_LINKER_FLAGS)
  set(_lonejson_current_linker_flags "${${_lonejson_linker_flags}}")
  string(REGEX REPLACE "${_lonejson_legacy_darwin_linker_regex}" ""
         _lonejson_current_linker_flags "${_lonejson_current_linker_flags}")
  string(STRIP "${_lonejson_current_linker_flags}"
         _lonejson_current_linker_flags)
  if(NOT "${_lonejson_current_linker_flags}" MATCHES "(^| )--ld-path=")
    set(_lonejson_current_linker_flags
        "${_lonejson_darwin_linker_flag} ${_lonejson_current_linker_flags}")
  endif()
  if(NOT "${${_lonejson_linker_flags}}" STREQUAL
      "${_lonejson_current_linker_flags}")
    set(${_lonejson_linker_flags}
        "${_lonejson_current_linker_flags}"
        CACHE STRING "" FORCE)
  endif()
endforeach()

file(GLOB _lonejson_osxcross_sdks LIST_DIRECTORIES true
     "${LONEJSON_OSXCROSS_ROOT}/SDK/MacOSX*.sdk")
if(NOT _lonejson_osxcross_sdks)
  message(FATAL_ERROR
    "failed to locate a usable osxcross macOS SDK under "
    "${LONEJSON_OSXCROSS_ROOT}/SDK")
endif()
list(SORT _lonejson_osxcross_sdks COMPARE NATURAL)
list(REVERSE _lonejson_osxcross_sdks)
list(GET _lonejson_osxcross_sdks 0 LONEJSON_OSXCROSS_SDK)
if(NOT EXISTS "${LONEJSON_OSXCROSS_SDK}/usr/include")
  message(FATAL_ERROR
    "failed to locate a usable osxcross macOS SDK under "
    "${LONEJSON_OSXCROSS_ROOT}/SDK")
endif()

set(CMAKE_OSX_SYSROOT "${LONEJSON_OSXCROSS_SDK}" CACHE PATH "" FORCE)
set(_lonejson_find_root_path "${LONEJSON_OSXCROSS_SDK}")
if(DEFINED LONEJSON_CPKT_ROOT AND
    NOT "${LONEJSON_CPKT_ROOT}" STREQUAL "")
  list(APPEND _lonejson_find_root_path "${LONEJSON_CPKT_ROOT}")
endif()
set(CMAKE_FIND_ROOT_PATH ${_lonejson_find_root_path})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(LONEJSON_TARGET_ARCH arm64 CACHE STRING "" FORCE)
set(LONEJSON_TARGET_OS darwin CACHE STRING "" FORCE)
set(LONEJSON_TARGET_LIBC "" CACHE STRING "" FORCE)
