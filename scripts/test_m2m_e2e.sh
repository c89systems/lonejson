#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir=${LONEJSON_E2E_BUILD_DIR:-$repo_root/build/host-curl}
server_port=${LONEJSON_M2M_E2E_SERVER_PORT:-0}
run_dir=
server_log=
server_pid=

cleanup() {
  local status=$?
  if [[ -n "$server_pid" ]]; then
    if kill -0 "$server_pid" >/dev/null 2>&1; then
      kill "$server_pid" >/dev/null 2>&1 || true
    fi
    wait "$server_pid" >/dev/null 2>&1 || true
  fi
  if [[ "$status" != 0 && -f "$server_log" ]]; then
    printf 'M2M fixture log: %s\n' "$server_log" >&2
    cat "$server_log" >&2
  elif [[ "$status" == 0 && -n "$run_dir" ]]; then
    rm -rf -- "$run_dir"
  fi
  exit "$status"
}
trap cleanup EXIT

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'missing required command for M2M e2e: %s\n' "$1" >&2
    exit 1
  fi
}

source "$repo_root/tests/json_helpers.sh"

fixture_curl() {
  command curl --noproxy '*' "$@"
}

expect_rejected() {
  local status
  status=$(fixture_curl -sS --max-time 5 -o "$run_dir/rejection.json" \
    -w '%{http_code}' "$@")
  if [[ "$status" != 401 ]]; then
    printf 'expected HTTP 401 from M2M fixture, got %s\n' "$status" >&2
    return 1
  fi
  json_assert ok=false < "$run_dir/rejection.json"
}

require_command curl
require_command cmake

if [[ ! -x "$build_dir/lonejson_m2m_fixture_server" ]]; then
  printf 'missing build/host-curl/lonejson_m2m_fixture_server; run make test-m2m-e2e\n' >&2
  exit 1
fi

umask 077
mkdir -p "$repo_root/build/devenv"
bash "$repo_root/tests/test_m2m_fixture.sh" "$repo_root" \
  "$build_dir/lonejson_m2m_fixture_server"
run_dir=$(mktemp -d "$repo_root/build/devenv/m2m-e2e.XXXXXX")
secrets_file="$run_dir/ready.json"
server_log="$run_dir/server.log"
"$build_dir/lonejson_m2m_fixture_server" \
  "$server_port" "$secrets_file" >"$server_log" 2>&1 &
server_pid=$!

for ((attempt = 0; attempt < 90; ++attempt)); do
  if ! kill -0 "$server_pid" >/dev/null 2>&1; then
    printf 'M2M fixture exited before readiness\n' >&2
    exit 1
  fi
  if [[ -s "$secrets_file" ]]; then
    break
  fi
  sleep 1
done
if [[ ! -s "$secrets_file" ]]; then
  printf 'timed out waiting for M2M fixture readiness\n' >&2
  exit 1
fi

server_port="$(json_field port <"$secrets_file")"
client_id="$(json_field client_id <"$secrets_file")"
client_secret="$(json_field client_secret <"$secrets_file")"
api_key="$(json_field api_key <"$secrets_file")"
signup_url="$(json_field signup_url <"$secrets_file")"

expect_rejected "http://127.0.0.1:$server_port/protected"

fixture_curl -fsS --max-time 5 -u "$client_id:$client_secret" \
  "http://127.0.0.1:$server_port/protected" \
  | json_assert ok=true authorized=true

fixture_curl -fsS --max-time 5 \
  -H "Authorization: Bearer $api_key" \
  "http://127.0.0.1:$server_port/protected" \
  | json_assert ok=true authorized=true

expect_rejected -u "$client_id:wrong-secret" \
  "http://127.0.0.1:$server_port/protected"
expect_rejected "${signup_url}&email=user@example.com&signup_secret=wrong"
expect_rejected "$signup_url"

signup_json="$(
  fixture_curl -fsS --max-time 5 "${signup_url}&email=user@example.com"
)"
signup_client_id="$(printf '%s' "$signup_json" | json_field client_id)"
signup_client_secret="$(printf '%s' "$signup_json" | json_field client_secret)"
signup_api_key="$(printf '%s' "$signup_json" | json_field api_key)"

printf '%s' "$signup_json" | json_assert ok=true signed_up=true email=user@example.com

expect_rejected "${signup_url}&email=user@example.com"

fixture_curl -fsS --max-time 5 \
  -H "Authorization: Bearer $signup_api_key" \
  "http://127.0.0.1:$server_port/protected" \
  | json_assert ok=true authorized=true

fixture_curl -fsS --max-time 5 -u "$signup_client_id:$signup_client_secret" \
  "http://127.0.0.1:$server_port/protected" \
  | json_assert ok=true authorized=true

fixture_curl -fsS --max-time 5 "http://127.0.0.1:$server_port/shutdown" \
  | json_assert ok=true stopped=true
wait "$server_pid"
server_pid=

printf '%s\n' 'M2M credential and signup e2e passed'
