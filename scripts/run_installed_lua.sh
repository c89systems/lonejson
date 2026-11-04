#!/usr/bin/env bash
set -euo pipefail
if [[ $# -lt 4 ]]; then
  printf "usage: run_installed_lua.sh ROCK_TREE CORE_LIBDIR LUA SCRIPT...\n" >&2
  exit 1
fi
tree=${1:?usage: run_installed_lua.sh ROCK_TREE CORE_LIBDIR LUA SCRIPT...}
core_libdir=${2:?}
lua=${3:?}
shift 3
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
core_libdir=$(CDPATH= cd -- "$core_libdir" && pwd -P)
mkdir -p "$root/build"
workspace=$(mktemp -d "$root/build/installed-lua.XXXXXX")
trap 'rm -rf "$workspace"' EXIT
if [[ "$(uname -s)" != Linux ]]; then
  mkdir -p "$workspace/temp"
  export LONEJSON_TEST_TEMP_DIR="$workspace/temp"
  for script in "$@"; do
    DYLD_LIBRARY_PATH="$core_libdir:${DYLD_LIBRARY_PATH:-}" "$lua" "$script"
  done
  exit 0
fi
target_id=$("$root/scripts/detect_native_bootlin_target.sh")
"$root/scripts/deps.sh" "$target_id" >/dev/null
bundle_root="$root/.cache/cpkt/$target_id/root"
cmake -S "$root/cmake/lua_sdk_smoke" -B "$workspace/runner" -G Ninja \
  -D "CMAKE_TOOLCHAIN_FILE=$root/cmake/toolchains/native.cmake" \
  -D "LONEJSON_REPO_ROOT=$root" -D "LONEJSON_CORE_LIBDIR=$core_libdir" \
  -D "LONEJSON_CPKT_ROOT=$bundle_root" >/dev/null
cmake --build "$workspace/runner" >/dev/null
"$root/tests/test_bootlin_runtime.sh" "$workspace/runner" "$workspace/runner/installed_lua"
module=$(find "$tree" -type f -path '*/lonejson/core.so' | LC_ALL=C sort | sed -n '1p')
facade=$(find "$tree" -type f -path '*/lonejson/init.lua' | LC_ALL=C sort | sed -n '1p')
[[ -f "$module" && -f "$facade" ]]
mkdir -p "$workspace/runtime/lua-target/lonejson" "$workspace/runtime/lua/lonejson" \
  "$workspace/runtime/build/test-tmp"
cp "$module" "$workspace/runtime/lua-target/lonejson/core.so"
cp "$facade" "$workspace/runtime/lua/lonejson/init.lua"
"$workspace/runner/installed_lua" "$workspace/runtime" "$workspace/runtime" "$@"
