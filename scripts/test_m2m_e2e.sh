#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir=${LONEJSON_E2E_BUILD_DIR:-$repo_root/build/host-curl}
server_port=${LONEJSON_M2M_E2E_SERVER_PORT:-18082}
secrets_file="$repo_root/build/devenv/credentials/m2m-fixture-secrets.json"
server_pid=

cleanup() {
  if [[ -n "${server_pid}" ]] && kill -0 "$server_pid" >/dev/null 2>&1; then
    kill "$server_pid" >/dev/null 2>&1 || true
    wait "$server_pid" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'missing required command for M2M e2e: %s\n' "$1" >&2
    exit 1
  fi
}

source "$repo_root/tests/json_helpers.sh"

require_command curl
require_command cmake

if [[ ! -x "$build_dir/lonejson_m2m_fixture_server" ]]; then
  printf 'missing build/host-curl/lonejson_m2m_fixture_server; run make test-m2m-e2e\n' >&2
  exit 1
fi

umask 077
mkdir -p "$repo_root/build/devenv/credentials" "$repo_root/build/devenv/logs"
rm -f "$secrets_file"
"$build_dir/lonejson_m2m_fixture_server" \
  "$server_port" "$secrets_file" 11 >"$repo_root/build/devenv/logs/m2m-fixture-server.log" 2>&1 &
server_pid=$!

for _ in $(seq 1 90); do
  if [[ -s "$secrets_file" ]] && curl -fsS --max-time 2 "http://127.0.0.1:$server_port/health" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done
if [[ ! -s "$secrets_file" ]]; then
  printf 'timed out waiting for M2M fixture secrets\n' >&2
  exit 1
fi

client_id="$(json_field client_id <"$secrets_file")"
client_secret="$(json_field client_secret <"$secrets_file")"
api_key="$(json_field api_key <"$secrets_file")"
signup_url="$(json_field signup_url <"$secrets_file")"

if curl -fsS --max-time 5 "http://127.0.0.1:$server_port/protected" >/dev/null 2>&1; then
  printf 'protected endpoint accepted missing credentials\n' >&2
  exit 1
fi

curl -fsS --max-time 5 -u "$client_id:$client_secret" \
  "http://127.0.0.1:$server_port/protected" \
  | json_assert ok=true authorized=true

curl -fsS --max-time 5 \
  -H "Authorization: Bearer $api_key" \
  "http://127.0.0.1:$server_port/protected" \
  | json_assert ok=true authorized=true

if curl -fsS --max-time 5 -u "$client_id:wrong-secret" \
  "http://127.0.0.1:$server_port/protected" >/dev/null 2>&1; then
  printf 'protected endpoint accepted wrong client secret\n' >&2
  exit 1
fi

if curl -fsS --max-time 5 "${signup_url}&email=user@example.com&signup_secret=wrong" \
  >/dev/null 2>&1; then
  printf 'signup endpoint accepted wrong signup secret\n' >&2
  exit 1
fi

if curl -fsS --max-time 5 "$signup_url" >/dev/null 2>&1; then
  printf 'signup endpoint accepted missing email\n' >&2
  exit 1
fi

signup_json="$(
  curl -fsS --max-time 5 "${signup_url}&email=user@example.com"
)"
signup_client_id="$(printf '%s' "$signup_json" | json_field client_id)"
signup_client_secret="$(printf '%s' "$signup_json" | json_field client_secret)"
signup_api_key="$(printf '%s' "$signup_json" | json_field api_key)"

printf '%s' "$signup_json" | json_assert ok=true signed_up=true email=user@example.com

if curl -fsS --max-time 5 "${signup_url}&email=user@example.com" >/dev/null 2>&1; then
  printf 'signup endpoint accepted consumed signup seed\n' >&2
  exit 1
fi

curl -fsS --max-time 5 \
  -H "Authorization: Bearer $signup_api_key" \
  "http://127.0.0.1:$server_port/protected" \
  | json_assert ok=true authorized=true

curl -fsS --max-time 5 -u "$signup_client_id:$signup_client_secret" \
  "http://127.0.0.1:$server_port/protected" \
  | json_assert ok=true authorized=true

wait "$server_pid"
server_pid=

printf '%s\n' 'M2M credential and signup e2e passed'
