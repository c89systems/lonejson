#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Rationale: SDK acquisition must be cache-stable, checksum-verified, and
# retryable without turning transient downloads into corrupted dependency roots.

repo_root=$1
tmp_dir=$(mktemp -d "$workspace_root/build/test_fetch_cpkt.XXXXXX")

bundle_dir_name="cpkt-0.6.1-x86_64-linux-gnu"
assets_dir="$tmp_dir/assets"
bundle_root="$assets_dir/$bundle_dir_name"
success_root="$tmp_dir/success-root"
failure_root="$tmp_dir/failure-root"
offline_root="$tmp_dir/offline-root"
corrupt_root="$tmp_dir/corrupt-root"
pinned_version_root="$tmp_dir/pinned-version-root"
concurrent_first_root="$tmp_dir/concurrent-first-root"
concurrent_second_root="$tmp_dir/concurrent-second-root"
archive_path="$assets_dir/$bundle_dir_name.tar.gz"
dependency_cache="$tmp_dir/dependency-cache"
failure_dependency_cache="$tmp_dir/failure-dependency-cache"
concurrent_dependency_cache="$tmp_dir/concurrent-dependency-cache"
first_pid=
second_pid=

cleanup() {
  for pid in "$first_pid" "$second_pid"; do
    if [[ -n "$pid" ]]; then
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
    fi
  done
  rm -rf "$tmp_dir"
}
trap cleanup EXIT

mkdir -p "$bundle_root/share/cpkt/packages"
cat >"$bundle_root/share/cpkt/packages/core.json" <<'EOF'
{"group":"core","release_version":"0.6.1","target_id":"x86_64-linux-gnu"}
EOF
mkdir -p \
  "$bundle_root/include/curl" \
  "$bundle_root/include/openssl" \
  "$bundle_root/include/nghttp2" \
  "$bundle_root/lib/cmake/CURL" \
  "$bundle_root/lib/cmake/OpenSSL" \
  "$bundle_root/lib/cmake/nghttp2" \
  "$bundle_root/lib/cmake/zlib" \
  "$bundle_root/lib/cmake/libssh2" \
  "$bundle_root/lib/pkgconfig"

cat >"$bundle_root/include/curl/curlver.h" <<'EOF'
#define LIBCURL_VERSION "8.22.0"
EOF
cat >"$bundle_root/include/openssl/opensslv.h" <<'EOF'
# define OPENSSL_VERSION_TEXT "OpenSSL 3.6.5 29 Sep 2026"
EOF
touch \
  "$bundle_root/include/lua.h" \
  "$bundle_root/lib/liblua.so" \
  "$bundle_root/include/nghttp2/nghttp2.h" \
  "$bundle_root/include/zlib.h" \
  "$bundle_root/lib/libcurl.so" \
  "$bundle_root/lib/libssl.so" \
  "$bundle_root/lib/libcrypto.so" \
  "$bundle_root/lib/cmake/CURL/CURLConfig.cmake" \
  "$bundle_root/lib/cmake/OpenSSL/OpenSSLConfig.cmake" \
  "$bundle_root/lib/cmake/nghttp2/nghttp2Config.cmake" \
  "$bundle_root/lib/cmake/zlib/ZLIBConfig.cmake" \
  "$bundle_root/lib/cmake/libssh2/libssh2-config.cmake" \
  "$bundle_root/lib/pkgconfig/libcurl.pc" \
  "$bundle_root/lib/pkgconfig/libcrypto.pc"

mkdir -p "$assets_dir"
tar -czf "$archive_path" -C "$assets_dir" "$bundle_dir_name"
archive_sha256=$(sha256sum "$archive_path" | awk '{print $1}')

success_log="$tmp_dir/success.log"
cmake \
  -D LONEJSON_SOURCE_DIR="$success_root" \
  -D LONEJSON_CPKT_TARGET_ID=x86_64-linux-gnu \
  -D LONEJSON_CPKT_BASE_URL="file://$assets_dir" \
  -D CPKT_DEPENDENCY_CACHE="$dependency_cache" \
  -D LONEJSON_CPKT_EXPECTED_SHA256_OVERRIDE="$archive_sha256" \
  -D LONEJSON_CPKT_DOWNLOAD_RETRIES=3 \
  -D LONEJSON_CPKT_RETRY_DELAY_SECONDS=0 \
  -D LONEJSON_CPKT_TEST_FAIL_DOWNLOAD_ATTEMPTS=2 \
  -P "$repo_root/cmake/fetch_cpkt.cmake" \
  >"$success_log" 2>&1

grep -q 'download attempt 1/3 failed' "$success_log"
grep -q 'download attempt 2/3 failed' "$success_log"
test -f "$success_root/.cache/cpkt/x86_64-linux-gnu/root/include/curl/curlver.h"
test -f "$success_root/.cache/cpkt/x86_64-linux-gnu/root/lib/cmake/CURL/CURLConfig.cmake"
test -f "$success_root/.cache/cpkt/x86_64-linux-gnu/root/lib/pkgconfig/libcurl.pc"
test -f "$success_root/.cache/cpkt/x86_64-linux-gnu/root/.lonejson-cpkt-identity"
test -f "$dependency_cache/archives/sha256/$archive_sha256/$bundle_dir_name.tar.gz"
test ! -e "$success_root/.cache/cpkt/x86_64-linux-gnu/$bundle_dir_name.tar.gz"

# A reusable extraction still has to carry the selected provider identity.
manifest="$success_root/.cache/cpkt/x86_64-linux-gnu/root/share/cpkt/packages/core.json"
cp "$manifest" "$tmp_dir/manifest.json"
for wrong in '{"group":"db","release_version":"0.6.1","target_id":"x86_64-linux-gnu"}' \
    '{"group":"core","release_version":"0.6.0","target_id":"x86_64-linux-gnu"}' \
    '{"group":"core","release_version":"0.6.1","target_id":"aarch64-linux-gnu"}' \
    '{}'; do
  printf '%s\n' "$wrong" >"$manifest"
  if cmake -D "LONEJSON_SOURCE_DIR=$success_root" \
      -D LONEJSON_CPKT_TARGET_ID=x86_64-linux-gnu \
      -D "LONEJSON_CPKT_BASE_URL=file://$assets_dir" \
      -D "LONEJSON_CPKT_EXPECTED_SHA256_OVERRIDE=$archive_sha256" \
      -P "$repo_root/cmake/fetch_cpkt.cmake" >"$tmp_dir/wrong-identity.log" 2>&1; then
    printf 'accepted incorrect cpkt provider identity\n' >&2; exit 1
  fi
  grep -E 'Wrong cpkt package identity|Invalid cpkt core package manifest' "$tmp_dir/wrong-identity.log" >/dev/null
done
cp "$tmp_dir/manifest.json" "$manifest"

# A local extraction is disposable: the verified shared archive supports a
# fresh offline extraction without consulting the network.
offline_log="$tmp_dir/offline.log"
cmake \
  -D LONEJSON_SOURCE_DIR="$offline_root" \
  -D LONEJSON_CPKT_TARGET_ID=x86_64-linux-gnu \
  -D LONEJSON_CPKT_BASE_URL="file://$tmp_dir/missing-assets" \
  -D CPKT_DEPENDENCY_CACHE="$dependency_cache" \
  -D LONEJSON_CPKT_EXPECTED_SHA256_OVERRIDE="$archive_sha256" \
  -P "$repo_root/cmake/fetch_cpkt.cmake" \
  >"$offline_log" 2>&1
test -f "$offline_root/.cache/cpkt/x86_64-linux-gnu/root/include/curl/curlver.h"

failure_log="$tmp_dir/failure.log"
if cmake \
  -D LONEJSON_SOURCE_DIR="$failure_root" \
  -D LONEJSON_CPKT_TARGET_ID=x86_64-linux-gnu \
  -D LONEJSON_CPKT_BASE_URL="file://$assets_dir" \
  -D CPKT_DEPENDENCY_CACHE="$failure_dependency_cache" \
  -D LONEJSON_CPKT_EXPECTED_SHA256_OVERRIDE="$archive_sha256" \
  -D LONEJSON_CPKT_DOWNLOAD_RETRIES=3 \
  -D LONEJSON_CPKT_RETRY_DELAY_SECONDS=0 \
  -D LONEJSON_CPKT_TEST_FAIL_DOWNLOAD_ATTEMPTS=3 \
  -P "$repo_root/cmake/fetch_cpkt.cmake" \
  >"$failure_log" 2>&1; then
  printf 'expected fetch_cpkt.cmake to fail after exhausting retries\n' >&2
  exit 1
fi

grep -q 'failed to acquire verified archive' "$failure_log"
test ! -e "$failure_root/.cache/cpkt/x86_64-linux-gnu/root/.lonejson-cpkt-identity"

# A corrupted global entry is rejected before extraction and is never treated
# as a cache hit.
printf '%s\n' corrupt >"$dependency_cache/archives/sha256/$archive_sha256/$bundle_dir_name.tar.gz"
corrupt_log="$tmp_dir/corrupt.log"
if cmake \
  -D LONEJSON_SOURCE_DIR="$corrupt_root" \
  -D LONEJSON_CPKT_TARGET_ID=x86_64-linux-gnu \
  -D LONEJSON_CPKT_BASE_URL="file://$tmp_dir/missing-assets" \
  -D CPKT_DEPENDENCY_CACHE="$dependency_cache" \
  -D LONEJSON_CPKT_EXPECTED_SHA256_OVERRIDE="$archive_sha256" \
  -P "$repo_root/cmake/fetch_cpkt.cmake" \
  >"$corrupt_log" 2>&1; then
  printf 'expected corrupt shared archive to be rejected\n' >&2
  exit 1
fi
grep -q 'discarding corrupt shared cache archive' "$corrupt_log"
test ! -e "$corrupt_root/.cache/cpkt/x86_64-linux-gnu/root/.lonejson-cpkt-identity"
test ! -e "$dependency_cache/archives/sha256/$archive_sha256/$bundle_dir_name.tar.gz"

pinned_version_log="$tmp_dir/pinned-version.log"
if cmake \
  -D LONEJSON_SOURCE_DIR="$pinned_version_root" \
  -D CPKT_DEPENDENCY_CACHE="$tmp_dir/pinned-version-cache" \
  -D LONEJSON_CPKT_VERSION=0.2.0 \
  -D LONEJSON_CPKT_TARGET_ID=x86_64-linux-gnu \
  -D LONEJSON_CPKT_BASE_URL="file://$assets_dir" \
  -P "$repo_root/cmake/fetch_cpkt.cmake" \
  >"$pinned_version_log" 2>&1; then
  printf 'expected cpkt version override to fail before download\n' >&2
  exit 1
fi

grep -q 'LONEJSON_CPKT_VERSION is not configurable' "$pinned_version_log"
test ! -e "$pinned_version_root/.cache/cpkt"

# Two independent consumers must never observe a partial archive in the shared
# cache.  Hold the first process after it owns the per-digest lock, then start
# the second consumer and prove it reuses the atomically published archive.
fetch_concurrent() {
  local source_root=$1
  local log_path=$2
  local hold_seconds=${3:-}
  local -a command=(
    cmake
    -D "LONEJSON_SOURCE_DIR=$source_root"
    -D LONEJSON_CPKT_TARGET_ID=x86_64-linux-gnu
    -D "LONEJSON_CPKT_BASE_URL=file://$assets_dir"
    -D "CPKT_DEPENDENCY_CACHE=$concurrent_dependency_cache"
    -D "LONEJSON_CPKT_EXPECTED_SHA256_OVERRIDE=$archive_sha256"
  )
  if [[ -n "$hold_seconds" ]]; then
    command+=( -D "CPKT_DEPENDENCY_CACHE_TEST_HOLD_LOCK_SECONDS=$hold_seconds" )
  fi
  command+=( -P "$repo_root/cmake/fetch_cpkt.cmake" )
  "${command[@]}" >"$log_path" 2>&1
}

first_log="$tmp_dir/concurrent-first.log"
second_log="$tmp_dir/concurrent-second.log"
fetch_concurrent "$concurrent_first_root" "$first_log" 2 &
first_pid=$!
for _ in $(seq 1 100); do
  if grep -q 'test hook holding shared archive cache lock' "$first_log"; then
    break
  fi
  if ! kill -0 "$first_pid" 2>/dev/null; then
    cat "$first_log" >&2
    exit 1
  fi
  sleep 0.05
done
if ! grep -q 'test hook holding shared archive cache lock' "$first_log"; then
  printf 'first concurrent fetch did not acquire the shared cache lock\n' >&2
  cat "$first_log" >&2
  exit 1
fi

fetch_concurrent "$concurrent_second_root" "$second_log" &
second_pid=$!
if ! wait "$first_pid"; then
  cat "$first_log" >&2
  exit 1
fi
first_pid=
if ! wait "$second_pid"; then
  cat "$second_log" >&2
  exit 1
fi
second_pid=

concurrent_archive="$concurrent_dependency_cache/archives/sha256/$archive_sha256/$bundle_dir_name.tar.gz"
test -f "$concurrent_archive"
[[ "$(sha256sum "$concurrent_archive" | awk '{print $1}')" == "$archive_sha256" ]]
test -f "$concurrent_first_root/.cache/cpkt/x86_64-linux-gnu/root/.lonejson-cpkt-identity"
test -f "$concurrent_second_root/.cache/cpkt/x86_64-linux-gnu/root/.lonejson-cpkt-identity"
grep -q 'reusing verified shared archive cache entry' "$second_log"
if find "$(dirname -- "$concurrent_archive")" -maxdepth 1 -type f -name '.*.tmp' -print -quit | grep -q .; then
  printf 'concurrent cache acquisition left a temporary archive behind\n' >&2
  exit 1
fi
