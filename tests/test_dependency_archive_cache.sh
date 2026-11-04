#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/dependency-cache-contract.XXXXXX")
server_pid=
cleanup() {
  if [[ -n "$server_pid" ]]; then
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
  rm -rf -- "$work"
}
trap cleanup EXIT
printf 'verified immutable synthetic archive\n' >"$work/payload"
: >"$work/requests"
python3 -B "$repo_root/tests/fixtures/http_origin.py" "$work/payload" "$work/requests" "$work/port" >"$work/origin.log" 2>&1 &
server_pid=$!
for ((attempt=0; attempt<100; attempt++)); do
  [[ ! -s "$work/port" ]] || break
  kill -0 "$server_pid" || { cat "$work/origin.log" >&2; exit 1; }
  sleep 0.05
done
[[ -s "$work/port" ]] || { printf 'origin fixture did not become ready\n' >&2; exit 1; }
url="http://127.0.0.1:$(cat "$work/port")"
digest=$(sha256sum "$work/payload" | cut -d ' ' -f1)
cache="$work/cache"
published="$cache/archives/sha256/$digest"
acquire() {
  local name=$1 route=$2 source=$3 expected=${4:-$digest}
  mkdir -p "$work/$source"
  cat >"$work/$source/acquire.cmake" <<CMAKE
cmake_minimum_required(VERSION 3.21)
include("$repo_root/cmake/CpktDependencyCache.cmake")
cpkt_acquire_verified_archive(COMPONENT fixture URL "$url/$route"
 SHA256 "$expected" ARCHIVE_NAME "$name" RETRIES 2 OUTPUT_VARIABLE archive)
file(SHA256 "\${archive}" actual)
if(NOT actual STREQUAL "$expected")
 message(FATAL_ERROR "unverified archive returned")
endif()
CMAKE
  cmake "-DCPKT_DEPENDENCY_CACHE=$cache" -P "$work/$source/acquire.cmake" >"$work/acquire.log" 2>&1
}
expect_requests() {
  printf '%s\n' "$@" >"$work/expected"
  diff -u "$work/expected" "$work/requests"
}
acquire first.tar.gz first source-one
expect_requests /first
cmp "$work/payload" "$published/first.tar.gz"
: >"$work/requests"
acquire renamed.zip different-url source-two
[[ ! -s "$work/requests" ]]
printf 'corrupt cache entry' >"$published/renamed.zip"
acquire renamed.zip different-url source-three
[[ ! -s "$work/requests" ]]
cmp "$work/payload" "$published/renamed.zip"
rm "$published/first.tar.gz" "$published/renamed.zip"
for name in .hidden.tmp transfer.tmp.123 transfer.part-123 transfer.tmp; do
  cp "$work/payload" "$published/$name"
done
acquire complete.tar.gz fresh source-one
expect_requests /fresh
cmp "$work/payload" "$published/complete.tar.gz"
: >"$work/requests"
missing_digest=$(printf 'not served by origin' | sha256sum | cut -d ' ' -f1)
if acquire bad.tar.gz bad source-one "$missing_digest"; then
  printf 'corrupt origin bytes accepted\n' >&2; exit 1
fi
expect_requests /bad /bad
grep -F 'SHA256 mismatch' "$work/acquire.log" >/dev/null
[[ -z "$(ls -A "$cache/archives/sha256/$missing_digest")" ]]
printf 'PASS dependency archive digest reuse, zero requests, partial rejection, retry cleanup\n'
