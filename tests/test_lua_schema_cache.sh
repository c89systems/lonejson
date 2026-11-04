#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

repo_root=$1
lua_exec=${2:-lua}
luarocks_exec=${3:-luarocks}
libdir=${4:-${LONEJSON_LIBDIR:-"$repo_root/build/lua-sdk/lib"}}
tmp_dir=$(mktemp -d "$workspace_root/build/test_lua_schema_cache.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT
export TMPDIR="$tmp_dir"

rock_tree="$tmp_dir/luarocks"
rockspec="$tmp_dir/lonejson-0.0.0-1.rockspec"

mkdir -p "$rock_tree"

(
  cd "$repo_root"
  lib_ext="$("$luarocks_exec" config variables.LIB_EXTENSION)"
  ./scripts/render_release_rockspec.sh \
    "0.0.0" "$rockspec" "git+file://$repo_root" "" "$lib_ext"
  CFLAGS="${CFLAGS:+$CFLAGS }-DLONEJSON_TEST_MAP_ANALYSIS_COUNTER=1" \
  LONEJSON_LIBDIR="$libdir" \
    "$luarocks_exec" make --tree "$rock_tree" "$rockspec" >/dev/null
)

resolved_environment=$("$luarocks_exec" path --tree "$rock_tree") || exit $?
eval "$resolved_environment"
"$repo_root/scripts/run_installed_lua.sh" "$rock_tree" "$libdir" "$lua_exec" "$repo_root/tests/test_lua_schema_cache.lua"
