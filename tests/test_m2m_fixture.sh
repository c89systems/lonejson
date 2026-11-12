#!/usr/bin/env bash
set -euo pipefail
repo_root=$1
fixture=$2
source "$repo_root/tests/json_helpers.sh"
umask 077
mkdir -p "$repo_root/build/test-tmp"
work=$(mktemp -d "$repo_root/build/test-tmp/m2m-fixture.XXXXXX")
pids=()

cleanup() {
  local status=$? pid
  for pid in "${pids[@]}"; do
    if kill -0 "$pid" 2>/dev/null; then kill "$pid" 2>/dev/null || true; fi
    wait "$pid" 2>/dev/null || true
  done
  if [[ "$status" == 0 ]]; then
    rm -rf -- "$work"
  else
    printf 'M2M fixture regression logs: %s\n' "$work" >&2
  fi
  exit "$status"
}
trap cleanup EXIT

fail() { printf '%s\n' "$*" >&2; exit 1; }
fixture_curl() { command curl --noproxy '*' --max-time 5 "$@"; }

start_fixture() {
  local name=$1 attempt
  "$fixture" 0 "$work/$name.json" >"$work/$name.log" 2>&1 &
  fixture_pid=$!
  pids+=("$fixture_pid")
  for ((attempt = 0; attempt < 200; ++attempt)); do
    kill -0 "$fixture_pid" 2>/dev/null || fail "$name fixture exited before readiness"
    [[ ! -s "$work/$name.json" ]] || break
    sleep 0.05
  done
  [[ -s "$work/$name.json" ]] || fail "$name fixture did not publish readiness"
  [[ ! -e "$work/$name.json.tmp" ]] || fail 'readiness was not published atomically'
  fixture_port=$(json_field port <"$work/$name.json")
  [[ "$fixture_port" =~ ^[0-9]+$ && "$fixture_port" -gt 0 && "$fixture_port" -le 65535 ]]
  fixture_curl -fsS "http://127.0.0.1:$fixture_port/health" | json_assert ok=true
  [[ $(json_field signup_url <"$work/$name.json") == "http://127.0.0.1:$fixture_port/signup?"* ]]
}

start_fixture first
first_pid=$fixture_pid first_port=$fixture_port
start_fixture second
second_pid=$fixture_pid second_port=$fixture_port
[[ "$first_port" != "$second_port" ]] || fail 'live fixtures shared an ephemeral port'

# The first fixture owns this port throughout the negative check: no port probe.
if "$fixture" "$first_port" "$work/collision.json" >"$work/collision.log" 2>&1; then
  fail 'fixture accepted an occupied port'
else
  [[ "$?" == 1 ]] || fail 'occupied-port failure returned the wrong status'
fi
[[ ! -e "$work/collision.json" && ! -e "$work/collision.json.tmp" ]]
grep -F 'bind M2M fixture:' "$work/collision.log" >/dev/null

for port in -1 65536 invalid; do
  if "$fixture" "$port" "$work/invalid.json" >"$work/invalid.log" 2>&1; then
    fail 'fixture accepted an invalid port'
  else
    [[ "$?" == 2 ]] || fail 'invalid-port failure returned the wrong status'
  fi
  [[ ! -e "$work/invalid.json" ]]
done

if "$fixture" 0 "$work/missing/ready.json" >"$work/publication.log" 2>&1; then
  fail 'fixture accepted an unwritable readiness path'
else
  [[ "$?" == 1 ]] || fail 'publication failure returned the wrong status'
fi
grep -F 'open M2M readiness file:' "$work/publication.log" >/dev/null

# Retries and dropped responses must never consume the fixture's lifetime.
for ((probe = 0; probe < 24; ++probe)); do
  fixture_curl -fsS "http://127.0.0.1:$first_port/health" >/dev/null
done
kill -0 "$first_pid"

# A request may arrive across multiple TCP reads. Do not answer partial headers.
exec 9<>"/dev/tcp/127.0.0.1/$first_port"
printf 'GET /health HTTP/1.1\r\nHost: localhost\r\n' >&9
if IFS= read -r -t 0.2 response <&9; then
  fail 'fixture answered before receiving complete HTTP headers'
fi
printf '\r\n' >&9
IFS= read -r -t 5 response <&9
[[ "$response" == 'HTTP/1.1 200 '* ]]
exec 9>&-

# Abandoned requests and the bounded header limit leave the listener usable.
exec 9<>"/dev/tcp/127.0.0.1/$first_port"
printf 'GET /health HTTP/1.1\r\n' >&9
exec 9>&-
exec 9<>"/dev/tcp/127.0.0.1/$first_port"
printf -v oversized '%08191d' 0
printf '%s' "$oversized" >&9
IFS= read -r -t 5 response <&9
[[ "$response" == 'HTTP/1.1 400 '* ]]
exec 9>&-
fixture_curl -fsS "http://127.0.0.1:$first_port/health" | json_assert ok=true
fixture_curl -fsS "http://127.0.0.1:$second_port/health" | json_assert ok=true

fixture_curl -fsS "http://127.0.0.1:$first_port/shutdown" | json_assert ok=true stopped=true
wait "$first_pid"
pids=("$second_pid")
fixture_curl -fsS "http://127.0.0.1:$second_port/shutdown" | json_assert ok=true stopped=true
wait "$second_pid"
pids=()
printf '%s\n' 'M2M fixture startup, isolation, request framing and shutdown regressions passed'
