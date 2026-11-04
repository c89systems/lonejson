#!/usr/bin/env bash
set -euo pipefail
[[ $# -ge 3 ]] || { printf 'usage: %s LUA LUAROCKS SCRIPT...\n' "$0" >&2; exit 2; }
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
lua_bin=${LUA:-$1}
luarocks_bin=${LUAROCKS:-$2}
shift 2
cd "$repo_root"
"$repo_root/scripts/require_build_workspace.sh" "$repo_root/build/test-tmp"
mkdir -p "$repo_root/build/test-tmp"
export LONEJSON_TEST_TEMP_DIR="$repo_root/build/test-tmp"
rock_path=$("$luarocks_bin" path --tree "$repo_root/build/luarocks")
eval "$rock_path"
export DYLD_LIBRARY_PATH="${LONEJSON_LIBDIR:-$repo_root/build/lua-sdk/lib}:${DYLD_LIBRARY_PATH:-}"
for script in "$@"; do "$lua_bin" "$script"; done
