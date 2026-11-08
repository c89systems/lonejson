cmake_minimum_required(VERSION 3.21)

set(LONEJSON_VERSION_OVERRIDE "" CACHE STRING
  "Explicit release-candidate version override in X.Y.Z form.")
set(LONEJSON_EFFECTIVE_VERSION_OVERRIDE "${LONEJSON_VERSION_OVERRIDE}")
if(LONEJSON_EFFECTIVE_VERSION_OVERRIDE STREQUAL "" AND
    DEFINED ENV{LONEJSON_VERSION_OVERRIDE} AND
    NOT "$ENV{LONEJSON_VERSION_OVERRIDE}" STREQUAL "")
  set(LONEJSON_EFFECTIVE_VERSION_OVERRIDE "$ENV{LONEJSON_VERSION_OVERRIDE}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "LONEJSON_VERSION_OVERRIDE=${LONEJSON_EFFECTIVE_VERSION_OVERRIDE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/scripts/release_version.sh"
  OUTPUT_VARIABLE LONEJSON_RESOLVED_VERSION
  ERROR_VARIABLE LONEJSON_VERSION_ERROR
  RESULT_VARIABLE LONEJSON_VERSION_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT LONEJSON_VERSION_RESULT EQUAL 0 OR NOT LONEJSON_RESOLVED_VERSION)
  if(NOT LONEJSON_VERSION_ERROR)
    set(LONEJSON_VERSION_ERROR "version resolver produced no version")
  endif()
  message(FATAL_ERROR "${LONEJSON_VERSION_ERROR}")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LonejsonCompiler.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CpktDependencyCache.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LonejsonDevelopmentRuntime.cmake")
project(lonejson VERSION ${LONEJSON_RESOLVED_VERSION} LANGUAGES C)
set(LONEJSON_VERSION "${PROJECT_VERSION}")
# Environment supplies a default for direct CMake callers. Make supplies -D
# on every configure so its current selection replaces any cached inventory.
if(NOT DEFINED LONEJSON_RELEASE_TARGETS AND DEFINED ENV{LONEJSON_RELEASE_TARGETS})
  set(LONEJSON_RELEASE_TARGETS "$ENV{LONEJSON_RELEASE_TARGETS}")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/release_artifacts.cmake")
set(LONEJSON_RELEASE_TARGETS "${LONEJSON_RELEASE_TARGETS}" CACHE STRING
  "Target IDs required by release artifact inventories.")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT LONEJSON_BOOTLIN_TOOLCHAIN_ROOT)
  message(FATAL_ERROR "Linux builds require the pinned Bootlin toolchain; host or custom compiler collections are unsupported")
endif()

cpkt_resolve_dependency_cache(LONEJSON_RESOLVED_DEPENDENCY_CACHE)

if(LONEJSON_LUA_EXECUTABLE)
  execute_process(
    COMMAND "${LONEJSON_LUA_EXECUTABLE}" -v
    RESULT_VARIABLE LONEJSON_LUA_VERSION_RESULT
    OUTPUT_VARIABLE LONEJSON_LUA_VERSION_OUTPUT
    ERROR_VARIABLE LONEJSON_LUA_VERSION_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  set(LONEJSON_LUA_VERSION_TEXT
    "${LONEJSON_LUA_VERSION_OUTPUT}${LONEJSON_LUA_VERSION_ERROR}")
  if(NOT LONEJSON_LUA_VERSION_RESULT EQUAL 0 OR
      NOT LONEJSON_LUA_VERSION_TEXT MATCHES "Lua 5\\.5\\.")
    message(FATAL_ERROR
      "lonejson requires LONEJSON_LUA_EXECUTABLE to be a Lua 5.5 executable; ${LONEJSON_LUA_VERSION_TEXT}")
  endif()
  set(LONEJSON_LUA_EXECUTABLE "${LONEJSON_LUA_EXECUTABLE}" CACHE FILEPATH
      "Resolved Lua 5.5 executable for lonejson tooling." FORCE)
else()
  execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/resolve_lua55.sh"
    RESULT_VARIABLE LONEJSON_LUA_VERSION_RESULT
    OUTPUT_VARIABLE LONEJSON_LUA_VERSION_OUTPUT
    ERROR_VARIABLE LONEJSON_LUA_VERSION_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  set(LONEJSON_LUA_VERSION_TEXT
    "${LONEJSON_LUA_VERSION_OUTPUT}${LONEJSON_LUA_VERSION_ERROR}")
  if(NOT LONEJSON_LUA_VERSION_RESULT EQUAL 0 OR
      NOT LONEJSON_LUA_VERSION_OUTPUT)
    message(FATAL_ERROR
      "lonejson requires a Lua 5.5 executable for Lua tooling; ${LONEJSON_LUA_VERSION_TEXT}")
  endif()
  set(LONEJSON_LUA_EXECUTABLE "${LONEJSON_LUA_VERSION_OUTPUT}" CACHE FILEPATH
      "Resolved Lua 5.5 executable for lonejson tooling." FORCE)
endif()
find_program(LONEJSON_LUAROCKS_EXECUTABLE NAMES luarocks)
find_program(CLANG_FORMAT_BIN NAMES clang-format)

option(LONEJSON_BUILD_TESTS "Build lonejson tests." ON)
option(LONEJSON_BUILD_EXAMPLES "Build lonejson examples." OFF)
option(LONEJSON_BUILD_BENCHMARKS "Build lonejson benchmarks." OFF)
option(LONEJSON_BUILD_FUZZERS "Build lonejson AFL++ fuzz targets." OFF)
set(LONEJSON_FUZZ_SECONDS "30" CACHE STRING "Duration per AFL++ case; smoke uses LONEJSON_FUZZ_EXECUTIONS instead.")
option(LONEJSON_ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer." OFF)
option(LONEJSON_ASAN_DISABLE_LEAK_DETECTION
  "Disable LeakSanitizer in cross-emulated ASan test executables." OFF)
option(LONEJSON_ENABLE_TSAN "Enable ThreadSanitizer." OFF)
option(LONEJSON_ENABLE_STACK_USAGE "Emit compiler stack-usage reports when supported." OFF)
option(LONEJSON_BUILD_WITH_CURL "Build lonejson with curl adapter APIs enabled." OFF)
option(LONEJSON_BUILD_WITH_OPENSSL "Build lonejson with OpenSSL-backed auth/crypto support enabled." OFF)
option(LONEJSON_BUILD_WITH_JWT "Build lonejson with JWT/JWK auth APIs enabled." OFF)
option(LONEJSON_BUILD_WITH_OIDC "Build lonejson with OIDC/OAuth2 helper APIs enabled." OFF)
set(LONEJSON_ABI_VERSION "28" CACHE STRING "Shared-library ABI / SONAME version"
    FORCE)

set(LONEJSON_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(LONEJSON_SHARED_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build")
set(LONEJSON_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist" CACHE PATH "Directory for generated release artifacts.")
set(LONEJSON_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(LONEJSON_GENERATED_INCLUDE_DIR "${LONEJSON_GENERATED_DIR}/include")
set(LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR "${LONEJSON_GENERATED_DIR}/single-header/include")
set(LONEJSON_GENERATED_FIXTURE_DIR "${LONEJSON_SHARED_BUILD_DIR}/generated/fixtures")
set(LONEJSON_TEST_BUNDLE_ROOT "" CACHE PATH "Root directory for extracted cpkt dependency bundle content used by tests.")
set(LONEJSON_CPKT_ROOT "" CACHE PATH "Root directory for extracted cpkt dependency bundle content.")
if(LONEJSON_CPKT_ROOT AND IS_DIRECTORY "${LONEJSON_CPKT_ROOT}")
  execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/require_cache_path.sh"
    --local "${LONEJSON_CPKT_ROOT}" RESULT_VARIABLE _cpkt_root_status
    ERROR_VARIABLE _cpkt_root_error)
  if(NOT _cpkt_root_status EQUAL 0)
    message(FATAL_ERROR "${_cpkt_root_error}")
  endif()
endif()
set(LONEJSON_TEST_TIMEOUT "120" CACHE STRING "Default timeout in seconds for CTest tests that do not set their own timeout.")
set(LONEJSON_TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "Artifact architecture name")
set(LONEJSON_TARGET_OS "${CMAKE_SYSTEM_NAME}" CACHE STRING "Artifact operating system name")
set(LONEJSON_TARGET_LIBC "" CACHE STRING "Artifact libc suffix for Linux targets (gnu, musl, or empty)")
set(LONEJSON_STACK_USAGE_TARGETS "")
set(LONEJSON_SINGLE_HEADER_BUILD "${LONEJSON_GENERATED_INCLUDE_DIR}/lonejson_single_header.h")
set(LONEJSON_SINGLE_HEADER_BUILD_ALIAS "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}/lonejson.h")
set(LONEJSON_SINGLE_HEADER_DIST_GZ "${LONEJSON_DIST_DIR}/lonejson-${LONEJSON_RESOLVED_VERSION}.h.gz")

include(cmake/LonejsonLinkWarnings.cmake)

function(lonejson_configure_c_target target)
  if(UNIX OR APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_compile_definitions(${target} PRIVATE
      _POSIX_C_SOURCE=200809L
      _FILE_OFFSET_BITS=64)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(${target} PRIVATE _DARWIN_C_SOURCE=1)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE
      -std=c89
      -Wall
      -Wextra
      -Wpedantic
      -pedantic-errors
      -Werror
    )
  endif()
  lonejson_configure_link_warnings(${target})
  lonejson_configure_development_runtime(${target})
endfunction()

set(LONEJSON_STRICT_DOWNSTREAM_WARNING_FLAGS
  -std=c89
  -Wall
  -Wextra
  -Werror
  -Wpedantic
  -Wno-long-long
  -Wstrict-prototypes
  -Wmissing-prototypes
  -Wshadow
  -Wpointer-arith
  -Wcast-qual
  -Wwrite-strings
  -Wdeclaration-after-statement)

function(lonejson_configure_strict_downstream_target target)
  if(UNIX OR APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_compile_definitions(${target} PRIVATE
      _POSIX_C_SOURCE=200809L
      _FILE_OFFSET_BITS=64)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(${target} PRIVATE _DARWIN_C_SOURCE=1)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE
      ${LONEJSON_STRICT_DOWNSTREAM_WARNING_FLAGS})
  endif()
  lonejson_configure_link_warnings(${target})
  lonejson_configure_development_runtime(${target})
endfunction()

function(lonejson_configure_lua_c_target target)
  if(UNIX OR APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_compile_definitions(${target} PRIVATE
      _POSIX_C_SOURCE=200809L
      _FILE_OFFSET_BITS=64)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(${target} PRIVATE _DARWIN_C_SOURCE=1)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE
      -std=gnu89
      -Wall
      -Wextra
      -Werror)
  endif()
  lonejson_configure_link_warnings(${target})
  lonejson_configure_development_runtime(${target})
endfunction()

string(TOLOWER "${LONEJSON_TARGET_OS}" LONEJSON_TARGET_OS_LOWER)
string(TOLOWER "${CMAKE_SYSTEM_NAME}" LONEJSON_CMAKE_SYSTEM_NAME_LOWER)
string(TOLOWER "${LONEJSON_TARGET_ARCH}" LONEJSON_TARGET_ARCH_LOWER)
if(LONEJSON_TARGET_ARCH_LOWER STREQUAL "amd64")
  set(LONEJSON_TARGET_ARCH_LOWER "x86_64")
endif()
if(LONEJSON_TARGET_ARCH_LOWER STREQUAL "arm64")
  set(LONEJSON_TARGET_ARCH_LOWER "aarch64")
endif()
if(LONEJSON_TARGET_OS_LOWER STREQUAL "linux" AND NOT LONEJSON_TARGET_LIBC)
  set(LONEJSON_TARGET_LIBC "gnu")
endif()
if(LONEJSON_TARGET_OS_LOWER STREQUAL "linux")
  set(LONEJSON_TARGET_ID "${LONEJSON_TARGET_ARCH_LOWER}-linux-${LONEJSON_TARGET_LIBC}")
elseif(LONEJSON_TARGET_OS_LOWER STREQUAL "darwin" AND
       LONEJSON_TARGET_ARCH_LOWER STREQUAL "aarch64")
  set(LONEJSON_TARGET_ID "arm64-apple-darwin")
else()
  set(LONEJSON_TARGET_ID "${LONEJSON_TARGET_ARCH_LOWER}-${LONEJSON_TARGET_OS_LOWER}")
endif()
set(LONEJSON_RELEASE_TARGET_IDS
  x86_64-linux-gnu
  x86_64-linux-musl
  aarch64-linux-gnu
  aarch64-linux-musl
  armhf-linux-gnu
  armhf-linux-musl
  arm64-apple-darwin)
set(LONEJSON_IS_SUPPORTED_RELEASE_TARGET OFF)
if((LONEJSON_TARGET_OS_LOWER STREQUAL "linux" OR
    LONEJSON_TARGET_OS_LOWER STREQUAL "darwin") AND
   LONEJSON_TARGET_ID IN_LIST LONEJSON_RELEASE_TARGET_IDS)
  set(LONEJSON_IS_SUPPORTED_RELEASE_TARGET ON)
endif()

set(LONEJSON_CAN_RUN_LUA_NATIVE_TESTS OFF)
if(NOT CMAKE_CROSSCOMPILING AND
    LONEJSON_TARGET_OS_LOWER STREQUAL LONEJSON_CMAKE_SYSTEM_NAME_LOWER)
  if(NOT LONEJSON_TARGET_OS_LOWER STREQUAL "linux" OR
      LONEJSON_TARGET_LIBC STREQUAL "gnu")
    set(LONEJSON_CAN_RUN_LUA_NATIVE_TESTS ON)
  endif()
endif()
function(lonejson_find_curl)
  if(LONEJSON_CPKT_ROOT)
    get_filename_component(_cpkt_root "${LONEJSON_CPKT_ROOT}" ABSOLUTE)
    set(_cpkt_curl_config
        "${_cpkt_root}/lib/cmake/CURL/CURLConfig.cmake")
    if(NOT IS_DIRECTORY "${_cpkt_root}")
      message(FATAL_ERROR
        "LONEJSON_CPKT_ROOT does not exist: ${_cpkt_root}. "
        "Run the matching make deps-* target or unset "
        "LONEJSON_CPKT_ROOT to use system curl.")
    endif()
    if(NOT EXISTS "${_cpkt_curl_config}")
      message(FATAL_ERROR
        "LONEJSON_CPKT_ROOT is missing CURLConfig.cmake: "
        "${_cpkt_curl_config}. Use the pinned cpkt "
        "bundle, or unset LONEJSON_CPKT_ROOT to use system curl.")
    endif()
    set(LONEJSON_CPKT_ROOT "${_cpkt_root}" CACHE PATH
        "Root directory for extracted cpkt dependency bundle content."
        FORCE)
    set(CURL_DIR "${_cpkt_root}/lib/cmake/CURL" CACHE PATH
        "Directory containing CURLConfig.cmake." FORCE)
    # The split SDK's CURL config resolves its own transitive packages. Keep
    # those lookups inside the same prefix, including cross configurations and
    # configurations with stale package-directory cache entries.
    list(PREPEND CMAKE_PREFIX_PATH "${_cpkt_root}")
    list(PREPEND CMAKE_FIND_ROOT_PATH "${_cpkt_root}")
    foreach(_package OpenSSL:OpenSSL ZLIB:zlib nghttp2:nghttp2 Libssh2:libssh2)
      string(REPLACE ":" ";" _parts "${_package}")
      list(GET _parts 0 _name)
      list(GET _parts 1 _directory)
      set(${_name}_DIR "${_cpkt_root}/lib/cmake/${_directory}" CACHE PATH
        "Selected cpkt dependency package directory" FORCE)
    endforeach()
    find_package(CURL CONFIG REQUIRED
      PATHS "${_cpkt_root}"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH)
  else()
    find_package(CURL REQUIRED)
  endif()
  if(NOT TARGET CURL::libcurl)
    message(FATAL_ERROR
      "CURL was found but did not provide the CURL::libcurl target.")
  endif()
endfunction()

if(LONEJSON_BUILD_WITH_CURL)
  lonejson_find_curl()
endif()

function(lonejson_find_openssl)
  if(LONEJSON_CPKT_ROOT)
    get_filename_component(_cpkt_root "${LONEJSON_CPKT_ROOT}" ABSOLUTE)
    set(_cpkt_openssl_config
        "${_cpkt_root}/lib/cmake/OpenSSL/OpenSSLConfig.cmake")
    if(NOT IS_DIRECTORY "${_cpkt_root}")
      message(FATAL_ERROR
        "LONEJSON_CPKT_ROOT does not exist: ${_cpkt_root}. "
        "Run the matching make deps-* target or unset "
        "LONEJSON_CPKT_ROOT to use system OpenSSL.")
    endif()
    if(NOT EXISTS "${_cpkt_openssl_config}")
      message(FATAL_ERROR
        "LONEJSON_CPKT_ROOT is missing OpenSSLConfig.cmake: "
        "${_cpkt_openssl_config}. Use the pinned cpkt "
        "bundle, or unset LONEJSON_CPKT_ROOT to use system OpenSSL.")
    endif()
    set(LONEJSON_CPKT_ROOT "${_cpkt_root}" CACHE PATH
        "Root directory for extracted cpkt dependency bundle content."
        FORCE)
    set(OpenSSL_DIR "${_cpkt_root}/lib/cmake/OpenSSL" CACHE PATH
        "Directory containing OpenSSLConfig.cmake." FORCE)
    find_package(OpenSSL CONFIG REQUIRED
      PATHS "${_cpkt_root}"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH)
  else()
    find_package(OpenSSL REQUIRED)
  endif()
  if(NOT TARGET OpenSSL::SSL OR NOT TARGET OpenSSL::Crypto)
    message(FATAL_ERROR
      "OpenSSL was found but did not provide OpenSSL::SSL and "
      "OpenSSL::Crypto targets.")
  endif()
  if(NOT DEFINED OPENSSL_VERSION AND DEFINED OpenSSL_VERSION)
    set(OPENSSL_VERSION "${OpenSSL_VERSION}")
  endif()
  if(NOT DEFINED OPENSSL_VERSION OR OPENSSL_VERSION VERSION_LESS 3.0.0)
    message(FATAL_ERROR
      "OpenSSL 3.0 or newer is required for OpenSSL-backed auth builds.")
  endif()
endfunction()

if(LONEJSON_BUILD_WITH_OPENSSL)
  lonejson_find_openssl()
endif()

if(LONEJSON_BUILD_WITH_OIDC AND NOT LONEJSON_BUILD_WITH_JWT)
  message(FATAL_ERROR
    "LONEJSON_BUILD_WITH_OIDC requires LONEJSON_BUILD_WITH_JWT=ON.")
endif()

function(lonejson_target_include_curl target)
  if(CURL_INCLUDE_DIRS)
    target_include_directories(${target} SYSTEM PRIVATE ${CURL_INCLUDE_DIRS})
    return()
  endif()
  get_target_property(curl_include_dirs CURL::libcurl INTERFACE_INCLUDE_DIRECTORIES)
  if(curl_include_dirs)
    target_include_directories(${target} SYSTEM PRIVATE ${curl_include_dirs})
  endif()
endfunction()

function(lonejson_target_include_openssl target)
  get_target_property(openssl_include_dirs OpenSSL::Crypto
    INTERFACE_INCLUDE_DIRECTORIES)
  if(openssl_include_dirs)
    target_include_directories(${target} SYSTEM PRIVATE ${openssl_include_dirs})
  elseif(OPENSSL_INCLUDE_DIR)
    target_include_directories(${target} SYSTEM PRIVATE ${OPENSSL_INCLUDE_DIR})
  endif()
endfunction()

set(LONEJSON_ENABLED_SANITIZER_COUNT 0)
if(LONEJSON_ENABLE_ASAN)
  math(EXPR LONEJSON_ENABLED_SANITIZER_COUNT
    "${LONEJSON_ENABLED_SANITIZER_COUNT} + 1")
endif()
if(LONEJSON_ENABLE_TSAN)
  math(EXPR LONEJSON_ENABLED_SANITIZER_COUNT
    "${LONEJSON_ENABLED_SANITIZER_COUNT} + 1")
endif()
if(LONEJSON_ENABLED_SANITIZER_COUNT GREATER 1)
  message(FATAL_ERROR
    "Enable at most one of LONEJSON_ENABLE_ASAN or LONEJSON_ENABLE_TSAN at a time.")
endif()

if(LONEJSON_ASAN_DISABLE_LEAK_DETECTION AND NOT LONEJSON_ENABLE_ASAN)
  message(FATAL_ERROR
    "LONEJSON_ASAN_DISABLE_LEAK_DETECTION requires LONEJSON_ENABLE_ASAN.")
endif()
if(LONEJSON_ASAN_DISABLE_LEAK_DETECTION AND NOT CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR
    "LONEJSON_ASAN_DISABLE_LEAK_DETECTION is only supported for cross-emulated tests.")
endif()

if(LONEJSON_ENABLE_ASAN AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()
if(LONEJSON_ASAN_DISABLE_LEAK_DETECTION)
  add_compile_options(-include
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lonejson_cross_asan_defaults.h")
endif()

if(LONEJSON_ENABLE_TSAN)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "LONEJSON_ENABLE_TSAN requires Clang or GCC.")
  endif()
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  # Production translation units share the supported default worker-stack budget.
  set_source_files_properties(src/lonejson.c src/lua/lonejson_lua.c PROPERTIES
    COMPILE_OPTIONS "-Wframe-larger-than=8192")
endif()

if(LONEJSON_ENABLE_STACK_USAGE AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-fstack-usage)
endif()

add_custom_target(lonejson_generate_fixtures
  COMMAND "${CMAKE_COMMAND}" -E env
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/ensure_large_fixtures.sh"
    "${LONEJSON_LUA_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_large_fixtures.lua"
    "${LONEJSON_GENERATED_FIXTURE_DIR}"
  VERBATIM)

set(LONEJSON_SOURCES
  src/lonejson.c
)

add_library(lonejson_shared SHARED ${LONEJSON_SOURCES})
add_library(lonejson_static STATIC ${LONEJSON_SOURCES})
add_library(lonejson ALIAS lonejson_static)

foreach(target lonejson_shared lonejson_static)
  target_include_directories(${target}
    PUBLIC
      $<BUILD_INTERFACE:${LONEJSON_PUBLIC_INCLUDE_DIR}>
      $<INSTALL_INTERFACE:include>
  )
  set_target_properties(${target} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    VISIBILITY_INLINES_HIDDEN NO
  )
  lonejson_configure_c_target(${target})
  if(LONEJSON_BUILD_WITH_CURL)
    target_compile_definitions(${target} PRIVATE LONEJSON_WITH_CURL)
    lonejson_target_include_curl(${target})
  endif()
  if(LONEJSON_BUILD_WITH_OPENSSL)
    target_compile_definitions(${target} PRIVATE LONEJSON_WITH_OPENSSL)
    lonejson_target_include_openssl(${target})
  endif()
  if(LONEJSON_BUILD_WITH_JWT)
    target_compile_definitions(${target} PRIVATE LONEJSON_WITH_JWT)
  endif()
  if(LONEJSON_BUILD_WITH_OIDC)
    target_compile_definitions(${target} PRIVATE LONEJSON_WITH_OIDC)
  endif()
endforeach()

if(LONEJSON_BUILD_WITH_OPENSSL AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_link_options(lonejson_shared PRIVATE "LINKER:--exclude-libs,ALL")
endif()
include(cmake/lonejson_exports.cmake)
set(LONEJSON_CORE_EXPORTS "${CMAKE_CURRENT_BINARY_DIR}/lonejson_shared.allowlist")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lonejson_base.exports" core_exports)
foreach(feature curl jwt oidc)
  string(TOUPPER "${feature}" feature_upper)
  if(LONEJSON_BUILD_WITH_${feature_upper})
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lonejson_${feature}.exports" feature_exports)
    string(APPEND core_exports "${feature_exports}")
  endif()
endforeach()
if(LONEJSON_BUILD_WITH_CURL AND LONEJSON_BUILD_WITH_OIDC)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lonejson_curl_oidc.exports" feature_exports)
  string(APPEND core_exports "${feature_exports}")
endif()
file(CONFIGURE OUTPUT "${LONEJSON_CORE_EXPORTS}" CONTENT "${core_exports}" @ONLY)
file(GLOB export_lists "${CMAKE_CURRENT_SOURCE_DIR}/cmake/*.exports")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${export_lists})
lonejson_limit_exports(lonejson_shared "${LONEJSON_CORE_EXPORTS}")

if(LONEJSON_BUILD_FUZZERS)
  if(LONEJSON_ENABLE_TSAN)
    message(FATAL_ERROR
      "LONEJSON_BUILD_FUZZERS is incompatible with LONEJSON_ENABLE_TSAN.")
  endif()
  if(NOT LONEJSON_AFLPP_ROOT)
    message(FATAL_ERROR
      "LONEJSON_BUILD_FUZZERS requires the pinned AFL++ GCC-plugin toolchain.")
  endif()
endif()

set_target_properties(lonejson_shared PROPERTIES
  OUTPUT_NAME lonejson
  VERSION ${PROJECT_VERSION}
  SOVERSION ${LONEJSON_ABI_VERSION}
)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(lonejson_shared PROPERTIES
    MACOSX_RPATH ON
    INSTALL_NAME_DIR "@rpath"
    BUILD_WITH_INSTALL_NAME_DIR ON)
endif()
set_target_properties(lonejson_static PROPERTIES
  OUTPUT_NAME lonejson
)

if(LONEJSON_BUILD_TESTS)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/test-tmp")
  set(BUILD_TESTING ON)
  include(CTest)
  add_test(NAME lonejson_library_exports_tests COMMAND bash
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_library_exports.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_NM}" "${CMAKE_SYSTEM_NAME}"
    "$<TARGET_FILE:lonejson_shared>" "${CMAKE_C_COMPILER}" "${CMAKE_C_FLAGS}" "${LONEJSON_CORE_EXPORTS}")
  if(LONEJSON_BUILD_WITH_CURL AND LONEJSON_IS_SUPPORTED_RELEASE_TARGET)
    add_test(NAME lonejson_build_tree_curl_abi_tests
      COMMAND bash
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_curl_abi_symbols.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${LONEJSON_TARGET_ID}"
        "$<TARGET_FILE:lonejson_shared>"
        "$<TARGET_FILE:lonejson_static>"
        "build tree ${LONEJSON_TARGET_ID}")
    set_tests_properties(lonejson_build_tree_curl_abi_tests PROPERTIES
      LABELS "packaging;smoke;local"
      TIMEOUT 20)
  endif()
  if(LONEJSON_BUILD_WITH_CURL
      AND LONEJSON_BUILD_WITH_OPENSSL
      AND LONEJSON_BUILD_WITH_JWT
      AND LONEJSON_BUILD_WITH_OIDC
      AND LONEJSON_IS_SUPPORTED_RELEASE_TARGET)
    add_test(NAME lonejson_build_tree_jwt_abi_tests
      COMMAND bash
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_jwt_abi_symbols.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${LONEJSON_TARGET_ID}"
        "$<TARGET_FILE:lonejson_shared>"
        "$<TARGET_FILE:lonejson_static>"
        "build tree ${LONEJSON_TARGET_ID}")
    set_tests_properties(lonejson_build_tree_jwt_abi_tests PROPERTIES
      LABELS "packaging;smoke;local"
      TIMEOUT 20)
  endif()

  add_executable(lonejson_tests tests/test_main.c)
  target_include_directories(lonejson_tests PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_compile_definitions(lonejson_tests PRIVATE
    LONEJSON_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    LONEJSON_GENERATED_FIXTURE_DIR="${LONEJSON_GENERATED_FIXTURE_DIR}"
    LONEJSON_TEST_RUNTIME_HANDLE_CACHE_DRAIN=1)
  if(LONEJSON_BUILD_WITH_CURL)
    target_compile_definitions(lonejson_tests PRIVATE LONEJSON_WITH_CURL)
    target_link_libraries(lonejson_tests PRIVATE CURL::libcurl)
  endif()
  if(LONEJSON_BUILD_WITH_OPENSSL)
    target_compile_definitions(lonejson_tests PRIVATE LONEJSON_WITH_OPENSSL)
    target_link_libraries(lonejson_tests PRIVATE OpenSSL::Crypto)
  endif()
  if(LONEJSON_BUILD_WITH_JWT)
    target_compile_definitions(lonejson_tests PRIVATE LONEJSON_WITH_JWT)
  endif()
  if(LONEJSON_BUILD_WITH_OIDC)
    target_compile_definitions(lonejson_tests PRIVATE LONEJSON_WITH_OIDC)
  endif()
  lonejson_configure_c_target(lonejson_tests)
  add_dependencies(lonejson_tests lonejson_generate_fixtures)
  add_test(NAME lonejson_tests COMMAND lonejson_tests)
  if(UNIX)
    add_executable(lonejson_array_stream_record_reuse_tests tests/test_array_stream_record_reuse.c)
    target_link_libraries(lonejson_array_stream_record_reuse_tests PRIVATE lonejson_shared)
    target_compile_definitions(lonejson_array_stream_record_reuse_tests PRIVATE
      LONEJSON_TEST_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    lonejson_configure_c_target(lonejson_array_stream_record_reuse_tests)
    add_test(NAME lonejson_array_stream_record_reuse_tests
      COMMAND lonejson_array_stream_record_reuse_tests)
    list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_array_stream_record_reuse_tests)
    find_package(Threads REQUIRED)
    add_executable(lonejson_worker_stack_tests tests/test_worker_stack.c)
    target_link_libraries(lonejson_worker_stack_tests PRIVATE lonejson_shared Threads::Threads)
    lonejson_configure_c_target(lonejson_worker_stack_tests)
    add_test(NAME lonejson_worker_stack_tests COMMAND lonejson_worker_stack_tests)
    list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_worker_stack_tests)
    add_executable(lonejson_record_lifecycle_stack_tests tests/test_record_lifecycle_stack.c)
    target_link_libraries(lonejson_record_lifecycle_stack_tests PRIVATE lonejson_shared Threads::Threads)
    lonejson_configure_c_target(lonejson_record_lifecycle_stack_tests)
    list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_record_lifecycle_stack_tests)
    foreach(operation init reset cleanup parse)
      add_test(NAME lonejson_record_${operation}_stack_tests
        COMMAND lonejson_record_lifecycle_stack_tests ${operation} 4096)
    endforeach()
    foreach(depth 1 15 16 17 64)
      add_test(NAME lonejson_record_stack_boundary_${depth}_tests
        COMMAND lonejson_record_lifecycle_stack_tests init ${depth})
    endforeach()
    add_test(NAME lonejson_record_stack_oom_tests
      COMMAND lonejson_record_lifecycle_stack_tests oom 64)
    add_test(NAME lonejson_record_stack_cycles_tests
      COMMAND lonejson_record_lifecycle_stack_tests cycles 64)
    add_test(NAME lonejson_record_stack_arrays_tests
      COMMAND lonejson_record_lifecycle_stack_tests arrays 4096)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME lonejson_bootlin_runtime_tests
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bootlin_runtime.sh"
        "${CMAKE_CURRENT_BINARY_DIR}" "$<TARGET_FILE:lonejson_tests>")
    set_tests_properties(lonejson_bootlin_runtime_tests PROPERTIES
      LABELS "smoke;local" TIMEOUT 20)
    add_test(NAME lonejson_bootlin_runtime_symlink_cache_tests
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bootlin_runtime_symlink_cache.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(lonejson_bootlin_runtime_symlink_cache_tests PROPERTIES
      LABELS "smoke;local" TIMEOUT 20)
  endif()
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_tests)

  if(LONEJSON_BUILD_WITH_CURL AND UNIX)
    add_executable(lonejson_curl_rewind_e2e tests/curl_rewind_e2e.c)
    target_include_directories(lonejson_curl_rewind_e2e PRIVATE
      "${LONEJSON_PUBLIC_INCLUDE_DIR}")
    target_compile_definitions(lonejson_curl_rewind_e2e PRIVATE LONEJSON_WITH_CURL
      LONEJSON_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(lonejson_curl_rewind_e2e PRIVATE lonejson CURL::libcurl)
    lonejson_configure_c_target(lonejson_curl_rewind_e2e)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
      add_test(NAME lonejson_curl_bootlin_runtime_tests
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bootlin_runtime.sh"
          "${CMAKE_CURRENT_BINARY_DIR}"
          "$<TARGET_FILE:lonejson_curl_rewind_tests>" --run)
      set_tests_properties(lonejson_curl_bootlin_runtime_tests PROPERTIES
        LABELS "smoke;local;integration" TIMEOUT 20)
    endif()

    add_executable(lonejson_curl_rewind_tests tests/test_curl_rewind.c)
    target_include_directories(lonejson_curl_rewind_tests PRIVATE
      "${LONEJSON_PUBLIC_INCLUDE_DIR}")
    target_compile_definitions(lonejson_curl_rewind_tests PRIVATE LONEJSON_WITH_CURL
      LONEJSON_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(lonejson_curl_rewind_tests PRIVATE lonejson CURL::libcurl)
    lonejson_configure_c_target(lonejson_curl_rewind_tests)
    add_test(NAME lonejson_curl_rewind_tests COMMAND lonejson_curl_rewind_tests)
    set_tests_properties(lonejson_curl_rewind_tests PROPERTIES
      LABELS "integration;local;offline" TIMEOUT 60)
  endif()

  if(LONEJSON_BUILD_WITH_CURL AND LONEJSON_BUILD_WITH_OPENSSL AND
      LONEJSON_BUILD_WITH_JWT AND LONEJSON_BUILD_WITH_OIDC)
    add_executable(lonejson_oidc_fixture_server
      tests/oidc_fixture_server.c)
    target_include_directories(lonejson_oidc_fixture_server PRIVATE
      "${LONEJSON_PUBLIC_INCLUDE_DIR}")
    target_compile_definitions(lonejson_oidc_fixture_server PRIVATE
      LONEJSON_WITH_CURL
      LONEJSON_WITH_OPENSSL
      LONEJSON_WITH_JWT
      LONEJSON_WITH_OIDC)
    target_link_libraries(lonejson_oidc_fixture_server PRIVATE
      lonejson
      CURL::libcurl
      OpenSSL::Crypto)
    lonejson_configure_c_target(lonejson_oidc_fixture_server)

    add_executable(lonejson_m2m_fixture_server
      tests/m2m_fixture_server.c)
    target_include_directories(lonejson_m2m_fixture_server PRIVATE
      "${LONEJSON_PUBLIC_INCLUDE_DIR}")
    target_compile_definitions(lonejson_m2m_fixture_server PRIVATE
      LONEJSON_WITH_JWT
      LONEJSON_WITH_OIDC
      LONEJSON_WITH_OPENSSL)
    target_link_libraries(lonejson_m2m_fixture_server PRIVATE
      lonejson
      OpenSSL::Crypto)
    lonejson_configure_c_target(lonejson_m2m_fixture_server)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND
        LONEJSON_CAN_RUN_LUA_NATIVE_TESTS AND NOT LONEJSON_ENABLE_ASAN AND
        NOT LONEJSON_ENABLE_TSAN)
      foreach(case IN ITEMS curl oidc m2m)
        add_test(NAME lonejson_${case}_e2e_tests COMMAND bash
          "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_${case}_e2e.sh")
        set_tests_properties(lonejson_${case}_e2e_tests PROPERTIES
          LABELS "local;e2e" TIMEOUT 300
          ENVIRONMENT "LONEJSON_E2E_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}")
      endforeach()
      add_test(NAME lonejson_devenv_ownership_e2e_tests COMMAND bash
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_devenv_ownership.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
      set_tests_properties(lonejson_devenv_ownership_e2e_tests PROPERTIES
        LABELS "local;e2e" TIMEOUT 60)
    endif()
  endif()

  add_executable(lonejson_map_cache_no_tls_tests
    tests/test_map_cache_no_tls.c)
  target_include_directories(lonejson_map_cache_no_tls_tests PRIVATE
    "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_compile_definitions(lonejson_map_cache_no_tls_tests PRIVATE
    LONEJSON_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    LONEJSON_GENERATED_FIXTURE_DIR="${LONEJSON_GENERATED_FIXTURE_DIR}"
    LONEJSON_TEST_RUNTIME_HANDLE_CACHE_DRAIN=1)
  lonejson_configure_c_target(lonejson_map_cache_no_tls_tests)
  add_test(NAME lonejson_map_cache_no_tls_tests
    COMMAND lonejson_map_cache_no_tls_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_map_cache_no_tls_tests)

  find_program(LONEJSON_BASH_EXECUTABLE bash)
  if(LONEJSON_BASH_EXECUTABLE AND LONEJSON_LUA_EXECUTABLE
      AND LONEJSON_LUAROCKS_EXECUTABLE)
    if(LONEJSON_CAN_RUN_LUA_NATIVE_TESTS AND
        (NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" OR
         EXISTS "${LONEJSON_CPKT_ROOT}/include/lua.h"))
      add_test(
        NAME lonejson_bench_baseline_history_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bench_baseline_history.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}"
          "${LONEJSON_LUA_EXECUTABLE}"
          "${LONEJSON_LUAROCKS_EXECUTABLE}"
          "${CMAKE_CURRENT_BINARY_DIR}")
      add_test(
        NAME lonejson_bench_retry_confirm_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bench_retry_confirm.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}"
          "${LONEJSON_LUA_EXECUTABLE}"
          "${LONEJSON_LUAROCKS_EXECUTABLE}"
          "${CMAKE_CURRENT_BINARY_DIR}")
      add_test(
        NAME lonejson_lua_rock_makefile_deps_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_rock_makefile_deps.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}"
          "${LONEJSON_LUAROCKS_EXECUTABLE}")
      set_tests_properties(lonejson_bench_baseline_history_tests
        lonejson_bench_retry_confirm_tests lonejson_lua_rock_makefile_deps_tests
        PROPERTIES LABELS "local;host-policy")
    endif()
  endif()

  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LonejsonNativeLuaTests.cmake")

  set(LONEJSON_TARGET_LUA_INCLUDE_DIR
    "${LONEJSON_CPKT_ROOT}/include")
  set(LONEJSON_TARGET_LUA_LIBRARY
    "${LONEJSON_CPKT_ROOT}/lib/liblua.so")
  if(LONEJSON_TARGET_OS_LOWER STREQUAL "linux" AND
      LONEJSON_CPKT_ROOT AND
      EXISTS "${LONEJSON_TARGET_LUA_INCLUDE_DIR}/lua.h" AND
      EXISTS "${LONEJSON_TARGET_LUA_LIBRARY}")
    foreach(lua_module_target IN ITEMS lonejson_lua_target_core lonejson_lua_benchmark_core)
      set(lua_module_output "${CMAKE_CURRENT_BINARY_DIR}")
      if(lua_module_target STREQUAL "lonejson_lua_benchmark_core")
        set(lua_module_output "${CMAKE_CURRENT_BINARY_DIR}/lua-benchmark")
      endif()
      add_library(${lua_module_target} MODULE src/lua/lonejson_lua.c)
      target_include_directories(${lua_module_target} PRIVATE
        "${LONEJSON_PUBLIC_INCLUDE_DIR}"
        "${LONEJSON_TARGET_LUA_INCLUDE_DIR}")
      target_link_libraries(${lua_module_target} PRIVATE
        lonejson_shared
        "${LONEJSON_TARGET_LUA_LIBRARY}")
      set_target_properties(${lua_module_target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME core
        LIBRARY_OUTPUT_DIRECTORY
          "${lua_module_output}/lua-target/lonejson"
        BUILD_RPATH
          "${CMAKE_CURRENT_BINARY_DIR};${LONEJSON_CPKT_ROOT}/lib")
      if(LONEJSON_BUILD_WITH_CURL)
        target_compile_definitions(${lua_module_target} PRIVATE
          LONEJSON_WITH_CURL)
        lonejson_target_include_curl(${lua_module_target})
        target_link_libraries(${lua_module_target} PRIVATE CURL::libcurl)
      endif()
      if(LONEJSON_BUILD_WITH_OPENSSL AND LONEJSON_BUILD_WITH_JWT)
        target_compile_definitions(${lua_module_target} PRIVATE
          LONEJSON_WITH_OPENSSL)
        lonejson_target_include_openssl(${lua_module_target})
        target_link_libraries(${lua_module_target} PRIVATE OpenSSL::Crypto)
      endif()
      if(LONEJSON_BUILD_WITH_JWT)
        target_compile_definitions(${lua_module_target} PRIVATE
          LONEJSON_WITH_JWT)
      endif()
      if(LONEJSON_BUILD_WITH_OIDC)
        target_compile_definitions(${lua_module_target} PRIVATE
          LONEJSON_WITH_OIDC)
      endif()
      lonejson_configure_lua_c_target(${lua_module_target})
      lonejson_limit_exports(${lua_module_target}
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lonejson_lua.exports")
    endforeach()
    target_sources(lonejson_lua_benchmark_core PRIVATE
      tests/test_lua_benchmark_configuration.c)
    target_compile_definitions(lonejson_lua_target_core PRIVATE
      LONEJSON_TEST_FORCE_LUA_LEGACY_USERVALUE=1
      LONEJSON_TEST_MAP_ANALYSIS_COUNTER=1
      LONEJSON_TEST_LUA_ENCODE_STATS=1
      LONEJSON_TEST_LUA_SIMULATE_NO_MAXINTEGER=1)

    add_executable(lonejson_lua_target_runner
      tests/test_lua_embedded_runner.c)
    target_include_directories(lonejson_lua_target_runner PRIVATE
      "${LONEJSON_TARGET_LUA_INCLUDE_DIR}")
    target_link_libraries(lonejson_lua_target_runner PRIVATE
      "${LONEJSON_TARGET_LUA_LIBRARY}")
    set_target_properties(lonejson_lua_target_runner PROPERTIES
      BUILD_RPATH "${LONEJSON_CPKT_ROOT}/lib")
    lonejson_configure_lua_c_target(lonejson_lua_target_runner)
    add_dependencies(lonejson_lua_target_runner
      lonejson_lua_target_core lonejson_lua_benchmark_core)
    if(NOT CMAKE_CROSSCOMPILING)
      add_test(NAME lonejson_lua_benchmark_configuration_tests
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_lua_benchmark.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_benchmark_configuration.lua")
    endif()
    add_test(
      NAME lonejson_lua_target_tests
      COMMAND lonejson_lua_target_runner
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua.lua"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_fuzz.lua"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_schema_cache.lua"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_encode_stats.lua")
    add_test(NAME lonejson_lua_fuzz_tests COMMAND lonejson_lua_target_runner
      "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_fuzz.lua")
    add_test(
      NAME lonejson_lua_target_runner_argument_tests
      COMMAND lonejson_lua_target_runner
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        -- "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_embedded_runner_args.lua"
        runner-argument)

    add_executable(lonejson_lua_interop_tests
      src/lua/lonejson_lua.c
      tests/test_lua_interop.c)
    target_include_directories(lonejson_lua_interop_tests PRIVATE
      "${LONEJSON_PUBLIC_INCLUDE_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lua"
      "${LONEJSON_TARGET_LUA_INCLUDE_DIR}")
    target_compile_definitions(lonejson_lua_interop_tests PRIVATE
      LONEJSON_TEST_FORCE_LUA_LEGACY_USERVALUE=1
      LONEJSON_TEST_MAP_ANALYSIS_COUNTER=1
      LONEJSON_TEST_LUA_ENCODE_STATS=1
      LONEJSON_TEST_LUA_SIMULATE_NO_MAXINTEGER=1)
    target_link_libraries(lonejson_lua_interop_tests PRIVATE
      lonejson_shared
      Threads::Threads
      "${LONEJSON_TARGET_LUA_LIBRARY}")
    set_target_properties(lonejson_lua_interop_tests PROPERTIES
      BUILD_RPATH "${CMAKE_CURRENT_BINARY_DIR};${LONEJSON_CPKT_ROOT}/lib")
    if(LONEJSON_BUILD_WITH_CURL)
      target_compile_definitions(lonejson_lua_interop_tests PRIVATE
        LONEJSON_WITH_CURL)
      lonejson_target_include_curl(lonejson_lua_interop_tests)
      target_link_libraries(lonejson_lua_interop_tests PRIVATE CURL::libcurl)
    endif()
    if(LONEJSON_BUILD_WITH_OPENSSL AND LONEJSON_BUILD_WITH_JWT)
      target_compile_definitions(lonejson_lua_interop_tests PRIVATE
        LONEJSON_WITH_OPENSSL)
      lonejson_target_include_openssl(lonejson_lua_interop_tests)
      target_link_libraries(lonejson_lua_interop_tests PRIVATE OpenSSL::Crypto)
    endif()
    if(LONEJSON_BUILD_WITH_JWT)
      target_compile_definitions(lonejson_lua_interop_tests PRIVATE
        LONEJSON_WITH_JWT)
    endif()
    if(LONEJSON_BUILD_WITH_OIDC)
      target_compile_definitions(lonejson_lua_interop_tests PRIVATE
        LONEJSON_WITH_OIDC)
    endif()
    lonejson_configure_lua_c_target(lonejson_lua_interop_tests)
    add_test(
      NAME lonejson_lua_interop_tests
      COMMAND lonejson_lua_interop_tests
        "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  get_property(lonejson_runtime_tests DIRECTORY PROPERTY TESTS)
  if(LONEJSON_BASH_EXECUTABLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LonejsonLinuxPolicyTests.cmake")
    foreach(case native_lua_ctest native_host_workflows native_test_prerequisites host_policy_scope
        selected_release_build package_verify_directory required_tool_discovery
        release_archive_reproducibility)
      add_test(NAME lonejson_${case}_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${case}.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}")
    endforeach()
    add_test(NAME lonejson_release_identity_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_identity.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME lonejson_lifecycle_contract_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lifecycle_contract.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME lonejson_curl_rewind_fixture_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_curl_rewind_fixture.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_makefile_fuzz_targets_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_makefile_fuzz_targets.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_fuzz_affinity_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fuzz_affinity.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_native_darwin_toolchain_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_native_darwin_toolchain.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_lifecycle_preset_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lifecycle_presets.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_clean_safety_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_clean_safety.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_lifecycle_make_surface_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lifecycle_make_surface.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_e2e_keep_services_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_e2e_keep_services.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    # The rootless pod fixture uses Linux host networking and util-linux tools.
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
      add_test(
        NAME lonejson_podman_kube_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_devenv.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    add_test(
      NAME lonejson_darwin_macho_metadata_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_darwin_macho_metadata.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_darwin_linker_route_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_darwin_linker_route.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_path_index_formatting_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_path_index_formatting.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_cpkt_fetch_retry_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fetch_cpkt.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_dependency_archive_cache_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dependency_archive_cache.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_cmake_threads_optional_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cmake_threads_optional.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_test_all_hardening_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_test_all_hardening.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_msan_handle_probe_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_msan_handle_probes.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_oidc_pkce_provider_no_openssl_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_oidc_pkce_provider_no_openssl.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_release_werror_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_werror.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_COMMAND}"
        "${CMAKE_GENERATOR}")
    add_test(
      NAME lonejson_source_release_tarball_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_source_release_tarball.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_COMMAND}")
    add_test(
      NAME lonejson_lua_src_rock_privacy_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_src_rock_privacy.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_lua55_resolution_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua55_resolution.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_lua_public_boundary_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_public_boundary.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_lua_surface_coverage_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_surface_coverage.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_lua_source_stage_manifest_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_source_stage_manifest.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_release_artifact_verify_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_artifact_verify.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_run_release_matrix_darwin_target_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_run_release_matrix_darwin_target.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_lifecycle_script_safety_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lifecycle_script_safety.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_build_workspace_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_build_workspaces.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_release_version_detection_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_version_detection.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_release_version_configuration_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_version_configuration.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_release_pipeline_makefile_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_pipeline_makefile.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_downstream_abi_report_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_downstream_abi_report.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_release_checksum_manifest_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_checksum_manifest.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_ctest_metadata_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ctest_metadata.sh"
        "${CMAKE_CURRENT_BINARY_DIR}")
    get_property(lonejson_all_tests DIRECTORY PROPERTY TESTS)
    foreach(test IN LISTS lonejson_all_tests)
      if(NOT test IN_LIST lonejson_runtime_tests AND NOT test STREQUAL "lonejson_ctest_metadata_tests")
        set_tests_properties("${test}" PROPERTIES LABELS "local;host-policy")
      endif()
    endforeach()
  endif()

  add_executable(lonejson_short_names_tests tests/test_short_names.c)
  target_compile_definitions(lonejson_short_names_tests PRIVATE
    LONEJSON_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_include_directories(lonejson_short_names_tests PRIVATE
    "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
  add_dependencies(lonejson_short_names_tests package-single-header)
  lonejson_configure_c_target(lonejson_short_names_tests)
  add_test(NAME lonejson_short_names_tests COMMAND lonejson_short_names_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_short_names_tests)

  add_executable(lonejson_short_names_disabled_tests
    tests/test_short_names_disabled.c)
  target_include_directories(lonejson_short_names_disabled_tests PRIVATE
    "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
  add_dependencies(lonejson_short_names_disabled_tests package-single-header)
  lonejson_configure_c_target(lonejson_short_names_disabled_tests)
  add_test(NAME lonejson_short_names_disabled_tests
    COMMAND lonejson_short_names_disabled_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_short_names_disabled_tests)

  add_executable(lonejson_single_header_strict_warning_tests
    tests/test_single_header_strict_warnings.c)
  target_include_directories(lonejson_single_header_strict_warning_tests PRIVATE
    "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
  add_dependencies(lonejson_single_header_strict_warning_tests
    package-single-header)
  lonejson_configure_strict_downstream_target(
    lonejson_single_header_strict_warning_tests)
  add_test(NAME lonejson_single_header_strict_warning_tests
    COMMAND lonejson_single_header_strict_warning_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS
    lonejson_single_header_strict_warning_tests)
  add_test(
    NAME lonejson_single_header_strict_warning_build_tests
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_CURRENT_BINARY_DIR}"
      --target lonejson_single_header_strict_warning_tests)
  set_tests_properties(lonejson_single_header_strict_warning_build_tests
    PROPERTIES RUN_SERIAL TRUE)

  set(LONEJSON_SINGLE_HEADER_STRICT_TOOLCHAIN_OBJECT
    "${CMAKE_CURRENT_BINARY_DIR}/generated/lonejson_single_header_strict_toolchain.o")
  add_custom_command(
    OUTPUT "${LONEJSON_SINGLE_HEADER_STRICT_TOOLCHAIN_OBJECT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
      "${CMAKE_CURRENT_BINARY_DIR}/generated"
    # This must follow the configured toolchain rather than a host compiler.
    COMMAND "${CMAKE_C_COMPILER}"
      ${LONEJSON_STRICT_DOWNSTREAM_WARNING_FLAGS}
      -D_POSIX_C_SOURCE=200809L
      -D_FILE_OFFSET_BITS=64
      -I "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}"
      -c "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_single_header_strict_warnings.c"
      -o "${LONEJSON_SINGLE_HEADER_STRICT_TOOLCHAIN_OBJECT}"
    DEPENDS
      package-single-header
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_single_header_strict_warnings.c"
    VERBATIM)
  add_custom_target(lonejson_single_header_strict_toolchain ALL
    DEPENDS "${LONEJSON_SINGLE_HEADER_STRICT_TOOLCHAIN_OBJECT}")
  add_test(
    NAME lonejson_single_header_strict_toolchain_build_tests
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_CURRENT_BINARY_DIR}"
      --target lonejson_single_header_strict_toolchain)
  set_tests_properties(lonejson_single_header_strict_toolchain_build_tests
    PROPERTIES RUN_SERIAL TRUE)

  function(lonejson_add_single_header_config_test target)
    add_executable(${target} tests/test_single_header_config_matrix.c)
    target_include_directories(${target} PRIVATE
      "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
    add_dependencies(${target} package-single-header)
    if(ARGN)
      target_compile_definitions(${target} PRIVATE ${ARGN})
    endif()
    lonejson_configure_c_target(${target})
    add_test(NAME ${target} COMMAND ${target})
    list(APPEND LONEJSON_STACK_USAGE_TARGETS ${target})
    set(LONEJSON_STACK_USAGE_TARGETS "${LONEJSON_STACK_USAGE_TARGETS}"
      PARENT_SCOPE)
  endfunction()

  lonejson_add_single_header_config_test(
    lonejson_single_header_config_default)
  lonejson_add_single_header_config_test(
    lonejson_single_header_config_omit_protocol
    TEST_OMIT_PROTOCOL_FRAMING)
  lonejson_add_single_header_config_test(
    lonejson_single_header_config_lj_implementation
    TEST_LJ_IMPLEMENTATION_ALIAS
    TEST_USE_SHORT_NAMES)
  lonejson_add_single_header_config_test(
    lonejson_single_header_config_lj_config_aliases
    TEST_LJ_CONFIG_ALIASES
    TEST_USE_SHORT_NAMES)
  lonejson_add_single_header_config_test(
    lonejson_single_header_config_short_names_disabled
    TEST_DISABLE_SHORT_NAMES)

  add_executable(lonejson_static_link_tests tests/test_link_consumer.c)
  target_include_directories(lonejson_static_link_tests PRIVATE
    "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_static_link_tests PRIVATE lonejson_static)
  lonejson_configure_c_target(lonejson_static_link_tests)
  add_test(NAME lonejson_static_link_tests COMMAND lonejson_static_link_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_static_link_tests)

  add_executable(lonejson_shared_link_tests tests/test_link_consumer.c)
  target_include_directories(lonejson_shared_link_tests PRIVATE
    "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_shared_link_tests PRIVATE lonejson_shared)
  lonejson_configure_c_target(lonejson_shared_link_tests)
  add_test(NAME lonejson_shared_link_tests COMMAND lonejson_shared_link_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_shared_link_tests)

  if(UNIX AND NOT APPLE)
    add_test(
      NAME lonejson_shared_soversion_tests
      COMMAND ${CMAKE_COMMAND}
        -DLONEJSON_SONAME_FILE_NAME=$<TARGET_SONAME_FILE_NAME:lonejson_shared>
        -DLONEJSON_ABI_VERSION=${LONEJSON_ABI_VERSION}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_soversion.cmake)
  endif()

  add_test(
    NAME lonejson_single_header_version_tests
    COMMAND ${CMAKE_COMMAND}
      -DLONEJSON_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DLONEJSON_PUBLIC_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h
      -DLONEJSON_SINGLE_HEADER_BUILD=${LONEJSON_SINGLE_HEADER_BUILD}
      -DLONEJSON_SINGLE_HEADER_BUILD_ALIAS=${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_single_header_version.cmake)
  add_test(
    NAME lonejson_header_abi_version_tests
    COMMAND ${CMAKE_COMMAND}
      -DLONEJSON_PUBLIC_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h
      -DLONEJSON_ABI_VERSION=${LONEJSON_ABI_VERSION}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_header_abi_version.cmake)
  add_test(
    NAME lonejson_single_header_release_version_tests
    COMMAND ${CMAKE_COMMAND}
      -DLONEJSON_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DLONEJSON_RELEASE_VERSION=${LONEJSON_RESOLVED_VERSION}
      -DLONEJSON_SINGLE_HEADER_DIST_GZ=${LONEJSON_SINGLE_HEADER_DIST_GZ}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_single_header_release_version.cmake)

  get_property(lonejson_registered_tests DIRECTORY PROPERTY TESTS)
  foreach(lonejson_registered_test IN LISTS lonejson_registered_tests)
    get_property(lonejson_test_has_timeout TEST
      "${lonejson_registered_test}" PROPERTY TIMEOUT SET)
    if(NOT lonejson_test_has_timeout)
      set_property(TEST "${lonejson_registered_test}" PROPERTY TIMEOUT
        "${LONEJSON_TEST_TIMEOUT}")
    endif()
    get_property(lonejson_test_has_labels TEST
      "${lonejson_registered_test}" PROPERTY LABELS SET)
    if(NOT lonejson_test_has_labels)
      set_property(TEST "${lonejson_registered_test}" PROPERTY LABELS local)
    endif()
  endforeach()
endif()

if(LONEJSON_BUILD_EXAMPLES)
  set(LONEJSON_LINKED_EXAMPLES
    array_stream
    generator_pull
    json_value_buffer
    json_value_nested_parse
    json_value_parse
    json_value_source
    sse_json_value_stream
    json_value_visitor
    nullable_fields
    parse_file
    parse_reader
    push_parser
    spooled_text
    spooled_bytes
    source_text
    source_bytes
    serialize_string
    serialize_file
    value_rewrite_replace_with
    fixed_storage)
  foreach(example_name IN LISTS LONEJSON_LINKED_EXAMPLES)
    add_executable("example_${example_name}" "examples/${example_name}.c")
    target_include_directories("example_${example_name}" PRIVATE
      "${LONEJSON_PUBLIC_INCLUDE_DIR}")
    target_link_libraries("example_${example_name}" PRIVATE lonejson)
    lonejson_configure_c_target("example_${example_name}")
    list(APPEND LONEJSON_STACK_USAGE_TARGETS "example_${example_name}")
  endforeach()

  set(LONEJSON_SINGLE_HEADER_EXAMPLES
    parse_string
    serialize_jsonl)
  foreach(example_name IN LISTS LONEJSON_SINGLE_HEADER_EXAMPLES)
    add_executable("example_${example_name}" "examples/${example_name}.c")
    target_include_directories("example_${example_name}" PRIVATE
      "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
    add_dependencies("example_${example_name}" package-single-header)
    lonejson_configure_c_target("example_${example_name}")
    list(APPEND LONEJSON_STACK_USAGE_TARGETS "example_${example_name}")
  endforeach()

  if(LONEJSON_BUILD_WITH_CURL)
    foreach(example_name curl_get curl_put)
      add_executable("example_${example_name}" "examples/${example_name}.c")
      target_include_directories("example_${example_name}" PRIVATE
        "${LONEJSON_PUBLIC_INCLUDE_DIR}")
      target_compile_definitions("example_${example_name}" PRIVATE
        LONEJSON_WITH_CURL)
      lonejson_target_include_curl("example_${example_name}")
      target_link_libraries("example_${example_name}" PRIVATE lonejson CURL::libcurl)
      lonejson_configure_c_target("example_${example_name}")
      list(APPEND LONEJSON_STACK_USAGE_TARGETS "example_${example_name}")
    endforeach()
  endif()
endif()

if(LONEJSON_BUILD_BENCHMARKS)
  add_executable(lonejson_bench bench/lonejson_bench.c)
  if(LONEJSON_BOOTLIN_TOOLCHAIN_ROOT)
    get_filename_component(_lonejson_bench_collection
      "${LONEJSON_BOOTLIN_TOOLCHAIN_ROOT}" NAME)
    set(_lonejson_bench_toolchain "bootlin:${_lonejson_bench_collection}")
  elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_CROSSCOMPILING)
    set(_lonejson_bench_toolchain "apple:${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
  else()
    set(_lonejson_bench_toolchain "osxcross:${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
  endif()
  target_compile_definitions(lonejson_bench PRIVATE
    "LONEJSON_BENCH_TOOLCHAIN=\"${_lonejson_bench_toolchain}\"")
  target_include_directories(lonejson_bench PRIVATE
    "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
  # Keep the core JSON benchmark focused on JSON hot paths; protocol framing has
  # separate unit/fuzz coverage and would otherwise perturb single-header layout.
  target_compile_definitions(lonejson_bench PRIVATE
    LONEJSON_IMPLEMENTATION
    LONEJSON__OMIT_PROTOCOL_FRAMING_IMPL)
  add_dependencies(lonejson_bench package-single-header)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lonejson_bench PRIVATE -Wno-long-long)
  endif()
  lonejson_configure_c_target(lonejson_bench)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_bench)
  add_test(
    NAME lonejson_bench_gate_tests
    COMMAND ${CMAKE_COMMAND}
      -DLONEJSON_BENCH_EXE=$<TARGET_FILE:lonejson_bench>
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bench_gate.cmake)
  set_tests_properties(lonejson_bench_gate_tests PROPERTIES
    LABELS local
    TIMEOUT 120)
endif()

if(LONEJSON_BUILD_FUZZERS)
  if(NOT LONEJSON_FUZZ_SECONDS MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "LONEJSON_FUZZ_SECONDS must be a positive integer")
  endif()
  if(NOT LONEJSON_AFL_FUZZ OR NOT EXISTS "${LONEJSON_AFL_FUZZ}")
    message(FATAL_ERROR "Fuzzing requires the pinned AFL++ toolchain; configure with --preset fuzz")
  endif()
  enable_testing()
  add_test(NAME lonejson_fuzz_instrumentation_tests COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_COMPILE_COMMANDS=${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
    -DLONEJSON_SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/src/lonejson.c
    -DLONEJSON_AFL_COMPILER=${CMAKE_C_COMPILER}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_fuzz_instrumentation.cmake)
  add_test(NAME lonejson_fuzz_crash_handling_tests COMMAND bash
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fuzz_crash_handling.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}")
  set_tests_properties(lonejson_fuzz_instrumentation_tests lonejson_fuzz_crash_handling_tests
    PROPERTIES LABELS "local;fuzz" TIMEOUT 30 FIXTURES_SETUP lonejson_fuzz_checks)
  math(EXPR lonejson_fuzz_timeout "${LONEJSON_FUZZ_SECONDS} + 120")
  add_custom_target(lonejson_fuzz_targets)
  add_library(lonejson_afl_driver STATIC fuzz/afl_driver.c)
  lonejson_configure_c_target(lonejson_afl_driver)
  # AFL++ instruments through its pinned GCC wrapper. Existing callbacks use
  # a small native file-input driver rather than an LLVM runtime.
  function(lonejson_add_afl_fuzz_target target source name)
    add_executable("${target}" "${source}")
    target_include_directories("${target}" PRIVATE
      "${LONEJSON_PUBLIC_INCLUDE_DIR}")
    target_link_libraries("${target}" PRIVATE lonejson lonejson_afl_driver)
    target_compile_options("${target}" PRIVATE -fno-omit-frame-pointer)
    lonejson_configure_c_target("${target}")
    add_dependencies(lonejson_fuzz_targets "${target}")
    set(seeds "")
    foreach(seed IN LISTS ARGN)
      list(APPEND seeds "${CMAKE_CURRENT_SOURCE_DIR}/${seed}")
    endforeach()
    add_test(NAME ${target}_tests COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/fuzz.sh"
      "${LONEJSON_AFL_FUZZ}" "${LONEJSON_FUZZ_SECONDS}" "${name}"
      "$<TARGET_FILE:${target}>" ${seeds})
    set_tests_properties(${target}_tests PROPERTIES LABELS "local;fuzz"
      TIMEOUT "${lonejson_fuzz_timeout}" FIXTURES_REQUIRED lonejson_fuzz_checks)
  endfunction()

  lonejson_add_afl_fuzz_target(lonejson_fuzz_base64 fuzz/fuzz_base64.c base64 fuzz/corpus/base64)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_validate fuzz/fuzz_validate.c validate
    tests/fixtures/vendor/json_test_suite/test_parsing tests/fixtures/spec tests/fixtures/languages)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_mapped_parse fuzz/fuzz_mapped_parse.c mapped
    fuzz/corpus/mapped tests/fixtures/spec)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_array_stream fuzz/fuzz_array_stream.c array
    fuzz/corpus/array_stream fuzz/corpus/mapped)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_json_value fuzz/fuzz_json_value.c json
    fuzz/corpus/json_value fuzz/corpus/mapped)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_value_visitor fuzz/fuzz_value_visitor.c visitor
    fuzz/corpus/value_visitor fuzz/corpus/json_value)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_path_value_visitor
    fuzz/fuzz_path_value_visitor.c path fuzz/corpus/path_value_visitor fuzz/corpus/value_visitor)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_value_rewrite fuzz/fuzz_value_rewrite.c rewrite
    fuzz/corpus/value_rewrite fuzz/corpus/value_visitor)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_reader_stream_generator
    fuzz/fuzz_reader_stream_generator.c reader fuzz/corpus/mapped tests/fixtures/spec)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_writer_generator_backpressure
    fuzz/fuzz_writer_generator_backpressure.c writer fuzz/corpus/mapped fuzz/corpus/json_value)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_writer_value_stream
    fuzz/fuzz_writer_value_stream.c writer-value-stream fuzz/corpus/json_value fuzz/corpus/value_visitor)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_protocol_framing
    fuzz/fuzz_protocol_framing.c protocol fuzz/corpus/protocol_framing)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_fixed_string_paths
    fuzz/fuzz_fixed_string_paths.c fixed fuzz/corpus/fixed_string_paths)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_alloc_ceiling
    fuzz/fuzz_alloc_ceiling.c alloc fuzz/corpus/alloc_ceiling)

  lonejson_add_afl_fuzz_target(lonejson_fuzz_parser_boundaries
    fuzz/fuzz_parser_boundaries.c parser fuzz/corpus/parser_boundaries)

  if(LONEJSON_BUILD_WITH_CURL AND LONEJSON_BUILD_WITH_OPENSSL AND
      LONEJSON_BUILD_WITH_JWT AND LONEJSON_BUILD_WITH_OIDC)
    lonejson_add_afl_fuzz_target(lonejson_fuzz_jwt fuzz/fuzz_jwt.c jwt fuzz/corpus/jwt)
    lonejson_target_include_curl(lonejson_fuzz_jwt)
    lonejson_target_include_openssl(lonejson_fuzz_jwt)
    target_link_libraries(lonejson_fuzz_jwt PRIVATE OpenSSL::Crypto)
    target_compile_definitions(lonejson_fuzz_jwt PRIVATE
      LONEJSON_WITH_CURL
      LONEJSON_WITH_OPENSSL
      LONEJSON_WITH_JWT
      LONEJSON_WITH_OIDC)
  endif()
endif()

if(LONEJSON_BUILD_TESTS AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
    NOT CMAKE_CROSSCOMPILING)
  # Keep this after every executable declaration. A new local executable
  # fails configuration until it records the pinned Bootlin runtime.
  lonejson_verify_development_runtime_coverage()
  get_property(_lonejson_runtime_targets DIRECTORY
    PROPERTY BUILDSYSTEM_TARGETS)
  set(_lonejson_runtime_test_targets)
  foreach(_lonejson_runtime_target IN LISTS _lonejson_runtime_targets)
    get_target_property(_lonejson_runtime_target_type
      ${_lonejson_runtime_target} TYPE)
    if(_lonejson_runtime_target_type STREQUAL "EXECUTABLE")
      list(APPEND _lonejson_runtime_test_targets
        "$<TARGET_FILE:${_lonejson_runtime_target}>")
    endif()
  endforeach()
  if(_lonejson_runtime_test_targets)
    add_test(NAME lonejson_bootlin_runtime_target_coverage_tests
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bootlin_runtime_targets.sh"
        "${CMAKE_CURRENT_BINARY_DIR}" ${_lonejson_runtime_test_targets})
    set_tests_properties(lonejson_bootlin_runtime_target_coverage_tests
      PROPERTIES LABELS "smoke;local" TIMEOUT 60)
  endif()
endif()

file(GLOB_RECURSE LONEJSON_FORMAT_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/bench/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/bench/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/*.c")
if(CLANG_FORMAT_BIN)
  add_custom_target(format
    COMMAND "${CLANG_FORMAT_BIN}" -i ${LONEJSON_FORMAT_SOURCES} VERBATIM)
  add_custom_target(format-check
    COMMAND "${CLANG_FORMAT_BIN}" --dry-run --Werror ${LONEJSON_FORMAT_SOURCES} VERBATIM)
else()
  foreach(target format format-check)
    add_custom_target(${target}
      COMMAND "${CMAKE_COMMAND}" -E echo "clang-format not found; ${target} unavailable"
      COMMAND "${CMAKE_COMMAND}" -E false VERBATIM)
  endforeach()
endif()

add_custom_target(stack-usage-report
  COMMAND "${LONEJSON_LUA_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/report_stack_usage.lua" "${CMAKE_CURRENT_BINARY_DIR}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  VERBATIM)
if(LONEJSON_STACK_USAGE_TARGETS)
  add_dependencies(stack-usage-report ${LONEJSON_STACK_USAGE_TARGETS})
endif()

install(FILES include/lonejson.h DESTINATION include)
install(TARGETS lonejson_shared
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
)
install(TARGETS lonejson_static
  ARCHIVE DESTINATION lib
)

add_custom_target(package-archive
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_DIST_DIR=${LONEJSON_DIST_DIR}
    -DLONEJSON_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -DLONEJSON_VERSION=${LONEJSON_RESOLVED_VERSION}
    -DLONEJSON_TARGET_ID=${LONEJSON_TARGET_ID}
    -DLONEJSON_PUBLIC_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h
    -DLONEJSON_STATIC_LIB=$<TARGET_FILE:lonejson_static>
    -DLONEJSON_SHARED_LIB=$<TARGET_FILE:lonejson_shared>
    -DLONEJSON_STATIC_LIB_NAME=$<TARGET_FILE_NAME:lonejson_static>
    -DLONEJSON_SHARED_LINK_NAME=$<TARGET_LINKER_FILE_NAME:lonejson_shared>
    -DLONEJSON_SHARED_LIB_NAME=$<TARGET_FILE_NAME:lonejson_shared>
    -DLONEJSON_SHARED_SONAME=$<TARGET_SONAME_FILE_NAME:lonejson_shared>
    -DLONEJSON_BUILD_WITH_CURL=${LONEJSON_BUILD_WITH_CURL}
    -DLONEJSON_BUILD_WITH_OPENSSL=${LONEJSON_BUILD_WITH_OPENSSL}
    -DLONEJSON_BUILD_WITH_JWT=${LONEJSON_BUILD_WITH_JWT}
    -DLONEJSON_BUILD_WITH_OIDC=${LONEJSON_BUILD_WITH_OIDC}
    -DLONEJSON_STRIP=${CMAKE_STRIP}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_archive.cmake
  DEPENDS lonejson_shared lonejson_static
  VERBATIM
)

add_custom_target(package-darwin-smoke-bundle
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_DIST_DIR=${LONEJSON_DIST_DIR}
    -DLONEJSON_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -DLONEJSON_VERSION=${LONEJSON_RESOLVED_VERSION}
    -DLONEJSON_TARGET_ID=${LONEJSON_TARGET_ID}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_darwin_smoke_bundle.cmake
  DEPENDS package-archive
  VERBATIM
)
foreach(example_name
        array_stream
        fixed_storage
        generator_pull
        json_value_buffer
        sse_json_value_stream
        json_value_parse
        json_value_source
        json_value_visitor
        nullable_fields
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
        spooled_text
        value_rewrite_replace_with)
  if(TARGET "example_${example_name}")
    add_dependencies(package-darwin-smoke-bundle "example_${example_name}")
  endif()
endforeach()

add_custom_target(package-source
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_DIST_DIR=${LONEJSON_DIST_DIR}
    -DLONEJSON_VERSION=${LONEJSON_RESOLVED_VERSION}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_source.cmake
  VERBATIM
)

add_custom_command(
  OUTPUT
    ${LONEJSON_SINGLE_HEADER_BUILD}
    ${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}
    ${LONEJSON_SINGLE_HEADER_DIST_GZ}
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_DIST_DIR=${LONEJSON_DIST_DIR}
    -DLONEJSON_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -DLONEJSON_VERSION=${LONEJSON_RESOLVED_VERSION}
    -DLONEJSON_PUBLIC_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h
    -DLONEJSON_INTERNAL_IMPL=${CMAKE_CURRENT_SOURCE_DIR}/src/lonejson_impl.h
    -DLONEJSON_SINGLE_HEADER_BUILD=${LONEJSON_SINGLE_HEADER_BUILD}
    -DLONEJSON_SINGLE_HEADER_BUILD_ALIAS=${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}
    -DLONEJSON_SINGLE_HEADER_DIST_GZ=${LONEJSON_SINGLE_HEADER_DIST_GZ}
    -DLONEJSON_CLANG_FORMAT_BIN=${CLANG_FORMAT_BIN}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_single_header.cmake
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lonejson_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/00_prelude.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/09_base64.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/10_source_runtime.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/11_json_value_io.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/12_json_value_parse.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/13_json_value_visit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/14_json_value_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/20_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/20_parser_map.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/21_parser_values.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/22_parser_actions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/22_parser_engine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/23_serializer_io.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/24_serializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/25_json_pull.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/30_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/32_generator_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/33_array_stream_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/34_protocol_framing_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/35_json_pull_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/36_curl_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/37_array_rewrite_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/38_writer_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/39_value_rewrite_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/40_auth_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_single_header.cmake
  VERBATIM
)

add_custom_target(package-single-header
  DEPENDS
    ${LONEJSON_SINGLE_HEADER_BUILD}
    ${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}
    ${LONEJSON_SINGLE_HEADER_DIST_GZ}
)

add_custom_target(package-checksums
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_DIST_DIR=${LONEJSON_DIST_DIR}
    -DLONEJSON_VERSION=${LONEJSON_RESOLVED_VERSION}
    "-DLONEJSON_RELEASE_TARGETS=${LONEJSON_RELEASE_TARGETS}"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_checksums.cmake
  VERBATIM
)

add_custom_target(package-binary-checksums
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_DIST_DIR=${LONEJSON_DIST_DIR}
    -DLONEJSON_VERSION=${LONEJSON_RESOLVED_VERSION}
    "-DLONEJSON_RELEASE_TARGETS=${LONEJSON_RELEASE_TARGETS}"
    -DLONEJSON_CHECKSUM_SCOPE=binary
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_checksums.cmake
  VERBATIM)

add_custom_target(package-clean-dist
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_DIST_DIR=${LONEJSON_DIST_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_clean_dist.cmake
  VERBATIM
)
