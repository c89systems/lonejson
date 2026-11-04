#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?usage: run_lua_benchmark.sh REPO_ROOT BUILD_DIR SCRIPT [ARGUMENT...]}
build_dir=${2:?usage: run_lua_benchmark.sh REPO_ROOT BUILD_DIR SCRIPT [ARGUMENT...]}
script_path=${3:?usage: run_lua_benchmark.sh REPO_ROOT BUILD_DIR SCRIPT [ARGUMENT...]}
shift 3
cd "$repo_root"

if [[ "$(uname -s)" == Linux ]]; then
  runner="$build_dir/lonejson_lua_target_runner"
  if [[ ! -x "$runner" ]]; then
    printf 'missing Bootlin Lua target runner: %s\n' "$runner" >&2
    exit 1
  fi
  module_build_dir="$build_dir/lua-benchmark"
  if [[ ! -f "$module_build_dir/lua-target/lonejson/core.so" ]]; then
    printf 'missing production Lua benchmark module under %s\n' "$module_build_dir" >&2
    exit 1
  fi
  exec env LONEJSON_LUA_TARGET_RUNNER="$runner" \
    LONEJSON_LUA_SOURCE_DIR="$repo_root" LONEJSON_LUA_BUILD_DIR="$module_build_dir" \
    "$runner" "$repo_root" "$module_build_dir" -- "$script_path" "$@"
fi

lua_bin=${LUA:?set LUA to a native Lua interpreter}
luarocks_bin=${LUAROCKS:-luarocks}
lua_tree=${LUA_ROCK_TREE:?set LUA_ROCK_TREE for the native Lua rock tree}
lua_library_dir=${LONEJSON_LUA_BENCH_LIBDIR:?set LONEJSON_LUA_BENCH_LIBDIR}

resolved_environment=$("$luarocks_bin" path --tree "$lua_tree") || exit $?
eval "$resolved_environment"
exec env DYLD_LIBRARY_PATH="$lua_library_dir:${DYLD_LIBRARY_PATH-}" \
  "$lua_bin" "$script_path" "$@"
