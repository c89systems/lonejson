#!/usr/bin/env bash
set -euo pipefail
repo_root=$1
lua_exec=${2:-lua}
luarocks_exec=${3:-luarocks}
runner_build=${4:-}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/bench-measurement.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
if [[ $(uname -s) == Linux ]]; then
  "$repo_root/scripts/run_lua_benchmark.sh" "$repo_root" "$runner_build" \
    tests/test_bench_measurement.lua "$repo_root" "$work"
else
  resolved_environment=$("$luarocks_exec" path --tree "$repo_root/build/luarocks")
  eval "$resolved_environment"
  export DYLD_LIBRARY_PATH="${LONEJSON_LIBDIR:-$repo_root/build/lua-sdk/lib}:${DYLD_LIBRARY_PATH:-}"
  export LONEJSON_BENCH_TOOLCHAIN_FILE="$runner_build/benchmark-toolchain.json"
  cd "$repo_root"
  "$repo_root/scripts/run_benchmark.sh" "$lua_exec" \
    "$repo_root/tests/test_bench_measurement.lua" "$repo_root" "$work"
fi
