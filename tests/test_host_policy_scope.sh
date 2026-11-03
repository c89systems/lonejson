#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/host-policy-scope.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cat >"$work/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.21)
project(host_policy_scope NONE)
enable_testing()
set(CMAKE_HOST_SYSTEM_NAME "${TEST_HOST_OS}")
set(CMAKE_HOST_SYSTEM_PROCESSOR "${TEST_HOST_ARCH}")
find_program(LONEJSON_BASH_EXECUTABLE bash REQUIRED)
add_test(NAME portable COMMAND "${CMAKE_COMMAND}" -E true)
include("${TEST_POLICY_MODULE}")
CMAKE
for host in Linux:x86_64 Darwin:arm64 Linux:aarch64; do
  cmake -S "$work" -B "$work/build" \
    -D "TEST_HOST_OS=${host%:*}" -D "TEST_HOST_ARCH=${host#*:}" \
    -D "TEST_POLICY_MODULE=$repo_root/cmake/LonejsonLinuxPolicyTests.cmake" >"$work/configure" 2>&1
  ctest --test-dir "$work/build" -N -L host-policy >"$work/inventory"
  expected=0
  [[ $host != Linux:x86_64 ]] || expected=14
  grep -Fx "Total Tests: $expected" "$work/inventory" >/dev/null || {
    cat "$work/inventory" >&2
    printf 'incorrect Linux policy selection for %s\n' "$host" >&2; exit 1;
  }
  if [[ $expected == 14 ]]; then
    for name in compiler_selection release_archive_verify native_bootlin_target \
        cpkt_aflpp_resolver lua_native_test_target_filter; do
      grep -F "lonejson_${name}_tests" "$work/inventory" >/dev/null
    done
  fi
  ctest --test-dir "$work/build" --no-tests=error --output-on-failure -R '^portable$' >"$work/portable" 2>&1
done
printf 'Linux-only policy inventory and portable test coverage passed\n'
