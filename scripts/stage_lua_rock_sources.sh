#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  printf 'usage: %s <repo-root> <stage-dir> <release-version>\n' "$0" >&2
  exit 1
fi

repo_root=$1
stage_dir=$2
workspace_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
"$workspace_root/scripts/require_build_workspace.sh" "$stage_dir"
release_version=$3

static_files=(
  LICENSE
  README.md
  lonejson.rockspec.in
  scripts/build_lua_rock.sh
  scripts/cpkt-toolchains.sh
  scripts/cpkt-native-apple.sh
  scripts/require_cache_path.sh
  tools/lifecycle/scripts/require-host-bash.sh
  tools/lifecycle/scripts/cpkt-archive-cache.sh
  tools/lifecycle/scripts/cpkt-toolchains.sh
  tools/lifecycle/scripts/cpkt-aflpp.sh
  tools/lifecycle/scripts/cpkt-afl-runtime.sh
  scripts/deps.sh
  cmake/fetch_cpkt.cmake
  cmake/CpktDependencyCache.cmake
  cmake/cpkt_metadata.cmake
  cmake/lonejson_lua.exports
  cmake/lonejson.exports
  scripts/check_library_exports.sh
  scripts/detect_native_bootlin_target.sh
  cmake/toolchains/print_native_bootlin_target.cmake
  cmake/toolchains/lonejson_native_bootlin_target.cmake
  scripts/render_release_rockspec.sh
  include/lonejson.h
  src/lua/lonejson_lua.c
  lua/lonejson/init.lua
)

files=()
include_scan_queue=()
include_scan_seen=()

has_entry() {
  local needle=$1
  shift
  local entry
  for entry in "$@"; do
    if [[ "$entry" == "$needle" ]]; then
      return 0
    fi
  done
  return 1
}

append_file() {
  local path=$1
  if ! has_entry "$path" "${files[@]}"; then
    files+=("$path")
  fi
}

relative_path() {
  local base=$1
  local path=$2
  local abs_base
  local abs_dir
  local abs_path

  abs_base=$(cd "$base" && pwd -P)
  abs_dir=$(cd "$(dirname "$path")" && pwd -P)
  abs_path="$abs_dir/$(basename "$path")"

  case "$abs_path" in
    "$abs_base"/*)
      printf '%s\n' "${abs_path#"$abs_base"/}"
      ;;
    *)
      return 1
      ;;
  esac
}

enqueue_include_scan() {
  local path=$1
  case "$path" in
    *.c|*.h)
      if ! has_entry "$path" "${include_scan_seen[@]}"; then
        include_scan_seen+=("$path")
        include_scan_queue+=("$path")
      fi
      ;;
  esac
}

resolve_local_include() {
  local rel_file=$1
  local include_path=$2
  local rel_dir
  local candidate
  rel_dir=$(dirname "$rel_file")

  for candidate in \
    "$rel_dir/$include_path" \
    "include/$include_path" \
    "src/$include_path"; do
    if [[ -f "$repo_root/$candidate" ]]; then
      relative_path "$repo_root" "$repo_root/$candidate"
      return 0
    fi
  done

  return 1
}

for path in "${static_files[@]}"; do
  append_file "$path"
done

enqueue_include_scan src/lua/lonejson_lua.c
scan_index=0
while [[ "$scan_index" -lt "${#include_scan_queue[@]}" ]]; do
  rel_file=${include_scan_queue[$scan_index]}
  scan_index=$((scan_index + 1))

  if [[ ! -f "$repo_root/$rel_file" ]]; then
    printf 'missing Lua rock source input: %s\n' "$rel_file" >&2
    exit 1
  fi

  while IFS= read -r line; do
    if [[ $line =~ ^[[:space:]]*#[[:space:]]*include[[:space:]]*\"([^\"]+)\" ]]; then
      include_path=${BASH_REMATCH[1]}
      if ! resolved_include=$(resolve_local_include "$rel_file" "$include_path"); then
        printf 'unresolved Lua rock source include: %s includes %s\n' \
          "$rel_file" "$include_path" >&2
        exit 1
      fi
      append_file "$resolved_include"
      enqueue_include_scan "$resolved_include"
    fi
  done <"$repo_root/$rel_file"
done

rm -rf "$stage_dir"
mkdir -p "$stage_dir"

for path in "${files[@]}"; do
  if [[ ! -f "$repo_root/$path" ]]; then
    printf 'missing Lua rock source input: %s\n' "$path" >&2
    exit 1
  fi
  mkdir -p "$stage_dir/$(dirname "$path")"
  cp -p "$repo_root/$path" "$stage_dir/$path"
done

missing=0
while IFS= read -r staged_file; do
  rel_file=${staged_file#"$stage_dir"/}
  rel_dir=$(dirname "$rel_file")
  while IFS= read -r line; do
    if [[ $line =~ ^[[:space:]]*#[[:space:]]*include[[:space:]]*\"([^\"]+)\" ]]; then
      include_path=${BASH_REMATCH[1]}
      if [[ ! -f "$stage_dir/$rel_dir/$include_path" \
            && ! -f "$stage_dir/include/$include_path" \
            && ! -f "$stage_dir/src/$include_path" ]]; then
        printf 'unresolved Lua rock source include: %s includes %s\n' \
          "$rel_file" "$include_path" >&2
        missing=1
      fi
    fi
  done <"$staged_file"
done < <(find "$stage_dir" -type f \( -name '*.c' -o -name '*.h' \) | LC_ALL=C sort)

if [[ "$missing" -ne 0 ]]; then
  exit 1
fi

printf '%s\n' "$release_version" >"$stage_dir/VERSION"
printf '%s\n' "${files[@]}" VERSION >"$stage_dir/RELEASE_MANIFEST"
