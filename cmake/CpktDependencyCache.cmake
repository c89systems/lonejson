include_guard(GLOBAL)

# The global cache contains immutable, checksum-verified archives only.  Every
# consuming checkout owns its own extraction and build state under .cache/.
function(cpkt_resolve_dependency_cache out_var)
  if(DEFINED CPKT_DEPENDENCY_CACHE AND
     NOT "${CPKT_DEPENDENCY_CACHE}" STREQUAL "")
    set(_cache "${CPKT_DEPENDENCY_CACHE}")
  elseif(DEFINED ENV{CPKT_DEPENDENCY_CACHE} AND
         NOT "$ENV{CPKT_DEPENDENCY_CACHE}" STREQUAL "")
    set(_cache "$ENV{CPKT_DEPENDENCY_CACHE}")
  elseif(DEFINED ENV{XDG_CACHE_HOME} AND
         NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
    set(_cache "$ENV{XDG_CACHE_HOME}/cpkt/deps")
  elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    set(_cache "$ENV{HOME}/.cache/cpkt/deps")
  else()
    message(FATAL_ERROR
      "CPKT_DEPENDENCY_CACHE, XDG_CACHE_HOME, or HOME is required to locate "
      "the shared dependency archive cache.")
  endif()

  get_filename_component(_cache "${_cache}" ABSOLUTE)
  execute_process(COMMAND "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../scripts/require_cache_path.sh"
    "${_cache}" RESULT_VARIABLE _cache_status ERROR_VARIABLE _cache_error)
  if(NOT _cache_status EQUAL 0)
    message(FATAL_ERROR "${_cache_error}")
  endif()
  set(CPKT_DEPENDENCY_CACHE "${_cache}" CACHE PATH
    "Shared cache for verified cpkt dependency archives." FORCE)
  set(${out_var} "${_cache}" PARENT_SCOPE)
endfunction()

function(cpkt_acquire_verified_archive)
  cmake_parse_arguments(ARG ""
    "COMPONENT;URL;SHA256;ARCHIVE_NAME;OUTPUT_VARIABLE;RETRIES;RETRY_DELAY_SECONDS"
    "" ${ARGN})
  foreach(_required COMPONENT URL SHA256 ARCHIVE_NAME OUTPUT_VARIABLE)
    if(NOT ARG_${_required})
      message(FATAL_ERROR "cpkt_acquire_verified_archive requires ${_required}")
    endif()
  endforeach()
  string(LENGTH "${ARG_SHA256}" _sha256_length)
  if(NOT _sha256_length EQUAL 64 OR NOT ARG_SHA256 MATCHES "^[0-9A-Fa-f]+$")
    message(FATAL_ERROR
      "${ARG_COMPONENT}: expected SHA-256 must be a 64-digit hexadecimal value")
  endif()
  get_filename_component(_archive_name "${ARG_ARCHIVE_NAME}" NAME)
  if(NOT _archive_name STREQUAL ARG_ARCHIVE_NAME)
    message(FATAL_ERROR "${ARG_COMPONENT}: ARCHIVE_NAME must not contain a path")
  endif()
  if(NOT ARG_RETRIES)
    set(ARG_RETRIES 1)
  endif()
  if(NOT ARG_RETRY_DELAY_SECONDS)
    set(ARG_RETRY_DELAY_SECONDS 0)
  endif()
  if(NOT ARG_RETRIES MATCHES "^[1-9][0-9]*$" OR
     NOT ARG_RETRY_DELAY_SECONDS MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "${ARG_COMPONENT}: RETRIES must be positive and RETRY_DELAY_SECONDS must be non-negative")
  endif()

  string(TOLOWER "${ARG_SHA256}" _expected_sha256)
  cpkt_resolve_dependency_cache(_cache_root)
  set(_archive_dir "${_cache_root}/archives/sha256/${_expected_sha256}")
  set(_archive_path "${_archive_dir}/${_archive_name}")
  set(_lock_path "${_cache_root}/locks/${_expected_sha256}.lock")
  file(MAKE_DIRECTORY "${_archive_dir}" "${_cache_root}/locks")
  file(LOCK "${_lock_path}" GUARD FUNCTION TIMEOUT 120 RESULT_VARIABLE _lock_result)
  if(NOT _lock_result EQUAL 0)
    message(FATAL_ERROR
      "${ARG_COMPONENT}: unable to lock shared archive cache entry ${_lock_path}: ${_lock_result}")
  endif()

  if(DEFINED CPKT_DEPENDENCY_CACHE_TEST_HOLD_LOCK_SECONDS AND
     NOT "${CPKT_DEPENDENCY_CACHE_TEST_HOLD_LOCK_SECONDS}" STREQUAL "")
    if(NOT CPKT_DEPENDENCY_CACHE_TEST_HOLD_LOCK_SECONDS MATCHES "^[1-9][0-9]*$")
      message(FATAL_ERROR
        "CPKT_DEPENDENCY_CACHE_TEST_HOLD_LOCK_SECONDS must be a positive integer")
    endif()
    message(STATUS
      "${ARG_COMPONENT}: test hook holding shared archive cache lock for "
      "${CPKT_DEPENDENCY_CACHE_TEST_HOLD_LOCK_SECONDS}s")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep
      "${CPKT_DEPENDENCY_CACHE_TEST_HOLD_LOCK_SECONDS}")
  endif()

  if(EXISTS "${_archive_path}")
    file(SHA256 "${_archive_path}" _cached_sha256)
    string(TOLOWER "${_cached_sha256}" _cached_sha256)
    if(_cached_sha256 STREQUAL _expected_sha256)
      message(STATUS
        "${ARG_COMPONENT}: reusing verified shared archive cache entry ${_archive_path}")
      set(${ARG_OUTPUT_VARIABLE} "${_archive_path}" PARENT_SCOPE)
      return()
    endif()
    message(WARNING
      "${ARG_COMPONENT}: discarding corrupt shared cache archive ${_archive_path} "
      "(expected ${_expected_sha256}, got ${_cached_sha256})")
    file(REMOVE "${_archive_path}")
  endif()

  # Names and URLs are aliases; only the verified digest identifies an archive.
  # Ignore unpublished downloads left by an interrupted writer.
  file(GLOB _aliases LIST_DIRECTORIES FALSE "${_archive_dir}/*")
  foreach(_alias IN LISTS _aliases)
    get_filename_component(_alias_name "${_alias}" NAME)
    if(_alias_name MATCHES "^\\." OR _alias_name MATCHES "\\.tmp($|\\.)|\\.part($|[-.])")
      continue()
    endif()
    file(SHA256 "${_alias}" _alias_sha256)
    if(NOT _alias_sha256 STREQUAL _expected_sha256)
      continue()
    endif()
    string(RANDOM LENGTH 16 ALPHABET "0123456789abcdef" _nonce)
    set(_temporary_path "${_archive_dir}/.${_archive_name}.${_nonce}.tmp")
    file(COPY_FILE "${_alias}" "${_temporary_path}" RESULT _copy_result)
    if(NOT _copy_result STREQUAL "0")
      file(REMOVE "${_temporary_path}")
      message(FATAL_ERROR "${ARG_COMPONENT}: unable to reuse verified archive: ${_copy_result}")
    endif()
    file(SHA256 "${_temporary_path}" _copied_sha256)
    if(NOT _copied_sha256 STREQUAL _expected_sha256)
      file(REMOVE "${_temporary_path}")
      message(FATAL_ERROR "${ARG_COMPONENT}: verified archive changed while copying ${_alias}")
    endif()
    file(RENAME "${_temporary_path}" "${_archive_path}" RESULT _rename_result)
    if(NOT _rename_result STREQUAL "0")
      file(REMOVE "${_temporary_path}")
      message(FATAL_ERROR "${ARG_COMPONENT}: unable to publish archive alias: ${_rename_result}")
    endif()
    set(${ARG_OUTPUT_VARIABLE} "${_archive_path}" PARENT_SCOPE)
    return()
  endforeach()

  set(_last_message "download not attempted")
  foreach(_attempt RANGE 1 ${ARG_RETRIES})
    string(RANDOM LENGTH 16 ALPHABET "0123456789abcdef" _nonce)
    set(_temporary_path "${_archive_dir}/.${_archive_name}.${_nonce}.tmp")
    if(DEFINED CPKT_DEPENDENCY_CACHE_TEST_FAIL_DOWNLOAD_ATTEMPTS AND
       _attempt LESS_EQUAL CPKT_DEPENDENCY_CACHE_TEST_FAIL_DOWNLOAD_ATTEMPTS)
      set(_download_code 22)
      set(_download_message "simulated download failure")
    else()
      file(DOWNLOAD "${ARG_URL}" "${_temporary_path}"
        TLS_VERIFY ON
        STATUS _download_status)
      list(GET _download_status 0 _download_code)
      list(GET _download_status 1 _download_message)
    endif()

    if(_download_code EQUAL 0 AND EXISTS "${_temporary_path}")
      file(SHA256 "${_temporary_path}" _downloaded_sha256)
      string(TOLOWER "${_downloaded_sha256}" _downloaded_sha256)
      if(_downloaded_sha256 STREQUAL _expected_sha256)
        file(RENAME "${_temporary_path}" "${_archive_path}")
        set(${ARG_OUTPUT_VARIABLE} "${_archive_path}" PARENT_SCOPE)
        return()
      endif()
      set(_download_code 1)
      set(_download_message
        "SHA256 mismatch (expected ${_expected_sha256}, got ${_downloaded_sha256})")
    endif()

    file(REMOVE "${_temporary_path}")
    set(_last_message "${_download_message}")
    if(_attempt LESS ARG_RETRIES)
      message(WARNING
        "${ARG_COMPONENT}: download attempt ${_attempt}/${ARG_RETRIES} failed for "
        "${ARG_URL}: ${_download_message}; retrying in ${ARG_RETRY_DELAY_SECONDS}s")
      if(NOT ARG_RETRY_DELAY_SECONDS STREQUAL "0")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep "${ARG_RETRY_DELAY_SECONDS}")
      endif()
    endif()
  endforeach()

  message(FATAL_ERROR
    "${ARG_COMPONENT}: failed to acquire verified archive\n"
    "  url: ${ARG_URL}\n"
    "  expected_sha256: ${_expected_sha256}\n"
    "  cache_path: ${_archive_path}\n"
    "  reason: ${_last_message}")
endfunction()
