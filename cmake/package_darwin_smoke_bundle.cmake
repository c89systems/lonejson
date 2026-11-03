if(NOT DEFINED LONEJSON_BINARY_DIR OR LONEJSON_BINARY_DIR STREQUAL "")
  message(FATAL_ERROR "LONEJSON_BINARY_DIR is required")
endif()
if(NOT DEFINED LONEJSON_ROOT OR LONEJSON_ROOT STREQUAL "")
  message(FATAL_ERROR "LONEJSON_ROOT is required")
endif()
if(NOT DEFINED LONEJSON_VERSION OR LONEJSON_VERSION STREQUAL "")
  message(FATAL_ERROR "LONEJSON_VERSION is required")
endif()
if(NOT DEFINED LONEJSON_TARGET_ID OR LONEJSON_TARGET_ID STREQUAL "")
  message(FATAL_ERROR "LONEJSON_TARGET_ID is required")
endif()

get_filename_component(LONEJSON_BINARY_DIR "${LONEJSON_BINARY_DIR}" ABSOLUTE)
get_filename_component(LONEJSON_ROOT "${LONEJSON_ROOT}" ABSOLUTE)
include("${LONEJSON_ROOT}/cmake/lonejson_build_workspace.cmake")
lonejson_require_build_workspace("${LONEJSON_BINARY_DIR}/darwin-smoke-bundle")
include("${LONEJSON_ROOT}/cmake/lonejson_dist_dir.cmake")
lonejson_prepare_dist_dir()

function(lonejson_import_cache_path var_name)
  if(DEFINED ${var_name} AND NOT "${${var_name}}" STREQUAL "")
    return()
  endif()
  file(STRINGS "${LONEJSON_BINARY_DIR}/CMakeCache.txt" cache_line
       REGEX "^${var_name}(:[^=]+)?=" LIMIT_COUNT 1)
  if(cache_line)
    string(REGEX REPLACE "^[^=]*=" "" cache_value "${cache_line}")
    set(${var_name} "${cache_value}" PARENT_SCOPE)
  endif()
endfunction()

include("${LONEJSON_ROOT}/cmake/lonejson_target_tools.cmake")
lonejson_discover_target_tools()
lonejson_import_cache_path(CMAKE_BUILD_TYPE)
lonejson_import_cache_path(LONEJSON_ABI_VERSION)
lonejson_import_cache_path(LONEJSON_MACOS_DEPLOYMENT_TARGET)
lonejson_import_cache_path(LONEJSON_OSXCROSS_HOST)

if(NOT LONEJSON_TARGET_ID STREQUAL "arm64-apple-darwin")
  message(FATAL_ERROR
    "Darwin smoke bundle requires arm64-apple-darwin, got ${LONEJSON_TARGET_ID}")
endif()
if(NOT CMAKE_C_COMPILER OR NOT EXISTS "${CMAKE_C_COMPILER}")
  message(FATAL_ERROR "CMAKE_C_COMPILER is required for Darwin smoke bundle")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(NOT LONEJSON_MACOS_DEPLOYMENT_TARGET)
  set(LONEJSON_MACOS_DEPLOYMENT_TARGET "15.0")
endif()
if(NOT LONEJSON_ABI_VERSION)
  message(FATAL_ERROR "LONEJSON_ABI_VERSION is required for Darwin smoke bundle")
endif()

if(NOT CMAKE_LINKER OR NOT EXISTS "${CMAKE_LINKER}")
  message(FATAL_ERROR "Selected target CMAKE_LINKER is required for Darwin smoke bundle")
endif()
get_filename_component(_lonejson_linker_dir "${CMAKE_LINKER}" DIRECTORY)
set(ENV{PATH} "${_lonejson_linker_dir}:$ENV{PATH}")
if(NOT CMAKE_OTOOL OR NOT EXISTS "${CMAKE_OTOOL}")
  message(FATAL_ERROR "Selected target CMAKE_OTOOL is required for Darwin smoke bundle")
endif()
set(bundle_root "${LONEJSON_BINARY_DIR}/darwin-smoke-bundle")
set(extract_root "${bundle_root}/release")
set(consumer_bin_dir "${bundle_root}/consumer-bin")
set(stage_name "liblonejson-${LONEJSON_VERSION}-${LONEJSON_TARGET_ID}-smoke-test")
set(stage_root "${bundle_root}/${stage_name}")
set(release_archive
    "${LONEJSON_DIST_DIR}/liblonejson-${LONEJSON_VERSION}-${LONEJSON_TARGET_ID}.tar.gz")
set(release_prefix
    "${extract_root}/liblonejson-${LONEJSON_VERSION}-${LONEJSON_TARGET_ID}")
set(smoke_archive "${LONEJSON_DIST_DIR}/${stage_name}.zip")

if(NOT EXISTS "${release_archive}")
  message(FATAL_ERROR "missing Darwin release archive: ${release_archive}")
endif()

file(REMOVE_RECURSE "${bundle_root}")
file(MAKE_DIRECTORY
  "${extract_root}"
  "${consumer_bin_dir}"
  "${stage_root}/bin"
  "${stage_root}/lib"
  "${stage_root}/tests/fixtures/array_stream")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar xf "${release_archive}"
  WORKING_DIRECTORY "${extract_root}"
  RESULT_VARIABLE extract_result
  OUTPUT_VARIABLE extract_stdout
  ERROR_VARIABLE extract_stderr)
if(NOT extract_result EQUAL 0)
  message(FATAL_ERROR
    "failed to extract Darwin release archive for smoke bundle\n"
    "stdout:\n${extract_stdout}\n"
    "stderr:\n${extract_stderr}")
endif()

set(common_compile_args
  "${CMAKE_C_COMPILER}"
  -std=c89
  -Wall
  -Wextra
  -Werror
  -Wl,-fatal_warnings
  "-mmacosx-version-min=${LONEJSON_MACOS_DEPLOYMENT_TARGET}"
  "--ld-path=${CMAKE_LINKER}"
  -I "${release_prefix}/include"
  "${LONEJSON_ROOT}/tests/test_link_consumer.c")

execute_process(
  COMMAND ${common_compile_args}
    "${release_prefix}/lib/liblonejson.a"
    -o "${consumer_bin_dir}/lonejson_static_smoke"
  RESULT_VARIABLE static_result
  OUTPUT_VARIABLE static_stdout
  ERROR_VARIABLE static_stderr)
if(NOT static_result EQUAL 0)
  message(FATAL_ERROR
    "failed to build Darwin static smoke binary\n"
    "stdout:\n${static_stdout}\n"
    "stderr:\n${static_stderr}")
endif()

file(GLOB release_dylibs LIST_DIRECTORIES false "${release_prefix}/lib/*.dylib")
if(NOT release_dylibs)
  message(FATAL_ERROR
    "Darwin smoke bundle has no dylibs to package from ${release_prefix}/lib")
endif()
set(release_link_dylib "")
foreach(release_dylib IN LISTS release_dylibs)
  if(NOT IS_SYMLINK "${release_dylib}")
    set(release_link_dylib "${release_dylib}")
  endif()
endforeach()
if(NOT release_link_dylib)
  list(GET release_dylibs 0 release_link_dylib)
endif()

execute_process(
  COMMAND ${common_compile_args}
    "-Wl,-rpath,@executable_path/../lib"
    "${release_link_dylib}"
    -o "${consumer_bin_dir}/lonejson_shared_smoke"
  RESULT_VARIABLE shared_result
  OUTPUT_VARIABLE shared_stdout
  ERROR_VARIABLE shared_stderr)
if(NOT shared_result EQUAL 0)
  message(FATAL_ERROR
    "failed to build Darwin shared smoke binary\n"
    "stdout:\n${shared_stdout}\n"
    "stderr:\n${shared_stderr}")
endif()

file(COPY "${LONEJSON_ROOT}/LICENSE" DESTINATION "${stage_root}")

set(example_names
  array_stream
  fixed_storage
  generator_pull
  json_value_buffer
  json_value_parse
  json_value_source
  json_value_visitor
  parse_file
  parse_reader
  parse_string
  push_parser
  serialize_file
  serialize_jsonl
  serialize_string
  source_bytes
  source_text
  spooled_bytes
  spooled_text)

file(COPY
  "${consumer_bin_dir}/lonejson_static_smoke"
  "${consumer_bin_dir}/lonejson_shared_smoke"
  DESTINATION "${stage_root}/bin")
foreach(example_name IN LISTS example_names)
  set(example_binary "${LONEJSON_BINARY_DIR}/example_${example_name}")
  if(NOT EXISTS "${example_binary}")
    message(FATAL_ERROR "missing Darwin example binary: ${example_binary}")
  endif()
  file(COPY "${example_binary}" DESTINATION "${stage_root}/bin")
endforeach()

foreach(release_dylib IN LISTS release_dylibs)
  file(COPY "${release_dylib}" DESTINATION "${stage_root}/lib")
endforeach()
set(versioned_dylib "${stage_root}/lib/liblonejson.${LONEJSON_VERSION}.dylib")
if(EXISTS "${versioned_dylib}")
  file(COPY_FILE "${versioned_dylib}" "${stage_root}/lib/liblonejson.${LONEJSON_ABI_VERSION}.dylib")
  file(COPY_FILE "${versioned_dylib}" "${stage_root}/lib/liblonejson.dylib")
endif()

file(COPY "${LONEJSON_ROOT}/tests/fixtures/array_stream/issues.json"
     DESTINATION "${stage_root}/tests/fixtures/array_stream")

file(WRITE "${stage_root}/run-smoke.sh" [=[
#!/bin/sh
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DIR"

for binary in \
  lonejson_static_smoke \
  lonejson_shared_smoke \
  example_array_stream \
  example_fixed_storage \
  example_generator_pull \
  example_json_value_buffer \
  example_json_value_parse \
  example_json_value_source \
  example_json_value_visitor \
  example_parse_file \
  example_parse_reader \
  example_parse_string \
  example_push_parser \
  example_serialize_file \
  example_serialize_jsonl \
  example_serialize_string \
  example_source_bytes \
  example_source_text \
  example_spooled_bytes \
  example_spooled_text
do
  "bin/$binary"
done
]=])
file(CHMOD "${stage_root}/run-smoke.sh"
  PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

foreach(smoke_binary
        lonejson_static_smoke
        lonejson_shared_smoke
        example_array_stream
        example_parse_string
        example_serialize_jsonl)
  execute_process(
    COMMAND "${CMAKE_OTOOL}" -hv "${stage_root}/bin/${smoke_binary}"
    RESULT_VARIABLE otool_result
    OUTPUT_VARIABLE otool_output
    ERROR_VARIABLE otool_error)
  if(NOT otool_result EQUAL 0 OR NOT otool_output MATCHES "ARM64")
    message(FATAL_ERROR
      "Darwin smoke binary is not an arm64 Mach-O executable: "
      "${smoke_binary}\n${otool_output}${otool_error}")
  endif()
endforeach()

set(staged_smoke_archive "${bundle_root}/${stage_name}.zip")
execute_process(
  COMMAND "${LONEJSON_ROOT}/scripts/archive_release_tree.sh"
          "${stage_root}" "${staged_smoke_archive}"
  RESULT_VARIABLE zip_result
  OUTPUT_VARIABLE zip_stdout
  ERROR_VARIABLE zip_stderr)
if(NOT zip_result EQUAL 0)
  message(FATAL_ERROR
    "failed to create Darwin smoke bundle zip\n"
    "stdout:\n${zip_stdout}\n"
    "stderr:\n${zip_stderr}")
endif()

lonejson_publish_artifact("${staged_smoke_archive}" "${smoke_archive}")
message(STATUS "Wrote ${smoke_archive}")
