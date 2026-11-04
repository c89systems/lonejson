#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Rationale: Lua native test targets must stay selectable without accidentally
# expanding into cross-target or non-native release work.

repo_root=$1
cmake_cmd=$2
cmake_generator=$3

lua_exec="$(command -v lua || true)"
luarocks_exec="$(command -v luarocks || true)"
if [ -z "$lua_exec" ] || [ -z "$luarocks_exec" ]; then
  printf 'missing required lua or luarocks for native test target filter\n'
  exit 1
fi

tmp_dir="$(mktemp -d "$workspace_root/build/test_lua_native_test_target_filter.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

configure_target() {
  local build_dir=$1
  local libc=$2

  "$cmake_cmd" -S "$repo_root" -B "$build_dir" -G "$cmake_generator" \
    -D LONEJSON_TARGET_OS=linux \
    -D "LONEJSON_TARGET_LIBC=$libc" \
    -D "LONEJSON_LUA_EXECUTABLE=$lua_exec" \
    -D "LONEJSON_LUAROCKS_EXECUTABLE=$luarocks_exec" \
    -D "LONEJSON_CPKT_ROOT=${3:-}" \
    -D LONEJSON_BUILD_TESTS=ON >/dev/null
}

list_tests() {
  local build_dir=$1

  ctest --test-dir "$build_dir" --show-only=json-v1
}

assert_has_native_lua_tests() {
  local tests=$1

  printf '%s\n' "$tests" | grep -F 'lonejson_bench_baseline_history_tests' >/dev/null
  printf '%s\n' "$tests" | grep -F 'lonejson_bench_retry_confirm_tests' >/dev/null
  printf '%s\n' "$tests" | grep -F 'lonejson_lua_rock_makefile_deps_tests' >/dev/null
}

assert_lacks_native_lua_tests() {
  local tests=$1

  if printf '%s\n' "$tests" | grep -F 'lonejson_bench_baseline_history_tests' >/dev/null; then
    printf 'bench baseline Lua workflow tests must not run for non-host libc targets\n' >&2
    exit 1
  fi
  if printf '%s\n' "$tests" | grep -F 'lonejson_bench_retry_confirm_tests' >/dev/null; then
    printf 'bench retry Lua workflow tests must not run for non-host libc targets\n' >&2
    exit 1
  fi
  if printf '%s\n' "$tests" | grep -F 'lonejson_lua_rock_makefile_deps_tests' >/dev/null; then
    printf 'Lua rock Makefile dependency tests must not run for non-host libc targets\n' >&2
    exit 1
  fi
}

bundle_root=$("$repo_root/scripts/detect_cpkt_bundle.sh")
configure_target "$tmp_dir/gnu" gnu "$bundle_root"
gnu_tests="$(list_tests "$tmp_dir/gnu")"
assert_has_native_lua_tests "$gnu_tests"

configure_target "$tmp_dir/core" gnu
core_tests="$(list_tests "$tmp_dir/core")"
assert_lacks_native_lua_tests "$core_tests"

configure_target "$tmp_dir/musl" musl
musl_tests="$(list_tests "$tmp_dir/musl")"
assert_lacks_native_lua_tests "$musl_tests"
