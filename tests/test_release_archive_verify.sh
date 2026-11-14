#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Rationale: binary SDK archives must be verified through extracted downstream
# CMake/pkg-config consumers so metadata drift is caught before upload.

repo_root=${1:?usage: test_release_archive_verify.sh REPO_ROOT}
toolchain_env=$("$repo_root/scripts/cpkt-toolchains.sh" env x86_64-linux-gnu)
eval "$toolchain_env"

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'missing required command for archive verification test: %s\n' "$1" >&2
    exit 77
  fi
}

require_command "$CC"
require_command "$AR"
require_command cmake
require_command ninja
require_command "$NM"
require_command perl
require_command pkg-config
require_command "$CPKT_TOOLCHAIN_READELF"
require_command sha256sum
require_command tar

tmp_dir="$(mktemp -d "$workspace_root/build/test_release_archive_verify.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

dist_dir="$tmp_dir/dist"
build_root="$tmp_dir/build"
package_root="$tmp_dir/package/liblonejson-9.9.9-x86_64-linux-gnu"
dependency_root="$tmp_dir/deps"
mkdir -p \
  "$dist_dir" \
  "$build_root/x86_64-linux-gnu-release" \
  "$dependency_root/lib/cmake/CURL" \
  "$dependency_root/lib/cmake/OpenSSL" \
  "$dependency_root/lib/pkgconfig" \
  "$package_root/include" \
  "$package_root/lib/cmake/lonejson" \
  "$package_root/lib/pkgconfig" \
  "$package_root/share/lonejson" \
  "$package_root/share/doc/liblonejson"

no_binary_dist_dir="$tmp_dir/no-binary-dist"
mkdir -p "$no_binary_dist_dir"
printf 'header\n' | gzip -9 >"$no_binary_dist_dir/lonejson-9.9.9.h.gz"
(cd "$no_binary_dist_dir" && sha256sum lonejson-9.9.9.h.gz >lonejson-9.9.9-CHECKSUMS)
no_binary_log="$tmp_dir/no-binary.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$no_binary_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$no_binary_log" 2>&1; then
  printf 'expected archive verification to fail with no binary SDK archives\n' >&2
  exit 1
fi
grep -F 'no binary SDK archives listed' "$no_binary_log" >/dev/null

cat >"$package_root/include/lonejson.h" <<'EOF'
#ifndef LONEJSON_H
#define LONEJSON_H

typedef struct lonejson lonejson;
typedef enum lonejson_status {
  LONEJSON_STATUS_OK = 0
} lonejson_status;
typedef struct lonejson_error {
  int code;
} lonejson_error;
typedef struct lonejson_auth_provider {
  void *user_data;
} lonejson_auth_provider;
typedef struct lonejson_curl_parse {
  int unused;
} lonejson_curl_parse;
typedef struct lonejson_oidc_pkce {
  int unused;
} lonejson_oidc_pkce;

void lonejson_error_init(lonejson_error *error);
lonejson *lonejson_new(const void *config, lonejson_error *error);
void lonejson_free(lonejson *runtime);
lonejson_status lonejson_validate_cstr(lonejson *runtime, const char *json, lonejson_error *error);
void lonejson_curl_parse_init(void);
void lonejson_curl_upload_is_rewindable(void);
void lonejson_curl_upload_rewind(void);
void lonejson_curl_seek_callback(void);
void lonejson_jwt_parse_compact(void);
void lonejson_jwk_parse_json(void);
void lonejson_jwks_parse_json(void);
void lonejson_jwks_select(void);
void lonejson_jwk_cleanup(void);
void lonejson_jwks_cleanup(void);
void lonejson_jwt_decode_compact(void);
void lonejson_jwt_validate_claims(void);
void lonejson_jwt_validate_signature(void);
void lonejson_jwt_header_cleanup(void);
void lonejson_jwt_claims_cleanup(void);
void lonejson_oidc_discovery_url(void);
void lonejson_oidc_discovery_parse_json(void);
void lonejson_oidc_discovery_validate_issuer(void);
void lonejson_oidc_discovery_cleanup(void);
void lonejson_oidc_jwks_cache_init(void);
void lonejson_oidc_jwks_cache_cleanup(void);
void lonejson_oidc_jwks_cache_update_json(void);
void lonejson_oidc_jwks_cache_is_fresh(void);
void lonejson_oidc_jwks_cache_select(void);
void lonejson_oidc_jwks_cache_parse_init(void);
void lonejson_oidc_jwks_cache_write_callback(void);
void lonejson_oidc_jwks_cache_parse_finish(void);
void lonejson_oidc_jwks_cache_parse_cleanup(void);
void lonejson_oauth2_client_credentials_body(void);
void lonejson_oauth2_refresh_token_body(void);
void lonejson_oauth2_token_introspection_body(void);
void lonejson_oauth2_token_revocation_body(void);
void lonejson_oidc_authorization_code_token_body(void);
void lonejson_oauth2_client_credentials_request(void);
void lonejson_oauth2_refresh_token_request(void);
void lonejson_oauth2_token_flow_init(void);
void lonejson_oauth2_token_flow_cleanup(void);
void lonejson_oauth2_token_flow_assign(void);
void lonejson_oauth2_token_flow_is_expired(void);
void lonejson_oauth2_token_flow_update_response(void);
void lonejson_oauth2_token_flow_ensure(void);
void lonejson_oauth2_introspect_token_request(void);
void lonejson_oauth2_revoke_token_request(void);
void lonejson_oidc_fetch_userinfo(void);
void lonejson_oidc_authorization_code_token_request(void);
void lonejson_oauth2_token_response_init(void);
void lonejson_oauth2_token_response_cleanup(void);
void lonejson_oauth2_token_response_parse_json(void);
void lonejson_oauth2_introspection_response_init(void);
void lonejson_oauth2_introspection_response_cleanup(void);
void lonejson_oauth2_introspection_response_parse_json(void);
void lonejson_oidc_userinfo_response_init(void);
void lonejson_oidc_userinfo_response_cleanup(void);
void lonejson_oidc_userinfo_response_parse_json(void);
void lonejson_oidc_pkce_init(lonejson_oidc_pkce *pkce);
void lonejson_oidc_pkce_cleanup(lonejson_oidc_pkce *pkce);
void lonejson_oidc_pkce_challenge(void);
void lonejson_oidc_pkce_challenge_with_runtime(void);
void lonejson_oidc_pkce_generate(void);
void lonejson_oidc_pkce_generate_with_runtime(void);
void lonejson_oidc_authorization_url(void);
void lonejson_oidc_authorization_callback_init(void);
void lonejson_oidc_authorization_callback_cleanup(void);
void lonejson_oidc_authorization_callback_parse_query(void);
void lonejson_auth_failure_string(void);
void lonejson_oidc_bearer_validation_init(void);
void lonejson_oidc_bearer_validation_cleanup(void);
void lonejson_oidc_authorization_bearer_token(void);
void lonejson_oidc_validate_bearer_token(void);
void lonejson_m2m_credential_init(void);
void lonejson_m2m_credential_cleanup(void);
void lonejson_m2m_credential_generate(void);
void lonejson_m2m_authentication_init(void);
void lonejson_m2m_authentication_cleanup(void);
void lonejson_m2m_verify_authorization(void);
void lonejson_m2m_signup_init(void);
void lonejson_m2m_signup_cleanup(void);
void lonejson_m2m_signup_generate(void);
void lonejson_m2m_signup_complete_init(void);
void lonejson_m2m_signup_complete_cleanup(void);
void lonejson_m2m_signup_complete(void);

static __inline__ lonejson_status lonejson_auth_provider_init_openssl(
    lonejson_auth_provider *provider, const void *config,
    lonejson_error *error) {
  (void)config;
  lonejson_error_init(error);
  if (provider != 0) {
    provider->user_data = 0;
  }
  return LONEJSON_STATUS_OK;
}

#endif
EOF

cat >"$tmp_dir/lonejson_stub.c" <<'EOF'
#include <lonejson.h>

struct lonejson {
  int unused;
};

static lonejson runtime;

void lonejson_error_init(lonejson_error *error) {
  if (error != 0) {
    error->code = 0;
  }
}

lonejson *lonejson_new(const void *config, lonejson_error *error) {
  (void)config;
  lonejson_error_init(error);
  return &runtime;
}

void lonejson_free(lonejson *runtime_arg) {
  (void)runtime_arg;
}

lonejson_status lonejson_validate_cstr(lonejson *runtime_arg, const char *json, lonejson_error *error) {
  (void)runtime_arg;
  (void)json;
  lonejson_error_init(error);
  return LONEJSON_STATUS_OK;
}

void lonejson_curl_parse_init(void) {
}
void lonejson_curl_upload_is_rewindable(void) {
}
void lonejson_curl_upload_rewind(void) {
}
void lonejson_curl_seek_callback(void) {
}

void lonejson_jwt_parse_compact(void) {
}

void lonejson_jwk_parse_json(void) {
}

void lonejson_jwks_parse_json(void) {
}

void lonejson_jwks_select(void) {
}

void lonejson_jwk_cleanup(void) {
}

void lonejson_jwks_cleanup(void) {
}

void lonejson_jwt_decode_compact(void) {
}

void lonejson_jwt_validate_claims(void) {
}

void lonejson_jwt_validate_signature(void) {
}

void lonejson_jwt_header_cleanup(void) {
}

void lonejson_jwt_claims_cleanup(void) {
}

void lonejson_oidc_discovery_url(void) {
}

void lonejson_oidc_discovery_parse_json(void) {
}

void lonejson_oidc_discovery_validate_issuer(void) {
}

void lonejson_oidc_discovery_cleanup(void) {
}

void lonejson_oidc_jwks_cache_init(void) {
}

void lonejson_oidc_jwks_cache_cleanup(void) {
}

void lonejson_oidc_jwks_cache_update_json(void) {
}

void lonejson_oidc_jwks_cache_is_fresh(void) {
}

void lonejson_oidc_jwks_cache_select(void) {
}

void lonejson_oidc_jwks_cache_parse_init(void) {
}

void lonejson_oidc_jwks_cache_write_callback(void) {
}

void lonejson_oidc_jwks_cache_parse_finish(void) {
}

void lonejson_oidc_jwks_cache_parse_cleanup(void) {
}

void lonejson_oauth2_client_credentials_body(void) {
}

void lonejson_oauth2_refresh_token_body(void) {
}

void lonejson_oauth2_token_introspection_body(void) {
}

void lonejson_oauth2_token_revocation_body(void) {
}

void lonejson_oidc_authorization_code_token_body(void) {
}

void lonejson_oauth2_client_credentials_request(void) {
}

void lonejson_oauth2_refresh_token_request(void) {
}

void lonejson_oauth2_token_flow_init(void) {
}

void lonejson_oauth2_token_flow_cleanup(void) {
}

void lonejson_oauth2_token_flow_assign(void) {
}

void lonejson_oauth2_token_flow_is_expired(void) {
}

void lonejson_oauth2_token_flow_update_response(void) {
}

void lonejson_oauth2_token_flow_ensure(void) {
}

void lonejson_oauth2_introspect_token_request(void) {
}

void lonejson_oauth2_revoke_token_request(void) {
}

void lonejson_oidc_fetch_userinfo(void) {
}

void lonejson_oidc_authorization_code_token_request(void) {
}

void lonejson_oauth2_token_response_init(void) {
}

void lonejson_oauth2_token_response_cleanup(void) {
}

void lonejson_oauth2_token_response_parse_json(void) {
}

void lonejson_oauth2_introspection_response_init(void) {
}

void lonejson_oauth2_introspection_response_cleanup(void) {
}

void lonejson_oauth2_introspection_response_parse_json(void) {
}

void lonejson_oidc_userinfo_response_init(void) {
}

void lonejson_oidc_userinfo_response_cleanup(void) {
}

void lonejson_oidc_userinfo_response_parse_json(void) {
}

void lonejson_oidc_pkce_init(lonejson_oidc_pkce *pkce) {
  if (pkce != 0) {
    pkce->unused = 0;
  }
}

void lonejson_oidc_pkce_cleanup(lonejson_oidc_pkce *pkce) {
  (void)pkce;
}

void lonejson_oidc_pkce_challenge(void) {
}

void lonejson_oidc_pkce_challenge_with_runtime(void) {
}

void lonejson_oidc_pkce_generate(void) {
}

void lonejson_oidc_pkce_generate_with_runtime(void) {
}

void lonejson_oidc_authorization_url(void) {
}

void lonejson_oidc_authorization_callback_init(void) {
}

void lonejson_oidc_authorization_callback_cleanup(void) {
}

void lonejson_oidc_authorization_callback_parse_query(void) {
}

void lonejson_auth_failure_string(void) {
}

void lonejson_oidc_bearer_validation_init(void) {
}

void lonejson_oidc_bearer_validation_cleanup(void) {
}

void lonejson_oidc_authorization_bearer_token(void) {
}

void lonejson_oidc_validate_bearer_token(void) {
}

void lonejson_m2m_credential_init(void) {
}

void lonejson_m2m_credential_cleanup(void) {
}

void lonejson_m2m_credential_generate(void) {
}

void lonejson_m2m_authentication_init(void) {
}

void lonejson_m2m_authentication_cleanup(void) {
}

void lonejson_m2m_verify_authorization(void) {
}

void lonejson_m2m_signup_init(void) {
}

void lonejson_m2m_signup_cleanup(void) {
}

void lonejson_m2m_signup_generate(void) {
}

void lonejson_m2m_signup_complete_init(void) {
}

void lonejson_m2m_signup_complete_cleanup(void) {
}

void lonejson_m2m_signup_complete(void) {
}


EOF

# This metadata fixture implements the same export boundary as the real SDK.
# Supplement its exercised API stubs with inert definitions for the other
# approved symbols, then verify every extracted archive against the real list.
awk 'NR == FNR { source = source " " $0; next }
  source !~ ($0 "[[:space:]]*\\([^;{}]*\\)[[:space:]]*\\{") { print "void " $0 "(void) {}" }' \
  "$tmp_dir/lonejson_stub.c" "$repo_root/cmake/lonejson.exports" >"$tmp_dir/extra-stubs.c"
cat "$tmp_dir/extra-stubs.c" >>"$tmp_dir/lonejson_stub.c"

"$CC" -shared -fPIC -I"$package_root/include" "$tmp_dir/lonejson_stub.c" \
  -o "$package_root/lib/liblonejson.so"
"$CC" -c -I"$package_root/include" "$tmp_dir/lonejson_stub.c" \
  -o "$tmp_dir/lonejson_stub.o"
"$AR" rcs "$package_root/lib/liblonejson.a" "$tmp_dir/lonejson_stub.o"

cat >"$package_root/lib/pkgconfig/lonejson.pc" <<'EOF'
prefix=${pcfiledir}/../..
libdir=${prefix}/lib
includedir=${prefix}/include

Name: lonejson
Description: lonejson archive verifier fixture
Version: 9.9.9
Libs: -L${libdir} -llonejson
Cflags: -I${includedir}
EOF

cat >"$package_root/lib/pkgconfig/lonejson-jwt.pc" <<'EOF'
prefix=${pcfiledir}/../..

Name: lonejson-jwt
Description: lonejson JWT adapter fixture
Version: 9.9.9
Requires: lonejson
Cflags: -DLONEJSON_WITH_JWT
EOF

cat >"$package_root/lib/pkgconfig/lonejson-oidc.pc" <<'EOF'
prefix=${pcfiledir}/../..

Name: lonejson-oidc
Description: lonejson OIDC adapter fixture
Version: 9.9.9
Requires: lonejson-jwt
Cflags: -DLONEJSON_WITH_OIDC
EOF

cat >"$package_root/lib/pkgconfig/lonejson-curl.pc" <<'EOF'
prefix=${pcfiledir}/../..

Name: lonejson-curl
Description: lonejson curl adapter fixture
Version: 9.9.9
Requires: lonejson libcurl
Cflags: -DLONEJSON_WITH_CURL
EOF

cat >"$package_root/lib/pkgconfig/lonejson-openssl.pc" <<'EOF'
prefix=${pcfiledir}/../..

Name: lonejson-openssl
Description: lonejson OpenSSL adapter fixture
Version: 9.9.9
Requires: lonejson-jwt openssl
Cflags: -DLONEJSON_WITH_OPENSSL
EOF

cat >"$package_root/lib/cmake/lonejson/lonejsonConfig.cmake" <<'EOF'
include(CMakeFindDependencyMacro)
if(NOT TARGET lonejson::lonejson)
  add_library(lonejson::lonejson SHARED IMPORTED)
  set_target_properties(lonejson::lonejson PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../liblonejson.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include")
endif()
if(NOT TARGET lonejson::lonejson_static)
  add_library(lonejson::lonejson_static STATIC IMPORTED)
  set_target_properties(lonejson::lonejson_static PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../liblonejson.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include")
endif()
add_library(lonejson::jwt INTERFACE IMPORTED)
set_target_properties(lonejson::jwt PROPERTIES
  INTERFACE_COMPILE_DEFINITIONS LONEJSON_WITH_JWT)
add_library(lonejson::oidc INTERFACE IMPORTED)
set_target_properties(lonejson::oidc PROPERTIES
  INTERFACE_COMPILE_DEFINITIONS LONEJSON_WITH_OIDC
  INTERFACE_LINK_LIBRARIES lonejson::jwt)
if("curl" IN_LIST lonejson_FIND_COMPONENTS)
  find_dependency(CURL)
  add_library(lonejson::curl INTERFACE IMPORTED)
  set_target_properties(lonejson::curl PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS LONEJSON_WITH_CURL
    INTERFACE_LINK_LIBRARIES CURL::libcurl)
endif()
if("openssl" IN_LIST lonejson_FIND_COMPONENTS)
  find_dependency(OpenSSL)
  add_library(lonejson::openssl INTERFACE IMPORTED)
  set_target_properties(lonejson::openssl PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS LONEJSON_WITH_OPENSSL
    INTERFACE_LINK_LIBRARIES "lonejson::jwt;OpenSSL::Crypto")
endif()
EOF

cat >"$package_root/lib/cmake/lonejson/lonejsonConfigVersion.cmake" <<'EOF'
set(PACKAGE_VERSION 9.9.9)
set(PACKAGE_VERSION_COMPATIBLE TRUE)
EOF

cat >"$package_root/share/lonejson/dependencies.json" <<'EOF'
{
  "schema": "pkt.systems.dependencies.v1",
  "project": "lonejson",
  "version": "9.9.9",
  "target_id": "x86_64-linux-gnu",
  "dependencies": [
    {
      "target_id": "x86_64-linux-gnu",
      "source_url": "https://github.com/c89systems/cpkt/releases/download/v0.8.0/cpkt-0.8.0-x86_64-linux-gnu.tar.gz",
      "sha256": "7a9f2c1de42beec5fb324ac9c8b5a8005c17fb98003d9625eb5ad5ad228ba941",
      "archive_name": "cpkt-0.8.0-x86_64-linux-gnu.tar.gz",
      "bundled": false,
      "name": "cpkt",
      "version": "0.8.0",
      "source_system": "GitHub release",
      "license": "MIT",
      "external": false,
      "role": "release-sdk-build-input",
      "provides": [
        "curl",
        "openssl"
      ]
    },
    {
      "target_id": "x86_64-linux-gnu",
      "source_url": "https://github.com/c89systems/cpkt/releases/download/v0.8.0/cpkt-0.8.0-x86_64-linux-gnu.tar.gz",
      "sha256": "7a9f2c1de42beec5fb324ac9c8b5a8005c17fb98003d9625eb5ad5ad228ba941",
      "archive_name": "cpkt-0.8.0-x86_64-linux-gnu.tar.gz",
      "bundled": false,
      "name": "curl",
      "version": "8.22.0",
      "source_system": "cpkt",
      "license": "curl",
      "external": true,
      "role": "optional-facade-consumer-requirement"
    },
    {
      "target_id": "x86_64-linux-gnu",
      "source_url": "https://github.com/c89systems/cpkt/releases/download/v0.8.0/cpkt-0.8.0-x86_64-linux-gnu.tar.gz",
      "sha256": "7a9f2c1de42beec5fb324ac9c8b5a8005c17fb98003d9625eb5ad5ad228ba941",
      "archive_name": "cpkt-0.8.0-x86_64-linux-gnu.tar.gz",
      "bundled": false,
      "name": "openssl",
      "version": "3.6.5",
      "source_system": "cpkt",
      "license": "Apache-2.0",
      "external": true,
      "role": "optional-facade-consumer-requirement"
    }
  ]
}
EOF

# Validate malformed provenance before spending time on downstream builds.
for defect in sha256 license bundled external version malformed; do
  cp "$package_root/share/lonejson/dependencies.json" "$tmp_dir/manifest-$defect.json"
  case "$defect" in
    sha256) perl -pi -e 's/7a9f2c1d/00000000/g' "$tmp_dir/manifest-$defect.json" ;;
    license) perl -pi -e 's/Apache-2.0/MIT/' "$tmp_dir/manifest-$defect.json" ;;
    bundled) perl -pi -e 's/"bundled": false/"bundled": true/g' "$tmp_dir/manifest-$defect.json" ;;
    external) perl -pi -e 's/"external": true/"external": false/g' "$tmp_dir/manifest-$defect.json" ;;
    version) perl -pi -e 's/"version": "9.9.9"/"version": "9.9.8"/' "$tmp_dir/manifest-$defect.json" ;;
    malformed) printf '{invalid' >"$tmp_dir/manifest-$defect.json" ;;
  esac
  if cmake -DLONEJSON_MANIFEST="$tmp_dir/manifest-$defect.json" \
      -DLONEJSON_TARGET_ID=x86_64-linux-gnu -DLONEJSON_VERSION=9.9.9 \
      -P "$repo_root/cmake/verify_dependency_manifest.cmake" >"$tmp_dir/manifest-$defect.log" 2>&1; then
    echo "invalid dependency provenance accepted: $defect" >&2; exit 1
  fi
done

printf 'license\n' >"$package_root/share/doc/liblonejson/LICENSE"
printf 'readme\n' >"$package_root/share/doc/liblonejson/README.md"
mkdir -p "$package_root/share/doc/liblonejson/docs"
cat >"$package_root/share/doc/liblonejson/docs/toolchains.md" <<'EOF'
Use `cmake/toolchains/native.cmake` for native builds.
EOF

cat >"$build_root/x86_64-linux-gnu-release/CMakeCache.txt" <<EOF
CMAKE_C_COMPILER:FILEPATH=$CC
CMAKE_NM:FILEPATH=$NM
CMAKE_READELF:FILEPATH=$CPKT_TOOLCHAIN_READELF
CMAKE_SYSROOT:PATH=$CPKT_SYSROOT
LONEJSON_BOOTLIN_LIBC:STRING=gnu
LONEJSON_BOOTLIN_TOOLCHAIN_ROOT:PATH=$CPKT_TOOLCHAIN_ROOT
LONEJSON_CPKT_ROOT:PATH=$dependency_root
EOF

# Exercise the SDK producer with this already-compiled fixture. Stripping must
# normalize the static archive's internal headers as well as the outer tar.
mkdir -p "$tmp_dir/scripts" "$tmp_dir/cmake" "$tmp_dir/tools/lifecycle/scripts"
cp "$repo_root/scripts/"{archive_release_tree,require_build_workspace,discover_target_tools,cpkt-toolchains,cpkt-native-apple,require_cache_path}.sh "$tmp_dir/scripts/"
cp "$repo_root/tools/lifecycle/scripts/"*.sh "$tmp_dir/tools/lifecycle/scripts/"
cp "$repo_root/cmake/"{lonejson_build_workspace,lonejson_target_tools,lonejson_dist_dir,cpkt_metadata}.cmake "$tmp_dir/cmake/"
cp "$repo_root/LICENSE" "$repo_root/README.md" "$tmp_dir/"
cp -R "$repo_root/docs" "$tmp_dir/docs"
cat >"$build_root/x86_64-linux-gnu-release/cmake_install.cmake" <<EOF
file(COPY "$package_root/" DESTINATION "\${CMAKE_INSTALL_PREFIX}")
EOF
for mask in 022 077; do
  (umask "$mask"; cmake -D "LONEJSON_ROOT=$tmp_dir" \
    -D "LONEJSON_BINARY_DIR=$build_root/x86_64-linux-gnu-release" \
    -D LONEJSON_VERSION=9.9.9 -D LONEJSON_TARGET_ID=x86_64-linux-gnu \
    -D LONEJSON_SHARED_LIB_NAME=liblonejson.so -D LONEJSON_STATIC_LIB_NAME=liblonejson.a \
    -D LONEJSON_BUILD_WITH_CURL=ON -D LONEJSON_BUILD_WITH_OPENSSL=ON \
    -D LONEJSON_BUILD_WITH_JWT=ON -D LONEJSON_BUILD_WITH_OIDC=ON \
    -P "$repo_root/cmake/package_archive.cmake")
  cp "$dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" "$tmp_dir/produced-$mask.tar.gz"
done
cmp "$tmp_dir/produced-022.tar.gz" "$tmp_dir/produced-077.tar.gz" || {
  printf 'SDK producer changed shipped bytes with staging metadata\n' >&2; exit 1;
}
mkdir -p "$tmp_dir/produced-docs"
tar -xzf "$tmp_dir/produced-022.tar.gz" -C "$tmp_dir/produced-docs" \
  liblonejson-9.9.9-x86_64-linux-gnu/share/doc/liblonejson/docs
for doc in "$repo_root"/docs/*.md; do
  cmp "$doc" "$tmp_dir/produced-docs/liblonejson-9.9.9-x86_64-linux-gnu/share/doc/liblonejson/docs/${doc##*/}"
done

"$AR" rcs "$dependency_root/lib/libcurl.a"
"$AR" rcs "$dependency_root/lib/libcrypto.a"
cat >"$dependency_root/lib/pkgconfig/libcurl.pc" <<EOF
prefix=$dependency_root
libdir=\${prefix}/lib

Name: libcurl
Description: fake libcurl fixture
Version: 8.0.0
Libs: -L\${libdir} -lcurl
EOF
cat >"$dependency_root/lib/pkgconfig/openssl.pc" <<EOF
prefix=$dependency_root
libdir=\${prefix}/lib

Name: OpenSSL
Description: fake OpenSSL fixture
Version: 3.0.0
Libs: -L\${libdir} -lcrypto
EOF
cat >"$dependency_root/lib/cmake/CURL/CURLConfig.cmake" <<EOF
add_library(CURL::libcurl STATIC IMPORTED)
set_target_properties(CURL::libcurl PROPERTIES
  IMPORTED_LOCATION "$dependency_root/lib/libcurl.a")
EOF
cat >"$dependency_root/lib/cmake/OpenSSL/OpenSSLConfig.cmake" <<EOF
add_library(OpenSSL::Crypto STATIC IMPORTED)
set_target_properties(OpenSSL::Crypto PROPERTIES
  IMPORTED_LOCATION "$dependency_root/lib/libcrypto.a")
add_library(OpenSSL::SSL INTERFACE IMPORTED)
EOF

tar -C "$tmp_dir/package" -czf \
  "$dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)

"$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root"

"$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$tmp_dir/missing-build-root"

# The extracted SDK gate must reject a real shared object with a private
# dynamic export, even when the checksum and metadata are otherwise correct.
export_dist="$tmp_dir/export-leak-dist"
export_package="$tmp_dir/export-leak-package"
mkdir -p "$export_dist"
cp -R "$tmp_dir/package" "$export_package"
export_root="$export_package/liblonejson-9.9.9-x86_64-linux-gnu"
cp "$tmp_dir/lonejson_stub.c" "$tmp_dir/export-leak.c"
printf '\nvoid lonejson__export_leak(void) {}\n' >>"$tmp_dir/export-leak.c"
"$CC" -shared -fPIC -Wall -Wextra -Werror -I"$export_root/include" \
  "$tmp_dir/export-leak.c" -o "$export_root/lib/liblonejson.so"
tar -czf "$export_dist/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  -C "$export_package" liblonejson-9.9.9-x86_64-linux-gnu
(cd "$export_dist" && sha256sum *.tar.gz >lonejson-9.9.9-CHECKSUMS)
if "$repo_root/scripts/verify_release_archives.sh" "$repo_root" \
    "$export_dist/lonejson-9.9.9-CHECKSUMS" "$build_root" >"$tmp_dir/export-leak.log" 2>&1; then
  printf 'extracted SDK private export was accepted\n' >&2; exit 1
fi
grep -F 'lonejson__export_leak' "$tmp_dir/export-leak.log" >/dev/null
grep -F 'unexpected dynamic exports' "$tmp_dir/export-leak.log" >/dev/null

bootlin_path_dist_dir="$tmp_dir/bootlin-path-dist"
bootlin_path_package_dir="$tmp_dir/bootlin-path-package"
bootlin_path_root="$bootlin_path_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$bootlin_path_dist_dir"
cp -R "$tmp_dir/package" "$bootlin_path_package_dir"
cat >>"$bootlin_path_root/lib/pkgconfig/lonejson.pc" <<'EOF'
Libs.private: -Wl,-rpath,/srv/build-cache/cpkt/toolchains/roots/x86-64--glibc--stable-2026.08-1/sysroot/lib
EOF
tar -C "$bootlin_path_package_dir" -czf \
  "$bootlin_path_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$bootlin_path_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)
bootlin_path_log="$tmp_dir/bootlin-path.log"
if "$repo_root/scripts/verify_release_archives.sh" \
    "$repo_root" \
    "$bootlin_path_dist_dir/lonejson-9.9.9-CHECKSUMS" \
    "$build_root" >"$bootlin_path_log" 2>&1; then
  printf 'expected archive verification to fail on a pinned Bootlin path leak\n' >&2
  exit 1
fi
grep -F 'pinned Bootlin toolchain path leaked in release archive' "$bootlin_path_log" >/dev/null

custom_cache_dist_dir="$tmp_dir/custom-cache-dist"
custom_cache_package_dir="$tmp_dir/custom-cache-package"
custom_cache_root="$custom_cache_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$custom_cache_dist_dir"
cp -R "$tmp_dir/package" "$custom_cache_package_dir"
cat >>"$custom_cache_root/lib/pkgconfig/lonejson.pc" <<'EOF'
Libs.private: -L/srv/cpkt-cache/roots/x86-64--glibc--stable-2026.08-1/sysroot/lib
EOF
tar -C "$custom_cache_package_dir" -czf \
  "$custom_cache_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$custom_cache_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)
custom_cache_log="$tmp_dir/custom-cache.log"
if "$repo_root/scripts/verify_release_archives.sh" \
    "$repo_root" \
    "$custom_cache_dist_dir/lonejson-9.9.9-CHECKSUMS" \
    "$build_root" >"$custom_cache_log" 2>&1; then
  printf 'expected archive verification to fail on a custom toolchain cache path leak\n' >&2
  exit 1
fi
grep -F 'pinned Bootlin toolchain path leaked in release archive' "$custom_cache_log" >/dev/null

# Documentation may name repository toolchain files, but actual workstation
# collection paths must still be rejected there as in libraries and metadata.
doc_cache_dist_dir="$tmp_dir/doc-cache-dist"
doc_cache_package_dir="$tmp_dir/doc-cache-package"
doc_cache_root="$doc_cache_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$doc_cache_dist_dir"
cp -R "$tmp_dir/package" "$doc_cache_package_dir"
printf '%s\n' '/srv/build-cache/cpkt/toolchains/roots/prepared-sdk/sysroot/lib' \
  >>"$doc_cache_root/share/doc/liblonejson/docs/toolchains.md"
tar -C "$doc_cache_package_dir" -czf \
  "$doc_cache_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$doc_cache_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)
doc_cache_log="$tmp_dir/doc-cache.log"
if "$repo_root/scripts/verify_release_archives.sh" \
    "$repo_root" "$doc_cache_dist_dir/lonejson-9.9.9-CHECKSUMS" \
    "$build_root" >"$doc_cache_log" 2>&1; then
  printf 'expected archive verification to fail on a toolchain cache path in documentation\n' >&2
  exit 1
fi
grep -F 'pinned Bootlin toolchain path leaked in release archive' "$doc_cache_log" >/dev/null

missing_metadata_dist_dir="$tmp_dir/missing-metadata-dist"
missing_metadata_package_dir="$tmp_dir/missing-metadata-package"
mkdir -p "$missing_metadata_dist_dir"
cp -R "$tmp_dir/package" "$missing_metadata_package_dir"
rm -f "$missing_metadata_package_dir/liblonejson-9.9.9-x86_64-linux-gnu/share/lonejson/dependencies.json"
tar -C "$missing_metadata_package_dir" -czf \
  "$missing_metadata_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$missing_metadata_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)
missing_metadata_log="$tmp_dir/missing-metadata.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$missing_metadata_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$missing_metadata_log" 2>&1; then
  printf 'expected archive verification to fail when dependency manifest is missing\n' >&2
  exit 1
fi
grep -F 'missing required file:' "$missing_metadata_log" >/dev/null

pkg_crypto_dist_dir="$tmp_dir/pkg-crypto-dist"
pkg_crypto_package_dir="$tmp_dir/pkg-crypto-package"
pkg_crypto_root="$pkg_crypto_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$pkg_crypto_dist_dir"
cp -R "$tmp_dir/package" "$pkg_crypto_package_dir"
cat >>"$pkg_crypto_root/lib/pkgconfig/lonejson.pc" <<'EOF'
Libs.private: -lcrypto
EOF
tar -C "$pkg_crypto_package_dir" -czf \
  "$pkg_crypto_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$pkg_crypto_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)
pkg_crypto_log="$tmp_dir/pkg-crypto.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$pkg_crypto_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$pkg_crypto_log" 2>&1; then
  printf 'expected archive verification to fail when pkg-config forces libcrypto\n' >&2
  exit 1
fi
grep -F 'unexpected third-party pkg-config dependency in core lonejson SDK' "$pkg_crypto_log" >/dev/null

cmake_crypto_dist_dir="$tmp_dir/cmake-crypto-dist"
cmake_crypto_package_dir="$tmp_dir/cmake-crypto-package"
cmake_crypto_root="$cmake_crypto_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$cmake_crypto_dist_dir"
cp -R "$tmp_dir/package" "$cmake_crypto_package_dir"
perl -0pi -e 's/INTERFACE_INCLUDE_DIRECTORIES "\$\{CMAKE_CURRENT_LIST_DIR\}\/\.\.\/\.\.\/\.\.\/include"\)/INTERFACE_INCLUDE_DIRECTORIES "\$\{CMAKE_CURRENT_LIST_DIR\}\/\.\.\/\.\.\/\.\.\/include"\n  INTERFACE_LINK_LIBRARIES crypto)/' \
  "$cmake_crypto_root/lib/cmake/lonejson/lonejsonConfig.cmake"
tar -C "$cmake_crypto_package_dir" -czf \
  "$cmake_crypto_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$cmake_crypto_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)
cmake_crypto_log="$tmp_dir/cmake-crypto.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$cmake_crypto_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$cmake_crypto_log" 2>&1; then
  printf 'expected archive verification to fail when CMake forces libcrypto\n' >&2
  exit 1
fi
grep -F 'unexpected third-party CMake dependency in core lonejson SDK' "$cmake_crypto_log" >/dev/null

openssl_metadata_dist_dir="$tmp_dir/openssl-metadata-dist"
openssl_metadata_package_dir="$tmp_dir/openssl-metadata-package"
openssl_metadata_root="$openssl_metadata_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$openssl_metadata_dist_dir"
cp -R "$tmp_dir/package" "$openssl_metadata_package_dir"
cat >>"$openssl_metadata_root/lib/pkgconfig/lonejson.pc" <<'EOF'
Requires.private: openssl
EOF
tar -C "$openssl_metadata_package_dir" -czf \
  "$openssl_metadata_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$openssl_metadata_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)
openssl_metadata_log="$tmp_dir/openssl-metadata.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$openssl_metadata_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$openssl_metadata_log" 2>&1; then
  printf 'expected archive verification to fail when pkg-config requires OpenSSL\n' >&2
  exit 1
fi
grep -F 'unexpected third-party pkg-config dependency in core lonejson SDK' "$openssl_metadata_log" >/dev/null

empty_license_dist="$tmp_dir/empty-license-dist"
empty_license_package="$tmp_dir/empty-license-package"
mkdir -p "$empty_license_dist"
cp -R "$tmp_dir/package" "$empty_license_package"
: >"$empty_license_package/liblonejson-9.9.9-x86_64-linux-gnu/share/doc/liblonejson/LICENSE"
tar -C "$empty_license_package" -czf "$empty_license_dist/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  liblonejson-9.9.9-x86_64-linux-gnu
(cd "$empty_license_dist" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >CHECKSUMS)
if "$repo_root/scripts/verify_release_archives.sh" "$repo_root" \
    "$empty_license_dist/CHECKSUMS" "$build_root" >"$tmp_dir/empty-license.log" 2>&1; then
  echo 'empty project license accepted' >&2; exit 1
fi
grep -F 'missing project license text' "$tmp_dir/empty-license.log" >/dev/null

openssl_build_input_dist_dir="$tmp_dir/openssl-build-input-dist"
openssl_build_input_package_dir="$tmp_dir/openssl-build-input-package"
openssl_build_input_root="$openssl_build_input_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$openssl_build_input_dist_dir"
cp -R "$tmp_dir/package" "$openssl_build_input_package_dir"
perl -0pi -e 's/"external": true/"external": false/g' \
  "$openssl_build_input_root/share/lonejson/dependencies.json"
tar -C "$openssl_build_input_package_dir" -czf \
  "$openssl_build_input_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$openssl_build_input_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)
openssl_build_input_log="$tmp_dir/openssl-build-input.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$openssl_build_input_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$openssl_build_input_log" 2>&1; then
  printf 'expected archive verification to fail when facade requirements are not external\n' >&2
  exit 1
fi
grep -F 'dependency manifest mismatch: curl.external' "$openssl_build_input_log" >/dev/null

broken_dist_dir="$tmp_dir/broken-dist"
broken_package_dir="$tmp_dir/broken-package"
broken_package_root="$broken_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$broken_dist_dir"
cp -R "$tmp_dir/package" "$broken_package_dir"

cat >"$tmp_dir/lonejson_static_without_curl.c" <<'EOF'
#include <lonejson.h>

struct lonejson {
  int unused;
};

static lonejson runtime;

void lonejson_error_init(lonejson_error *error) {
  if (error != 0) {
    error->code = 0;
  }
}

lonejson *lonejson_new(const void *config, lonejson_error *error) {
  (void)config;
  lonejson_error_init(error);
  return &runtime;
}

void lonejson_free(lonejson *runtime_arg) {
  (void)runtime_arg;
}

lonejson_status lonejson_validate_cstr(lonejson *runtime_arg, const char *json, lonejson_error *error) {
  (void)runtime_arg;
  (void)json;
  lonejson_error_init(error);
  return LONEJSON_STATUS_OK;
}
EOF

"$CC" -c -I"$broken_package_root/include" "$tmp_dir/lonejson_static_without_curl.c" \
  -o "$tmp_dir/lonejson_static_without_curl.o"
rm -f "$broken_package_root/lib/liblonejson.a"
"$AR" rcs "$broken_package_root/lib/liblonejson.a" \
  "$tmp_dir/lonejson_static_without_curl.o"

tar -C "$broken_package_dir" -czf \
  "$broken_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$broken_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)

broken_log="$tmp_dir/broken.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$broken_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$broken_log" 2>&1; then
  printf 'expected archive verification to fail when static library lacks curl ABI\n' >&2
  exit 1
fi
grep -F 'missing lonejson_curl_parse_init ABI symbol in static library' "$broken_log" >/dev/null

jwt_broken_dist_dir="$tmp_dir/jwt-broken-dist"
jwt_broken_package_dir="$tmp_dir/jwt-broken-package"
jwt_broken_package_root="$jwt_broken_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$jwt_broken_dist_dir"
cp -R "$tmp_dir/package" "$jwt_broken_package_dir"

cat >"$tmp_dir/lonejson_static_without_jwt.c" <<'EOF'
#include <lonejson.h>

struct lonejson {
  int unused;
};

static lonejson runtime;

void lonejson_error_init(lonejson_error *error) {
  if (error != 0) {
    error->code = 0;
  }
}

lonejson *lonejson_new(const void *config, lonejson_error *error) {
  (void)config;
  lonejson_error_init(error);
  return &runtime;
}

void lonejson_free(lonejson *runtime_arg) {
  (void)runtime_arg;
}

lonejson_status lonejson_validate_cstr(lonejson *runtime_arg, const char *json, lonejson_error *error) {
  (void)runtime_arg;
  (void)json;
  lonejson_error_init(error);
  return LONEJSON_STATUS_OK;
}

void lonejson_curl_parse_init(void) {
}
void lonejson_curl_upload_is_rewindable(void) {
}
void lonejson_curl_upload_rewind(void) {
}
void lonejson_curl_seek_callback(void) {
}
EOF

"$CC" -c -I"$jwt_broken_package_root/include" "$tmp_dir/lonejson_static_without_jwt.c" \
  -o "$tmp_dir/lonejson_static_without_jwt.o"
rm -f "$jwt_broken_package_root/lib/liblonejson.a"
"$AR" rcs "$jwt_broken_package_root/lib/liblonejson.a" \
  "$tmp_dir/lonejson_static_without_jwt.o"

tar -C "$jwt_broken_package_dir" -czf \
  "$jwt_broken_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$jwt_broken_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)

jwt_broken_log="$tmp_dir/jwt-broken.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$jwt_broken_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$jwt_broken_log" 2>&1; then
  printf 'expected archive verification to fail when static library lacks JWT ABI\n' >&2
  exit 1
fi
grep -F 'missing lonejson_jwt_* ABI symbol in static library' "$jwt_broken_log" >/dev/null
