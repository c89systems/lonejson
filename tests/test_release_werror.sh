#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Rationale: release builds must treat project-owned warnings as errors so
# packaged consumers do not inherit warning debt from generated artifacts.

repo_root=$1
cmake_bin=$2
generator=$3
tmp_dir=$(mktemp -d "$workspace_root/build/test_release_werror.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

build_dir="$tmp_dir/build"
build_type=${4:-Release}
fake_lua_root="$tmp_dir/cpkt"

mkdir -p "$fake_lua_root/include" "$fake_lua_root/lib"
: >"$fake_lua_root/include/lua.h"
: >"$fake_lua_root/lib/liblua.so"

"$cmake_bin" -S "$repo_root" -B "$build_dir" \
  -G "$generator" \
  -DCMAKE_BUILD_TYPE="$build_type" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DLONEJSON_BUILD_TESTS=ON \
  -DLONEJSON_BUILD_EXAMPLES=ON \
  -DLONEJSON_CPKT_ROOT="$fake_lua_root" \
  >/dev/null

if [[ ! -f "$build_dir/compile_commands.json" ]]; then
  printf 'missing release compile_commands.json\n' >&2
  exit 1
fi

require_release_werror() {
  local source=$1

  if ! awk -v source="$repo_root/$source" '
    index($0, "\"command\":") { command = $0 }
    index($0, "\"file\": \"" source "\"") {
      found = 1
      if (index(command, "-Werror") == 0) {
        exit 1
      }
    }
    END {
      if (!found) {
        exit 2
      }
    }
  ' "$build_dir/compile_commands.json"; then
    printf 'release compile command for %s is missing -Werror\n' "$source" >&2
    exit 1
  fi
}

require_release_werror src/lonejson.c
require_release_werror tests/test_main.c
if [[ $(uname -s) == Linux ]]; then
  require_release_werror src/lua/lonejson_lua.c
fi

case "$(uname -s)" in
  Darwin) linker_warning_flag=-Wl,-fatal_warnings ;;
  *) linker_warning_flag=-Wl,--fatal-warnings ;;
esac
grep -F -- "$linker_warning_flag" "$build_dir/build.ninja" >/dev/null
if [[ "$build_type" == Release ]]; then
  bash "$repo_root/tests/test_release_werror.sh" "$repo_root" "$cmake_bin" "$generator" Debug
fi

# Temporary downstream consumers are owned build outputs as well.
for file in scripts/smoke_darwin_release.sh cmake/package_darwin_smoke_bundle.cmake; do
  grep -F -- '-Wl,-fatal_warnings' "$repo_root/$file" >/dev/null
done
grep -F -- '-Wl,--fatal-warnings' "$repo_root/scripts/verify_release_archives.sh" >/dev/null
grep -F -- 'lonejson_configure_link_warnings(${consumer})' "$repo_root/scripts/verify_release_archives.sh" >/dev/null
grep -F -- 'lonejson_configure_link_warnings(lonejson_archive_adapter_consumer)' "$repo_root/scripts/verify_release_archives.sh" >/dev/null
