#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/fuzz-inventory.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
plan=$(make --no-print-directory -C "$repo_root" -n fuzz)
grep -F 'cmake --build --preset fuzz --target lonejson_fuzz_targets' <<<"$plan" >/dev/null
grep -F 'ctest --preset fuzz' <<<"$plan" >/dev/null
grep -F 'make lua-fuzz' <<<"$plan" >/dev/null
plan=$(make --no-print-directory -C "$repo_root" -n fuzz-smoke)
grep -F 'LONEJSON_FUZZ_EXECUTIONS=1000' <<<"$plan" >/dev/null
# The Make surface is portable; the real AFL collection is native Linux x86_64.
if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
  exit 0
fi
bundle=$("$repo_root/scripts/detect_cpkt_bundle.sh")
cmake -S "$repo_root" -B "$work/build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$repo_root/cmake/toolchains/linux-x86_64-aflpp.cmake" \
  -DLONEJSON_BUILD_TESTS=OFF -DLONEJSON_BUILD_EXAMPLES=OFF -DLONEJSON_BUILD_FUZZERS=ON \
  -DLONEJSON_BUILD_WITH_CURL=ON -DLONEJSON_BUILD_WITH_OPENSSL=ON \
  -DLONEJSON_BUILD_WITH_JWT=ON -DLONEJSON_BUILD_WITH_OIDC=ON \
  -DLONEJSON_CPKT_ROOT="$bundle" >"$work/configure.log" 2>&1
ctest --test-dir "$work/build" --show-only=json-v1 >"$work/inventory.json"
cmake -DINVENTORY="$work/inventory.json" -P "$repo_root/tests/check_fuzz_inventory.cmake"
ninja -C "$work/build" -t graph lonejson_fuzz_targets >"$work/graph"
grep -F 'lonejson_fuzz_writer_value_stream' "$work/graph" >/dev/null
if grep -F 'liblonejson.so' "$work/graph"; then
  printf 'fuzz aggregate includes an unused shared-library producer\n' >&2; exit 1
fi
