#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?usage: check_jwt_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
build_dir=${2:?usage: check_jwt_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
target_id=${3:?usage: check_jwt_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
shared_lib=${4:?usage: check_jwt_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
static_lib=${5:?usage: check_jwt_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
context=${6:-$target_id}

repo_root="$(CDPATH= cd -- "$repo_root" && pwd)"
case "$build_dir" in
  /*) ;;
  *) build_dir="$PWD/$build_dir" ;;
esac
if [[ -d "$build_dir" ]]; then
  build_dir="$(CDPATH= cd -- "$build_dir" && pwd)"
fi

if [[ ! -f "$shared_lib" ]]; then
  printf 'missing shared library for JWT ABI check in %s: %s\n' "$context" "$shared_lib" >&2
  exit 1
fi
if [[ ! -f "$static_lib" ]]; then
  printf 'missing static library for JWT ABI check in %s: %s\n' "$context" "$static_lib" >&2
  exit 1
fi

resolved_environment=$("$repo_root/scripts/discover_target_tools.sh" \
  --build-dir "$build_dir" \
  --target-id "$target_id") || exit $?
eval "$resolved_environment"

if [[ -z "${NM:-}" ]]; then
  printf 'missing target nm for %s\n' "$target_id" >&2
  exit 1
fi

if [[ "$target_id" == *apple-darwin ]]; then
  shared_symbols="$("$NM" -gU "$shared_lib")"
  static_symbols="$("$NM" -gU "$static_lib")"
  symbol_prefix_optional=1
else
  shared_symbols="$("$NM" -D --defined-only "$shared_lib")"
  static_symbols="$("$NM" -g --defined-only "$static_lib")"
  symbol_prefix_optional=0
fi

symbol_present() {
  local symbols=$1
  local symbol=$2
  local prefix_optional=$3
  awk -v symbol="$symbol" -v prefix_optional="$prefix_optional" '
    {
      actual = $NF
      if (actual == symbol || (prefix_optional == 1 && actual == "_" symbol)) {
        found = 1
      }
    }
    END { exit found ? 0 : 1 }
  ' <<<"$symbols"
}

forbidden_openssl_export_present() {
  local symbols=$1
  awk '
    {
      actual = $NF
      sub(/^_/, "", actual)
      if (actual ~ /^(OPENSSL_|OpenSSL_|EVP_|RSA_|BN_|ASN1_|AES_|X509_|OSSL_|CRYPTO_)/) {
        found = actual
        exit
      }
    }
    END {
      if (found != "") {
        print found
        exit 0
      }
      exit 1
    }
  ' <<<"$symbols"
}

if leaked_symbol="$(forbidden_openssl_export_present "$shared_symbols")"; then
  printf 'forbidden OpenSSL ABI symbol exported by shared library for %s: %s\n' "$context" "$leaked_symbol" >&2
  exit 1
fi

for symbol in \
    lonejson_jwt_parse_compact \
    lonejson_jwk_parse_json \
    lonejson_jwks_parse_json \
    lonejson_jwks_select \
    lonejson_jwk_cleanup \
    lonejson_jwks_cleanup \
    lonejson_jwt_decode_compact \
    lonejson_jwt_validate_claims \
    lonejson_jwt_validate_signature \
    lonejson_jwt_header_cleanup \
    lonejson_jwt_claims_cleanup \
    lonejson_oidc_discovery_url \
    lonejson_oidc_discovery_parse_json \
    lonejson_oidc_discovery_validate_issuer \
    lonejson_oidc_discovery_cleanup \
    lonejson_oidc_jwks_cache_init \
    lonejson_oidc_jwks_cache_cleanup \
    lonejson_oidc_jwks_cache_update_json \
    lonejson_oidc_jwks_cache_is_fresh \
    lonejson_oidc_jwks_cache_select \
    lonejson_oidc_jwks_cache_parse_init \
    lonejson_oidc_jwks_cache_write_callback \
    lonejson_oidc_jwks_cache_parse_finish \
    lonejson_oidc_jwks_cache_parse_cleanup \
    lonejson_oauth2_client_credentials_body \
    lonejson_oauth2_token_response_init \
    lonejson_oauth2_token_response_cleanup \
    lonejson_oauth2_token_response_parse_json \
    lonejson_oauth2_token_flow_assign \
    lonejson_oidc_pkce_init \
    lonejson_oidc_pkce_cleanup \
    lonejson_oidc_pkce_challenge \
    lonejson_oidc_pkce_challenge_with_runtime \
    lonejson_oidc_pkce_generate \
    lonejson_oidc_pkce_generate_with_runtime \
    lonejson_oidc_authorization_url \
    lonejson_oidc_authorization_callback_init \
    lonejson_oidc_authorization_callback_cleanup \
    lonejson_oidc_authorization_callback_parse_query \
    lonejson_auth_failure_string \
    lonejson_oidc_bearer_validation_init \
    lonejson_oidc_bearer_validation_cleanup \
    lonejson_oidc_authorization_bearer_token \
    lonejson_oidc_validate_bearer_token \
    lonejson_m2m_credential_init \
    lonejson_m2m_credential_cleanup \
    lonejson_m2m_credential_generate \
    lonejson_m2m_authentication_init \
    lonejson_m2m_authentication_cleanup \
    lonejson_m2m_verify_authorization \
    lonejson_m2m_signup_init \
    lonejson_m2m_signup_cleanup \
    lonejson_m2m_signup_generate \
    lonejson_m2m_signup_complete_init \
    lonejson_m2m_signup_complete_cleanup \
    lonejson_m2m_signup_complete; do
  if ! symbol_present "$shared_symbols" "$symbol" "$symbol_prefix_optional"; then
    printf 'missing lonejson_jwt_* ABI symbol in shared library for %s: %s\n' "$context" "$symbol" >&2
    exit 1
  fi
  if ! symbol_present "$static_symbols" "$symbol" "$symbol_prefix_optional"; then
    printf 'missing lonejson_jwt_* ABI symbol in static library for %s: %s\n' "$context" "$symbol" >&2
    exit 1
  fi
done
