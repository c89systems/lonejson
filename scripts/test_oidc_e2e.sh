#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir=${LONEJSON_E2E_BUILD_DIR:-$repo_root/build/host-curl}
oidc_port=${LONEJSON_OIDC_E2E_PORT:-18443}
api_fixture_port=${LONEJSON_API_FIXTURE_E2E_PORT:-18080}
issuer=${LONEJSON_OIDC_E2E_ISSUER:-https://localhost:${oidc_port}/default}
audience=${LONEJSON_OIDC_E2E_AUDIENCE:-lonejson-api}
server_port=${LONEJSON_OIDC_E2E_SERVER_PORT:-18081}
ca_file=${LONEJSON_OIDC_E2E_CAINFO:-$repo_root/build/devenv/credentials/server.crt}
discovery_url=${LONEJSON_OIDC_E2E_DISCOVERY_URL:-https://localhost:${oidc_port}/.well-known/openid-configuration/default}
server_pid=
umask 077
mkdir -p "$repo_root/build/devenv/logs"

cleanup() {
  if [[ -n "${server_pid}" ]] && kill -0 "$server_pid" >/dev/null 2>&1; then
    kill "$server_pid" >/dev/null 2>&1 || true
    wait "$server_pid" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'missing required command for OIDC e2e: %s\n' "$1" >&2
    exit 1
  fi
}

wait_url() {
  local url=$1
  local i
  for i in $(seq 1 90); do
    if curl -fsS --max-time 3 --cacert "$ca_file" "$url" >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
  done
  printf 'timed out waiting for %s\n' "$url" >&2
  return 1
}

source "$repo_root/tests/json_helpers.sh"

client_credentials_token() {
  local client_id=$1
  local response
  response="$(
    curl -fsS --max-time 10 --cacert "$ca_file" -X POST "$token_endpoint" \
      -H 'Content-Type: application/x-www-form-urlencoded' \
      --data-urlencode 'grant_type=client_credentials' \
      --data-urlencode "client_id=$client_id" \
      --data-urlencode 'client_secret=lonejson-secret' \
      --data-urlencode 'scope=openid profile' \
      --data-urlencode "audience=$audience"
  )"
  printf '%s' "$response" | json_field access_token
}

assert_protected_authorized() {
  local token=$1
  local label=$2
  curl -fsS --max-time 5 "http://127.0.0.1:$server_port/protected" \
    -H "Authorization: Bearer $token" \
    | json_assert ok=true authorized=true
}

assert_protected_rejected() {
  local token=$1
  local expected_failure=$2
  local label=$3
  local body
  local response
  local status
  response="$(
    curl -sS --max-time 5 -w '\n%{http_code}' \
      "http://127.0.0.1:$server_port/protected" \
      -H "Authorization: Bearer $token"
  )"
  status="${response##*$'\n'}"
  body="${response%$'\n'*}"
  if [[ "$status" != 401 ]]; then
    printf '%s: expected HTTP 401, got %s\n' "$label" "$status" >&2
    printf '%s\n' "$body" >&2
    exit 1
  fi
  printf '%s' "$body" | json_assert ok=false authorized=false "failure=$expected_failure"
}

assert_protected_missing_bearer_rejected() {
  local body
  local response
  local status
  response="$(
    curl -sS --max-time 5 -w '\n%{http_code}' \
      "http://127.0.0.1:$server_port/protected"
  )"
  status="${response##*$'\n'}"
  body="${response%$'\n'*}"
  if [[ "$status" != 401 ]]; then
    printf 'missing bearer: expected HTTP 401, got %s\n' "$status" >&2
    printf '%s\n' "$body" >&2
    exit 1
  fi
  printf '%s' "$body" | json_assert ok=false authorized=false failure=missing_credentials
}

require_command curl
require_command cmake

if [[ ! -x "$build_dir/lonejson_oidc_fixture_server" ]]; then
  printf 'missing build/host-curl/lonejson_oidc_fixture_server; run make test-oidc-e2e\n' >&2
  exit 1
fi

wait_url "$discovery_url"
discovery_json="$(curl -fsS --max-time 10 --cacert "$ca_file" "$discovery_url")"
token_endpoint="$(printf '%s' "$discovery_json" | json_field token_endpoint)"
authorization_endpoint="$(printf '%s' "$discovery_json" | json_field authorization_endpoint)"
introspection_endpoint="$(printf '%s' "$discovery_json" | json_field introspection_endpoint)"
revocation_endpoint="$(printf '%s' "$discovery_json" | json_field revocation_endpoint)"
userinfo_endpoint="$(printf '%s' "$discovery_json" | json_field userinfo_endpoint)"

token_json="$(
  curl -fsS --max-time 10 --cacert "$ca_file" -X POST "$token_endpoint" \
    -H 'Content-Type: application/x-www-form-urlencoded' \
    --data-urlencode 'grant_type=client_credentials' \
    --data-urlencode 'client_id=lonejson-m2m' \
    --data-urlencode 'client_secret=lonejson-secret' \
    --data-urlencode 'scope=openid profile' \
    --data-urlencode "audience=$audience"
)"
access_token="$(printf '%s' "$token_json" | json_field access_token)"
multi_audience_token="$(client_credentials_token lonejson-good-multi-aud)"
scp_array_token="$(client_credentials_token lonejson-scp-array)"
wrong_audience_token="$(client_credentials_token lonejson-wrong-audience)"
missing_scope_token="$(client_credentials_token lonejson-missing-scope)"
wrong_azp_token="$(client_credentials_token lonejson-wrong-azp)"
missing_azp_token="$(client_credentials_token lonejson-missing-azp)"

authorization_location="$(
  curl -fsS --max-time 10 --cacert "$ca_file" -D - -o /dev/null \
    "${authorization_endpoint}?response_type=code&client_id=lonejson-m2m&redirect_uri=http%3A%2F%2F127.0.0.1%2Fcallback&scope=openid%20profile&audience=${audience}&state=lonejson-state&nonce=lonejson-nonce" \
    | awk 'BEGIN{IGNORECASE=1} /^location:/ {print $2}' \
    | tr -d '\r'
)"
authorization_code="$(
  url_query_field "$authorization_location" code
)"
authorization_token_json="$(
  curl -fsS --max-time 10 --cacert "$ca_file" -X POST "$token_endpoint" \
    -H 'Content-Type: application/x-www-form-urlencoded' \
    --data-urlencode 'grant_type=authorization_code' \
    --data-urlencode "code=$authorization_code" \
    --data-urlencode 'redirect_uri=http://127.0.0.1/callback' \
    --data-urlencode 'client_id=lonejson-m2m' \
    --data-urlencode 'client_secret=lonejson-secret'
)"
authorization_access_token="$(printf '%s' "$authorization_token_json" | json_field access_token)"
refresh_token="$(printf '%s' "$authorization_token_json" | json_field refresh_token)"

curl -fsS --max-time 10 --cacert "$ca_file" -X POST "$introspection_endpoint" \
  -u 'lonejson-m2m:lonejson-secret' \
  -H 'Content-Type: application/x-www-form-urlencoded' \
  --data-urlencode "token=$authorization_access_token" \
  --data-urlencode 'token_type_hint=access_token' \
  | json_assert active=true

curl -fsS --max-time 10 --cacert "$ca_file" "$userinfo_endpoint" \
  -H "Authorization: Bearer $authorization_access_token" \
  | json_field iss | grep -q .

refresh_json="$(
  curl -fsS --max-time 10 --cacert "$ca_file" -X POST "$token_endpoint" \
    -H 'Content-Type: application/x-www-form-urlencoded' \
    --data-urlencode 'grant_type=refresh_token' \
    --data-urlencode "refresh_token=$refresh_token" \
    --data-urlencode 'client_id=lonejson-m2m' \
    --data-urlencode 'client_secret=lonejson-secret'
)"
refreshed_access_token="$(printf '%s' "$refresh_json" | json_field access_token)"
rotated_refresh_token="$(printf '%s' "$refresh_json" | json_field refresh_token)"
if [[ -z "$rotated_refresh_token" ]]; then
  rotated_refresh_token="$refresh_token"
fi

LONEJSON_OIDC_E2E_CAINFO="$ca_file" \
  "$build_dir/lonejson_oidc_fixture_server" \
  "$issuer" "$audience" "$server_port" 10 \
  "$authorization_access_token" "$rotated_refresh_token" \
  >"$repo_root/build/devenv/logs/oidc-fixture-server.log" 2>&1 &
server_pid=$!

ready=0
for _ in $(seq 1 90); do
  if curl -fsS --max-time 2 "http://127.0.0.1:$server_port/health" >/dev/null 2>&1; then
    ready=1
    break
  fi
  sleep 1
done
if [[ "$ready" != 1 ]]; then
  printf 'timed out waiting for http://127.0.0.1:%s/health\n' "$server_port" >&2
  exit 1
fi

assert_protected_missing_bearer_rejected

assert_protected_authorized "$access_token" default-client-credentials
assert_protected_authorized "$refreshed_access_token" refreshed-token
assert_protected_authorized "$multi_audience_token" multi-audience-with-azp
assert_protected_authorized "$scp_array_token" scp-array-scopes
assert_protected_rejected "$wrong_audience_token" audience_mismatch wrong-audience
assert_protected_rejected "$missing_scope_token" claims_invalid missing-scope
assert_protected_rejected "$wrong_azp_token" claims_invalid wrong-azp
assert_protected_rejected "$missing_azp_token" claims_invalid missing-azp

wait "$server_pid"
server_pid=

curl -fsS --max-time 5 "http://127.0.0.1:${api_fixture_port}/health" >/dev/null
curl -fsS --max-time 5 "http://127.0.0.1:${api_fixture_port}/protected" \
  -H "Authorization: Bearer $access_token" >/dev/null

printf '%s\n' 'OIDC/OAuth2 e2e passed'
