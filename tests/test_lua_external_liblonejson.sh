#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

repo_root=$1
lua_exec=${2:-lua}
luarocks_exec=${3:-luarocks}
libdir=${4:-"$repo_root/build/lua-sdk/lib"}
tmp_dir=$(mktemp -d "$workspace_root/build/test_lua_external_liblonejson.XXXXXX")
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
  LONEJSON_LIBDIR="$libdir" \
    "$luarocks_exec" make --tree "$rock_tree" "$rockspec" CC=/bin/false >/dev/null
)

module_path=$(
  find "$rock_tree" -type f -path '*/lonejson/core.*' | LC_ALL=C sort | head -n 1
)
if [[ -z "$module_path" || ! -f "$module_path" ]]; then
  printf 'Lua core module was not built in %s\n' "$rock_tree" >&2
  exit 1
fi

target_id=$("$repo_root/scripts/detect_native_target.sh")
target_tools=$("$repo_root/scripts/discover_target_tools.sh" \
  --build-dir "$repo_root/build/$target_id-release" --target-id "$target_id")
eval "$target_tools"
system=$(uname -s)
bash "$repo_root/scripts/check_library_exports.sh" "$NM" "$system" "$module_path" \
  "$repo_root/cmake/lonejson_lua.exports"
if [[ "$system" == Linux ]]; then
  "$READELF" -d "$module_path" | grep -E 'Shared library: \[liblonejson\.so' >/dev/null
else
  "$OTOOL" -L "$module_path" | grep -E 'liblonejson\.(dylib|[0-9]+\.dylib)' >/dev/null
fi

resolved_environment=$("$luarocks_exec" path --tree "$rock_tree") || exit $?
eval "$resolved_environment"
printf '%s\n' 'assert(require("lonejson"))' >"$tmp_dir/smoke.lua"
"$repo_root/scripts/run_installed_lua.sh" "$rock_tree" "$libdir" "$lua_exec" "$tmp_dir/smoke.lua"
