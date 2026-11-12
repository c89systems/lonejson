if(NOT DEFINED LONEJSON_ROOT)
  message(FATAL_ERROR "LONEJSON_ROOT is required")
endif()
if(NOT DEFINED LONEJSON_PUBLIC_HEADER)
  message(FATAL_ERROR "LONEJSON_PUBLIC_HEADER is required")
endif()
if(NOT DEFINED LONEJSON_INTERNAL_IMPL)
  message(FATAL_ERROR "LONEJSON_INTERNAL_IMPL is required")
endif()
if(NOT DEFINED LONEJSON_SINGLE_HEADER_BUILD)
  message(FATAL_ERROR "LONEJSON_SINGLE_HEADER_BUILD is required")
endif()
if(NOT DEFINED LONEJSON_SINGLE_HEADER_BUILD_ALIAS)
  message(FATAL_ERROR "LONEJSON_SINGLE_HEADER_BUILD_ALIAS is required")
endif()
if(NOT DEFINED LONEJSON_SINGLE_HEADER_DIST_GZ)
  message(FATAL_ERROR "LONEJSON_SINGLE_HEADER_DIST_GZ is required")
endif()
include("${LONEJSON_ROOT}/cmake/lonejson_build_workspace.cmake")
get_filename_component(build_dir "${LONEJSON_SINGLE_HEADER_BUILD}" DIRECTORY)
get_filename_component(build_alias_dir "${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}" DIRECTORY)
lonejson_require_build_workspace("${build_dir}")
lonejson_require_build_workspace("${build_alias_dir}")
lonejson_require_build_workspace("${LONEJSON_ROOT}/build/packaging")
include("${LONEJSON_ROOT}/cmake/lonejson_dist_dir.cmake")
lonejson_prepare_dist_dir()
function(lonejson_read_normalized out_var path)
  file(READ "${path}" content)
  string(REPLACE "\r\n" "\n" content "${content}")
  set(${out_var} "${content}" PARENT_SCOPE)
endfunction()

lonejson_read_normalized(public_header "${LONEJSON_PUBLIC_HEADER}")

get_filename_component(impl_dir "${LONEJSON_INTERNAL_IMPL}" DIRECTORY)
set(impl_parts_dir "${impl_dir}/impl")
if(EXISTS "${impl_parts_dir}")
  file(GLOB impl_parts RELATIVE "${LONEJSON_ROOT}" "${impl_parts_dir}/*.h")
  list(SORT impl_parts)
  set(impl_header "")
  foreach(part IN LISTS impl_parts)
    lonejson_read_normalized(part_content "${LONEJSON_ROOT}/${part}")
    string(APPEND impl_header "${part_content}")
    if(NOT part_content MATCHES "\n$")
      string(APPEND impl_header "\n")
    endif()
  endforeach()
else()
  lonejson_read_normalized(impl_header "${LONEJSON_INTERNAL_IMPL}")
endif()

string(REGEX REPLACE "\n#endif[ \t]*\n?$" ""
                     public_header_without_final_endif
                     "${public_header}")
set(single_header
    "${public_header_without_final_endif}\n\n#ifdef LONEJSON_IMPLEMENTATION\n${impl_header}#endif\n\n#endif\n")

if(NOT LONEJSON_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
  message(FATAL_ERROR "LONEJSON_VERSION must be in X.Y.Z form")
endif()
set(release_version_major "${CMAKE_MATCH_1}")
set(release_version_minor "${CMAKE_MATCH_2}")
set(release_version_patch "${CMAKE_MATCH_3}")
get_filename_component(build_dir "${LONEJSON_SINGLE_HEADER_BUILD}" DIRECTORY)
get_filename_component(build_alias_dir "${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}" DIRECTORY)
get_filename_component(dist_dir "${LONEJSON_SINGLE_HEADER_DIST_GZ}" DIRECTORY)
set(dist_header "${build_dir}/lonejson-release.h")
set(build_tmp "${build_dir}/lonejson_single_header.h.tmp")
file(MAKE_DIRECTORY "${build_dir}" "${build_alias_dir}" "${dist_dir}")
file(WRITE "${build_tmp}" "${single_header}")

# Maintained fragments are formatted before committing. Packaging concatenates
# them without formatting the complete implementation again for every target.

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${build_tmp}" "${LONEJSON_SINGLE_HEADER_BUILD}"
  RESULT_VARIABLE copy_build_result
)
if(NOT copy_build_result EQUAL 0)
  message(FATAL_ERROR "failed to update build single-header artifact")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${build_tmp}" "${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}"
  RESULT_VARIABLE copy_alias_result
)
if(NOT copy_alias_result EQUAL 0)
  message(FATAL_ERROR "failed to update build alias single-header artifact")
endif()
# The two build headers are identical. The release header differs only in six
# one-line version definitions.
file(READ "${build_tmp}" dist_header_release_content)
foreach(version_macro IN ITEMS
    LONEJSON_VERSION_MAJOR
    LONEJSON_VERSION_MINOR
    LONEJSON_VERSION_PATCH
    LJ_VERSION_MAJOR
    LJ_VERSION_MINOR
    LJ_VERSION_PATCH)
  if(version_macro STREQUAL "LONEJSON_VERSION_MAJOR" OR
     version_macro STREQUAL "LJ_VERSION_MAJOR")
    set(version_value "${release_version_major}")
  elseif(version_macro STREQUAL "LONEJSON_VERSION_MINOR" OR
         version_macro STREQUAL "LJ_VERSION_MINOR")
    set(version_value "${release_version_minor}")
  else()
    set(version_value "${release_version_patch}")
  endif()
  string(REGEX MATCH
    "#define[ \t]+${version_macro}[ \t]+[^ \t\r\n]+"
    version_definition "${public_header}")
  string(REPLACE "${version_definition}"
    "#define ${version_macro} ${version_value}"
    dist_header_release_content
    "${dist_header_release_content}")
endforeach()
file(WRITE "${dist_header}" "${dist_header_release_content}")
file(REMOVE "${build_tmp}")

find_program(LONEJSON_GZIP_BIN NAMES gzip)
if(NOT LONEJSON_GZIP_BIN)
  message(FATAL_ERROR "failed to find gzip for single-header artifact creation")
endif()

execute_process(
  COMMAND "${LONEJSON_GZIP_BIN}" -n -9 -c "${dist_header}"
  OUTPUT_FILE "${build_dir}/lonejson-release.h.gz"
  RESULT_VARIABLE gzip_result
)
if(NOT gzip_result EQUAL 0)
  message(FATAL_ERROR "failed to gzip single-header artifact")
endif()
lonejson_publish_artifact("${build_dir}/lonejson-release.h.gz" "${LONEJSON_SINGLE_HEADER_DIST_GZ}")
file(REMOVE "${dist_header}")
