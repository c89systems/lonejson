cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED LONEJSON_SOURCE_DIR)
  get_filename_component(LONEJSON_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cpkt_metadata.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/CpktDependencyCache.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/toolchains/lonejson_native_bootlin_target.cmake")

if(DEFINED LONEJSON_CPKT_VERSION AND
   NOT LONEJSON_CPKT_VERSION STREQUAL LONEJSON_CPKT_PINNED_VERSION)
  message(FATAL_ERROR
    "LONEJSON_CPKT_VERSION is not configurable. This repository "
    "pins cpkt ${LONEJSON_CPKT_PINNED_VERSION} and its "
    "checksums; update cmake/fetch_cpkt.cmake to change the bundle "
    "version.")
endif()
set(LONEJSON_CPKT_VERSION "${LONEJSON_CPKT_PINNED_VERSION}")
set(LONEJSON_CPKT_BASE_URL
    "${LONEJSON_CPKT_BASE_URL_DEFAULT}"
    CACHE STRING "Base URL for cpkt release downloads.")
set(LONEJSON_CPKT_DOWNLOAD_RETRIES "4" CACHE STRING
    "Number of download attempts for cpkt bundles before failing.")
set(LONEJSON_CPKT_RETRY_DELAY_SECONDS "1" CACHE STRING
    "Seconds to wait between cpkt bundle download retries.")

function(lonejson_detect_host_arch out_var)
  execute_process(
    COMMAND uname -m
    OUTPUT_VARIABLE _uname_m
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _uname_rc)

  if(NOT _uname_rc EQUAL 0)
    message(FATAL_ERROR "Failed to detect host architecture using uname -m.")
  endif()

  if(_uname_m STREQUAL "x86_64" OR _uname_m STREQUAL "amd64")
    set(_arch "x86_64")
  elseif(_uname_m STREQUAL "aarch64" OR _uname_m STREQUAL "arm64")
    set(_arch "aarch64")
  elseif(_uname_m MATCHES "^armv[67].*" OR _uname_m STREQUAL "armhf")
    set(_arch "armhf")
  else()
    message(FATAL_ERROR
      "Unsupported host architecture '${_uname_m}'. Override with "
      "-D LONEJSON_CPKT_TARGET_ID=<target>.")
  endif()

  set(${out_var} "${_arch}" PARENT_SCOPE)
endfunction()

function(lonejson_detect_host_os out_var)
  execute_process(
    COMMAND uname -s
    OUTPUT_VARIABLE _uname_s
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _uname_rc)

  if(NOT _uname_rc EQUAL 0)
    message(FATAL_ERROR "Failed to detect host OS using uname -s.")
  endif()
  string(TOLOWER "${_uname_s}" _uname_s_lower)
  set(${out_var} "${_uname_s_lower}" PARENT_SCOPE)
endfunction()

if(NOT DEFINED LONEJSON_CPKT_TARGET_ID)
  lonejson_detect_host_os(_host_os)
  if(_host_os STREQUAL "linux")
    lonejson_detect_native_bootlin_target(
      LONEJSON_CPKT_TARGET_ID
      _host_processor
      _host_arch
      _host_libc
      _host_emulator)
  elseif(_host_os STREQUAL "darwin")
    lonejson_detect_host_arch(_host_arch)
    if(_host_arch STREQUAL "aarch64")
      set(LONEJSON_CPKT_TARGET_ID "arm64-apple-darwin")
    else()
      message(FATAL_ERROR
        "Unsupported host target '${_host_arch}-${_host_os}'. Override with "
        "-D LONEJSON_CPKT_TARGET_ID=<target>.")
    endif()
  else()
    message(FATAL_ERROR
      "Unsupported host OS '${_host_os}'. Override with "
      "-D LONEJSON_CPKT_TARGET_ID=<target>.")
  endif()
endif()

set(_supported_targets
  x86_64-linux-gnu
  x86_64-linux-musl
  aarch64-linux-gnu
  aarch64-linux-musl
  armhf-linux-gnu
  armhf-linux-musl
  arm64-apple-darwin)

if(NOT LONEJSON_CPKT_TARGET_ID IN_LIST _supported_targets)
  message(FATAL_ERROR
    "Unsupported cpkt target '${LONEJSON_CPKT_TARGET_ID}'. "
    "Supported values: ${_supported_targets}.")
endif()

function(lonejson_download_cpkt_bundle url archive_name expected_sha256 out_var)
  if(NOT LONEJSON_CPKT_DOWNLOAD_RETRIES MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "LONEJSON_CPKT_DOWNLOAD_RETRIES must be a non-negative integer.")
  endif()
  if(NOT LONEJSON_CPKT_RETRY_DELAY_SECONDS MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "LONEJSON_CPKT_RETRY_DELAY_SECONDS must be a non-negative integer.")
  endif()
  if(LONEJSON_CPKT_DOWNLOAD_RETRIES LESS 1)
    message(FATAL_ERROR
      "LONEJSON_CPKT_DOWNLOAD_RETRIES must be at least 1.")
  endif()

  if(DEFINED LONEJSON_CPKT_TEST_FAIL_DOWNLOAD_ATTEMPTS)
    set(CPKT_DEPENDENCY_CACHE_TEST_FAIL_DOWNLOAD_ATTEMPTS
      "${LONEJSON_CPKT_TEST_FAIL_DOWNLOAD_ATTEMPTS}")
  endif()
  cpkt_acquire_verified_archive(
    COMPONENT "cpkt"
    URL "${url}"
    SHA256 "${expected_sha256}"
    ARCHIVE_NAME "${archive_name}"
    OUTPUT_VARIABLE _archive_path
    RETRIES "${LONEJSON_CPKT_DOWNLOAD_RETRIES}"
    RETRY_DELAY_SECONDS "${LONEJSON_CPKT_RETRY_DELAY_SECONDS}")
  set(${out_var} "${_archive_path}" PARENT_SCOPE)
endfunction()

set(_target_id "${LONEJSON_CPKT_TARGET_ID}")
set(_bundle_dir_name "cpkt-${LONEJSON_CPKT_VERSION}-${_target_id}")
set(_filename "${_bundle_dir_name}.tar.gz")
set(_url "${LONEJSON_CPKT_BASE_URL}/${_filename}")
set(_deps_root "${LONEJSON_SOURCE_DIR}/.cache/cpkt/${_target_id}")
execute_process(COMMAND "${CMAKE_CURRENT_LIST_DIR}/../scripts/require_cache_path.sh" --local
  "${_deps_root}" RESULT_VARIABLE _cache_status ERROR_VARIABLE _cache_error)
if(NOT _cache_status EQUAL 0)
  message(FATAL_ERROR "${_cache_error}")
endif()
set(_extract_root "${_deps_root}/root")
set(_staging_root "${_deps_root}/extract")
set(_stamp_path "${_extract_root}/.lonejson-cpkt-identity")
lonejson_cpkt_sha256("${_target_id}" _expected_sha256)
if(DEFINED LONEJSON_CPKT_EXPECTED_SHA256_OVERRIDE)
  set(_expected_sha256 "${LONEJSON_CPKT_EXPECTED_SHA256_OVERRIDE}")
endif()
set(_dependency_identity
  "cache-layout=2\ncomponent=cpkt\nversion=${LONEJSON_CPKT_VERSION}\ntarget=${_target_id}\nurl=${_url}\nsha256=${_expected_sha256}\n")

function(lonejson_validate_cpkt_bundle extract_root filename target_id curl_version_out openssl_version_out)
  set(_manifest "${extract_root}/share/cpkt/packages/core.json")
  if(NOT EXISTS "${_manifest}")
    message(FATAL_ERROR "Downloaded cpkt bundle ${filename} is missing its core package manifest")
  endif()
  file(READ "${_manifest}" _package)
  foreach(_field group release_version target_id)
    string(JSON _${_field} ERROR_VARIABLE _error GET "${_package}" ${_field})
    if(_error)
      message(FATAL_ERROR "Invalid cpkt core package manifest: ${_error}")
    endif()
  endforeach()
  if(NOT _group STREQUAL "core" OR
      NOT _release_version STREQUAL LONEJSON_CPKT_PINNED_VERSION OR
      NOT _target_id STREQUAL target_id)
    message(FATAL_ERROR "Wrong cpkt package identity: ${_group}/${_release_version}/${_target_id}; expected core/${LONEJSON_CPKT_PINNED_VERSION}/${target_id}")
  endif()
  set(_curl_version_header "${extract_root}/include/curl/curlver.h")
  set(_openssl_version_header "${extract_root}/include/openssl/opensslv.h")
  if(target_id MATCHES "apple-darwin$")
    set(_shared_ext "dylib")
  else()
    set(_shared_ext "so")
  endif()

  foreach(_required_path
      "${_curl_version_header}"
      "${_openssl_version_header}"
      "${extract_root}/include/nghttp2/nghttp2.h"
      "${extract_root}/include/zlib.h"
      "${extract_root}/include/lua.h"
      "${extract_root}/lib/liblua.${_shared_ext}"
      "${extract_root}/lib/libcurl.${_shared_ext}"
      "${extract_root}/lib/libssl.${_shared_ext}"
      "${extract_root}/lib/libcrypto.${_shared_ext}"
      "${extract_root}/lib/cmake/CURL/CURLConfig.cmake"
      "${extract_root}/lib/cmake/OpenSSL/OpenSSLConfig.cmake"
      "${extract_root}/lib/cmake/nghttp2/nghttp2Config.cmake"
      "${extract_root}/lib/cmake/zlib/ZLIBConfig.cmake"
      "${extract_root}/lib/cmake/libssh2/libssh2-config.cmake"
      "${extract_root}/lib/pkgconfig/libcurl.pc"
      "${extract_root}/lib/pkgconfig/libcrypto.pc")
    if(NOT EXISTS "${_required_path}")
      message(FATAL_ERROR
        "Downloaded cpkt bundle ${filename} is missing required "
        "file: ${_required_path}")
    endif()
  endforeach()

  file(STRINGS "${_curl_version_header}" _curl_version_line
       REGEX "^#define LIBCURL_VERSION \"[^\"]+\"")
  file(STRINGS "${_openssl_version_header}" _openssl_version_line
       REGEX "^# define OPENSSL_VERSION_TEXT \"[^\"]+\"")
  string(REGEX REPLACE "^#define LIBCURL_VERSION \"([^\"]+)\".*$" "\\1"
         _curl_version "${_curl_version_line}")
  string(REGEX REPLACE "^# define OPENSSL_VERSION_TEXT \"([^\"]+)\".*$" "\\1"
         _openssl_version "${_openssl_version_line}")
  if(NOT _curl_version STREQUAL LONEJSON_CPKT_CURL_VERSION OR
      NOT _openssl_version MATCHES "^OpenSSL ${LONEJSON_CPKT_OPENSSL_VERSION} ")
    message(FATAL_ERROR "cpkt headers do not match pinned curl/OpenSSL versions: ${_curl_version}, ${_openssl_version}")
  endif()

  set(${curl_version_out} "${_curl_version}" PARENT_SCOPE)
  set(${openssl_version_out} "${_openssl_version}" PARENT_SCOPE)
endfunction()

file(MAKE_DIRECTORY "${_deps_root}")

if(EXISTS "${_stamp_path}")
  file(READ "${_stamp_path}" _existing_identity)
  if(_existing_identity STREQUAL _dependency_identity)
    lonejson_validate_cpkt_bundle("${_extract_root}" "${_filename}"
      "${_target_id}" _curl_version _openssl_version)
    message(STATUS
      "cpkt ${_target_id} bundle already extracted at "
      "${_extract_root} with curl ${_curl_version} and ${_openssl_version}")
    return()
  endif()
endif()

message(STATUS "Acquiring verified cpkt archive ${_url}")
lonejson_download_cpkt_bundle("${_url}" "${_filename}" "${_expected_sha256}" _archive_path)

file(REMOVE_RECURSE "${_extract_root}")
file(REMOVE_RECURSE "${_staging_root}")
file(MAKE_DIRECTORY "${_extract_root}")
file(MAKE_DIRECTORY "${_staging_root}")

message(STATUS "Extracting ${_archive_path} to ${_extract_root}")
file(ARCHIVE_EXTRACT INPUT "${_archive_path}" DESTINATION "${_staging_root}")

if(EXISTS "${_staging_root}/${_bundle_dir_name}/include" AND
   EXISTS "${_staging_root}/${_bundle_dir_name}/lib")
  file(COPY "${_staging_root}/${_bundle_dir_name}/" DESTINATION "${_extract_root}")
elseif(EXISTS "${_staging_root}/include" AND EXISTS "${_staging_root}/lib")
  file(COPY "${_staging_root}/" DESTINATION "${_extract_root}")
else()
  file(REMOVE_RECURSE "${_staging_root}")
  message(FATAL_ERROR
    "Downloaded cpkt bundle ${_filename} does not contain the "
    "expected include/ and lib/ directories.")
endif()

lonejson_validate_cpkt_bundle("${_extract_root}" "${_filename}"
  "${_target_id}" _curl_version _openssl_version)

file(WRITE "${_stamp_path}" "${_dependency_identity}")
file(REMOVE_RECURSE "${_staging_root}")

message(STATUS
  "cpkt ${_target_id} bundle ready at ${_extract_root} with "
  "curl ${_curl_version} and ${_openssl_version}")
