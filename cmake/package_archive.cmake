if(NOT LONEJSON_BUILD_WITH_CURL)
  message(FATAL_ERROR
    "package-archive requires LONEJSON_BUILD_WITH_CURL=ON; binary releases "
    "must include the lonejson_curl_* ABI")
endif()
if(NOT LONEJSON_BUILD_WITH_OPENSSL)
  message(FATAL_ERROR
    "package-archive requires LONEJSON_BUILD_WITH_OPENSSL=ON; binary releases "
    "must expose the OpenSSL provider integration surface")
endif()
if(NOT LONEJSON_BUILD_WITH_JWT)
  message(FATAL_ERROR
    "package-archive requires LONEJSON_BUILD_WITH_JWT=ON; binary releases "
    "must include the lonejson_jwt_* ABI")
endif()
if(NOT LONEJSON_BUILD_WITH_OIDC)
  message(FATAL_ERROR
    "package-archive requires LONEJSON_BUILD_WITH_OIDC=ON; binary releases "
    "must include the lonejson_oidc_* ABI")
endif()

include("${LONEJSON_ROOT}/cmake/lonejson_build_workspace.cmake")
set(package_stage_root "${LONEJSON_BINARY_DIR}/package/archive")
lonejson_require_build_workspace("${package_stage_root}")

include("${LONEJSON_ROOT}/cmake/lonejson_target_tools.cmake")
lonejson_discover_target_tools()

set(archive_name "liblonejson-${LONEJSON_VERSION}-${LONEJSON_TARGET_ID}")
set(package_root "${package_stage_root}/${archive_name}")
file(REMOVE_RECURSE "${package_stage_root}")
include("${LONEJSON_ROOT}/cmake/cpkt_metadata.cmake")
lonejson_cpkt_sha256("${LONEJSON_TARGET_ID}" cpkt_sha256)
set(cpkt_url
    "${LONEJSON_CPKT_BASE_URL_DEFAULT}/cpkt-${LONEJSON_CPKT_PINNED_VERSION}-${LONEJSON_TARGET_ID}.tar.gz")
file(MAKE_DIRECTORY "${package_root}/include")
file(MAKE_DIRECTORY "${package_root}/lib")
file(MAKE_DIRECTORY "${package_root}/lib/cmake/lonejson")
file(MAKE_DIRECTORY "${package_root}/lib/pkgconfig")
file(MAKE_DIRECTORY "${package_root}/share/lonejson")
file(MAKE_DIRECTORY "${package_root}/share/doc/liblonejson")

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${LONEJSON_BINARY_DIR}"
    --prefix "${package_root}" --config Release
  RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "failed to stage SDK install tree")
endif()

set(packaged_shared_lib "${package_root}/lib/${LONEJSON_SHARED_LIB_NAME}")
set(packaged_static_lib "${package_root}/lib/${LONEJSON_STATIC_LIB_NAME}")
if(DEFINED LONEJSON_STRIP AND NOT LONEJSON_STRIP STREQUAL "")
  if(NOT LONEJSON_TARGET_ID MATCHES "darwin")
    set(strip_shared_flag "--strip-unneeded")
    set(strip_static_flag "--strip-debug")
    execute_process(
      COMMAND "${LONEJSON_STRIP}" "${strip_shared_flag}" "${packaged_shared_lib}"
      RESULT_VARIABLE strip_shared_result
    )
    if(NOT strip_shared_result EQUAL 0)
      message(FATAL_ERROR "failed to strip ${packaged_shared_lib}")
    endif()
    execute_process(
      COMMAND "${LONEJSON_STRIP}" --enable-deterministic-archives
        "${strip_static_flag}" "${packaged_static_lib}"
      RESULT_VARIABLE strip_static_result
    )
    if(NOT strip_static_result EQUAL 0)
      message(FATAL_ERROR "failed to strip ${packaged_static_lib}")
    endif()
  endif()
endif()

if(DEFINED LONEJSON_SHARED_LINK_NAME
   AND NOT LONEJSON_SHARED_LINK_NAME STREQUAL ""
   AND NOT LONEJSON_SHARED_LINK_NAME STREQUAL LONEJSON_SHARED_LIB_NAME)
  file(CREATE_LINK "${LONEJSON_SHARED_LIB_NAME}"
    "${package_root}/lib/${LONEJSON_SHARED_LINK_NAME}"
    SYMBOLIC)
endif()

if(DEFINED LONEJSON_SHARED_SONAME
   AND DEFINED LONEJSON_SHARED_LIB_NAME
   AND NOT LONEJSON_SHARED_SONAME STREQUAL ""
   AND NOT LONEJSON_SHARED_SONAME STREQUAL LONEJSON_SHARED_LIB_NAME)
  file(CREATE_LINK "${LONEJSON_SHARED_LIB_NAME}"
    "${package_root}/lib/${LONEJSON_SHARED_SONAME}"
    SYMBOLIC)
endif()

file(COPY "${LONEJSON_ROOT}/LICENSE" DESTINATION "${package_root}/share/doc/liblonejson")
file(COPY "${LONEJSON_ROOT}/README.md" DESTINATION "${package_root}/share/doc/liblonejson")

set(dependencies_file "${package_root}/share/lonejson/dependencies.json")
set(dependency_entries "")
foreach(component IN ITEMS curl openssl)
  if(component STREQUAL "curl")
    set(component_version "${LONEJSON_CPKT_CURL_VERSION}")
    set(component_license "curl")
  else()
    set(component_version "${LONEJSON_CPKT_OPENSSL_VERSION}")
    set(component_license "Apache-2.0")
  endif()
  string(APPEND dependency_entries ",
    {
      \"name\": \"${component}\",
      \"version\": \"${component_version}\",
      \"target_id\": \"${LONEJSON_TARGET_ID}\",
      \"source_system\": \"cpkt\",
      \"source_url\": \"${cpkt_url}\",
      \"sha256\": \"${cpkt_sha256}\",
      \"archive_name\": \"cpkt-${LONEJSON_CPKT_PINNED_VERSION}-${LONEJSON_TARGET_ID}.tar.gz\",
      \"license\": \"${component_license}\",
      \"bundled\": false,
      \"external\": true,
      \"role\": \"optional-facade-consumer-requirement\"
    }")
endforeach()
file(WRITE "${dependencies_file}"
"{
  \"schema\": \"pkt.systems.dependencies.v1\",
  \"project\": \"lonejson\",
  \"version\": \"${LONEJSON_VERSION}\",
  \"target_id\": \"${LONEJSON_TARGET_ID}\",
  \"dependencies\": [
    {
      \"name\": \"cpkt\",
      \"version\": \"${LONEJSON_CPKT_PINNED_VERSION}\",
      \"target_id\": \"${LONEJSON_TARGET_ID}\",
      \"source_system\": \"GitHub release\",
      \"source_url\": \"${cpkt_url}\",
      \"sha256\": \"${cpkt_sha256}\",
      \"archive_name\": \"cpkt-${LONEJSON_CPKT_PINNED_VERSION}-${LONEJSON_TARGET_ID}.tar.gz\",
      \"license\": \"MIT\",
      \"bundled\": false,
      \"external\": false,
      \"role\": \"release-sdk-build-input\",
      \"provides\": [\"curl\", \"openssl\"]
    }${dependency_entries}
  ]
}")

set(pkgconfig_file "${package_root}/lib/pkgconfig/lonejson.pc")
file(WRITE "${pkgconfig_file}"
"prefix=\${pcfiledir}/../..
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: lonejson
Description: Strict C89 JSON parser, serializer, and streaming toolkit
Version: ${LONEJSON_VERSION}
Libs: -L\${libdir} -llonejson
Cflags: -I\${includedir}
")

set(pkgconfig_jwt_file "${package_root}/lib/pkgconfig/lonejson-jwt.pc")
file(WRITE "${pkgconfig_jwt_file}"
"prefix=\${pcfiledir}/../..
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: lonejson-jwt
Description: lonejson JWT/JWK/JWKS public API opt-in
Version: ${LONEJSON_VERSION}
Requires: lonejson
Cflags: -DLONEJSON_WITH_JWT
")

set(pkgconfig_oidc_file "${package_root}/lib/pkgconfig/lonejson-oidc.pc")
file(WRITE "${pkgconfig_oidc_file}"
"prefix=\${pcfiledir}/../..
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: lonejson-oidc
Description: lonejson OAuth2/OIDC public API opt-in
Version: ${LONEJSON_VERSION}
Requires: lonejson-jwt
Cflags: -DLONEJSON_WITH_OIDC
")

set(pkgconfig_curl_file "${package_root}/lib/pkgconfig/lonejson-curl.pc")
file(WRITE "${pkgconfig_curl_file}"
"prefix=\${pcfiledir}/../..
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: lonejson-curl
Description: lonejson libcurl adapter opt-in
Version: ${LONEJSON_VERSION}
Requires: lonejson libcurl
Cflags: -DLONEJSON_WITH_CURL
")

set(pkgconfig_openssl_file "${package_root}/lib/pkgconfig/lonejson-openssl.pc")
file(WRITE "${pkgconfig_openssl_file}"
"prefix=\${pcfiledir}/../..
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: lonejson-openssl
Description: lonejson OpenSSL auth provider opt-in
Version: ${LONEJSON_VERSION}
Requires: lonejson-jwt openssl
Cflags: -DLONEJSON_WITH_OPENSSL
")

set(cmake_config_file "${package_root}/lib/cmake/lonejson/lonejsonConfig.cmake")
file(WRITE "${cmake_config_file}"
"get_filename_component(_lonejson_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)

if(NOT TARGET lonejson::lonejson)
  add_library(lonejson::lonejson SHARED IMPORTED)
  set_target_properties(lonejson::lonejson PROPERTIES
    IMPORTED_LOCATION \"\${_lonejson_prefix}/lib/${LONEJSON_SHARED_LIB_NAME}\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${_lonejson_prefix}/include\"
  )
endif()

if(NOT TARGET lonejson::lonejson_static)
  add_library(lonejson::lonejson_static STATIC IMPORTED)
  set_target_properties(lonejson::lonejson_static PROPERTIES
    IMPORTED_LOCATION \"\${_lonejson_prefix}/lib/${LONEJSON_STATIC_LIB_NAME}\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${_lonejson_prefix}/include\"
  )
endif()

set(lonejson_jwt_FOUND TRUE)
set(lonejson_oidc_FOUND TRUE)
set(lonejson_curl_FOUND TRUE)
set(lonejson_openssl_FOUND TRUE)

if(lonejson_FIND_COMPONENTS)
  include(CMakeFindDependencyMacro)
endif()

if(NOT TARGET lonejson::jwt AND (NOT lonejson_FIND_COMPONENTS OR \"jwt\" IN_LIST lonejson_FIND_COMPONENTS OR \"oidc\" IN_LIST lonejson_FIND_COMPONENTS OR \"openssl\" IN_LIST lonejson_FIND_COMPONENTS))
  add_library(lonejson::jwt INTERFACE IMPORTED)
  set_target_properties(lonejson::jwt PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS LONEJSON_WITH_JWT
  )
endif()

if(NOT TARGET lonejson::oidc AND (NOT lonejson_FIND_COMPONENTS OR \"oidc\" IN_LIST lonejson_FIND_COMPONENTS))
  add_library(lonejson::oidc INTERFACE IMPORTED)
  set_target_properties(lonejson::oidc PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS LONEJSON_WITH_OIDC
    INTERFACE_LINK_LIBRARIES lonejson::jwt
  )
endif()

if(\"curl\" IN_LIST lonejson_FIND_COMPONENTS)
  find_dependency(CURL)
endif()
if(NOT TARGET lonejson::curl AND (\"curl\" IN_LIST lonejson_FIND_COMPONENTS OR TARGET CURL::libcurl))
  add_library(lonejson::curl INTERFACE IMPORTED)
  set_target_properties(lonejson::curl PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS LONEJSON_WITH_CURL
    INTERFACE_LINK_LIBRARIES CURL::libcurl
  )
endif()

if(\"openssl\" IN_LIST lonejson_FIND_COMPONENTS)
  find_dependency(OpenSSL)
endif()
if(NOT TARGET lonejson::openssl AND (\"openssl\" IN_LIST lonejson_FIND_COMPONENTS OR TARGET OpenSSL::Crypto))
  add_library(lonejson::openssl INTERFACE IMPORTED)
  set_target_properties(lonejson::openssl PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS LONEJSON_WITH_OPENSSL
    INTERFACE_LINK_LIBRARIES \"lonejson::jwt;OpenSSL::Crypto\"
  )
endif()

set(lonejson_FOUND TRUE)
set(lonejson_known_components jwt oidc curl openssl)
foreach(_lonejson_component IN LISTS lonejson_FIND_COMPONENTS)
  if(NOT _lonejson_component IN_LIST lonejson_known_components)
    set(lonejson_\${_lonejson_component}_FOUND FALSE)
    if(lonejson_FIND_REQUIRED_\${_lonejson_component})
      set(lonejson_FOUND FALSE)
    endif()
  endif()
endforeach()
unset(_lonejson_component)
unset(lonejson_known_components)
unset(_lonejson_prefix)
")

set(cmake_version_file "${package_root}/lib/cmake/lonejson/lonejsonConfigVersion.cmake")
file(WRITE "${cmake_version_file}"
"set(PACKAGE_VERSION \"${LONEJSON_VERSION}\")

if(PACKAGE_FIND_VERSION)
  if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
    set(PACKAGE_VERSION_EXACT TRUE)
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
  elseif(PACKAGE_FIND_VERSION VERSION_LESS PACKAGE_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
  else()
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
  endif()
endif()
")

include("${LONEJSON_ROOT}/cmake/lonejson_dist_dir.cmake")
lonejson_prepare_dist_dir()
set(archive_base "${package_stage_root}/${archive_name}.tar")
set(archive "${LONEJSON_DIST_DIR}/${archive_name}.tar.gz")

execute_process(
  COMMAND "${LONEJSON_ROOT}/scripts/archive_release_tree.sh"
    "${package_root}" "${archive_base}.gz"
  RESULT_VARIABLE tar_result
)
if(NOT tar_result EQUAL 0)
  message(FATAL_ERROR "failed to create package tar archive")
endif()

lonejson_publish_artifact("${archive_base}.gz" "${archive}")
