#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

if [[ $# -lt 1 || $# -gt 4 ]]; then
  printf 'usage: %s <src-rock> [lonejson-libdir] [lua] [luarocks]\n' "$0" >&2
  exit 1
fi

rock_path=$1
lonejson_libdir=${2:-${LONEJSON_LIBDIR:-}}
lua_exec=${3:-${LUA:-lua}}
luarocks_exec=${4:-${LUAROCKS:-luarocks}}
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp_dir=

if [[ ! -f "$rock_path" ]]; then
  printf 'smoke_lua_src_rock.sh: source rock not found: %s\n' "$rock_path" >&2
  exit 1
fi

if ! command -v unzip >/dev/null 2>&1; then
  printf 'smoke_lua_src_rock.sh: unzip not found in PATH\n' >&2
  exit 1
fi

scan_for_private_paths() {
  local scan_root=$1
  local label=$2

  if grep -RIE 'file://(/|~|[A-Za-z]:)' "$scan_root" >/dev/null 2>&1; then
    printf 'smoke_lua_src_rock.sh: %s contains an absolute local file:// URL\n' "$label" >&2
    return 1
  fi
  if grep -RIF "$repo_root" "$scan_root" >/dev/null 2>&1; then
    printf 'smoke_lua_src_rock.sh: %s contains the repository path\n' "$label" >&2
    return 1
  fi
  if [[ -n "${HOME:-}" ]] && grep -RIF "$HOME" "$scan_root" >/dev/null 2>&1; then
    printf 'smoke_lua_src_rock.sh: %s contains HOME path\n' "$label" >&2
    return 1
  fi
}

cleanup() {
  if [[ -n "$tmp_dir" && -d "$tmp_dir" ]]; then
    rm -rf -- "$tmp_dir"
  fi
}

trap cleanup EXIT INT TERM

tmp_dir=$(mktemp -d "$workspace_root/build/smoke_lua_src_rock.XXXXXX")
rock_unpack_dir="$tmp_dir/src-rock"
nested_unpack_dir="$tmp_dir/nested-source"

mkdir -p "$rock_unpack_dir" "$nested_unpack_dir"
unzip -q "$rock_path" -d "$rock_unpack_dir"
scan_for_private_paths "$rock_unpack_dir" "source rock"

mapfile -t rockspecs < <(find "$rock_unpack_dir" -maxdepth 1 -type f -name '*.rockspec' | LC_ALL=C sort)
if [[ "${#rockspecs[@]}" -ne 1 ]]; then
  printf 'smoke_lua_src_rock.sh: expected exactly one rockspec in %s, found %s\n' \
    "$rock_path" "${#rockspecs[@]}" >&2
  exit 1
fi

"$lua_exec" - "${rockspecs[0]}" <<'LUA'
local spec = {}
assert(loadfile(arg[1], "t", spec))()
local url = spec.source and spec.source.url
assert(type(url) == "string" and
       (url:match("^https://") or url:match("^git%+https://")),
       "source rock must declare a public HTTPS source URL")
LUA

mapfile -t source_archives < <(
  find "$rock_unpack_dir" -maxdepth 1 -type f \( -name '*.tar.gz' -o -name '*.tgz' \) |
    LC_ALL=C sort
)
if [[ "${#source_archives[@]}" -ne 1 ]]; then
  printf 'smoke_lua_src_rock.sh: expected exactly one nested source archive in %s, found %s\n' \
    "$rock_path" "${#source_archives[@]}" >&2
  exit 1
fi

tar -xzf "${source_archives[0]}" -C "$nested_unpack_dir"
scan_for_private_paths "$nested_unpack_dir" "nested source archive"

if [[ -z "$lonejson_libdir" ]]; then
  printf 'smoke_lua_src_rock.sh: LONEJSON_LIBDIR or lonejson-libdir is required for the external liblonejson dependency\n' >&2
  exit 1
fi
if [[ ! -d "$lonejson_libdir" ]]; then
  printf 'smoke_lua_src_rock.sh: liblonejson directory not found: %s\n' "$lonejson_libdir" >&2
  exit 1
fi

mkdir -p "$tmp_dir/tool-tmp"
TMPDIR="$tmp_dir/tool-tmp" LONEJSON_LIBDIR="$lonejson_libdir" "$luarocks_exec" install --tree "$tmp_dir/tree" "$rock_path"
resolved_environment=$("$luarocks_exec" path --tree "$tmp_dir/tree") || exit $?
eval "$resolved_environment"
printf '%s\n' 'assert(require("lonejson.init"))' >"$tmp_dir/smoke.lua"
"$repo_root/scripts/run_installed_lua.sh" "$tmp_dir/tree" "$lonejson_libdir" "$lua_exec" \
  "$tmp_dir/smoke.lua" "$repo_root/tests/test_lua.lua" "$repo_root/tests/test_lua_fuzz.lua"
