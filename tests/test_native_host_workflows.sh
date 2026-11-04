#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/native-host-workflows.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
mkdir "$work/bin"
export HOST_WORKFLOW_REAL_BASH="$(command -v bash)"
export HOST_WORKFLOW_REAL_CMAKE="$(command -v cmake)"
export HOST_WORKFLOW_EVENTS="$work/events"
cat >"$work/bin/uname" <<'SH'
#!/usr/bin/env bash
case "$1" in
  -s) printf '%s\n' "$HOST_WORKFLOW_OS" ;;
  -m) printf '%s\n' "$HOST_WORKFLOW_ARCH" ;;
  *) exit 1 ;;
esac
SH
cat >"$work/bin/make" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
printf 'make %s\n' "$*" >>"$HOST_WORKFLOW_EVENTS"
if [[ "${HOST_WORKFLOW_MODE:-}" == fuzz ]]; then
  printf '%s\n' 'cmake --build --preset fuzz --target lonejson_fuzz_targets' \
    'make lua-fuzz' 'LONEJSON_FUZZ_EXECUTIONS=1000'
  [[ ${HOST_WORKFLOW_BAD_PLAN:-0} == 1 ]] || printf '%s\n' 'ctest --preset fuzz'
  exit 0
fi
[[ "$1" == --no-print-directory && "$2" == -C && "$4" == lua-rock ]]
[[ ${HOST_WORKFLOW_FAIL_PREPARE:-0} == 0 ]] || exit 81
mkdir -p "$3/build/lua-sdk/lib" "$3/build/luarocks"
printf prepared >"$3/build/luarocks/module"
SH
cat >"$work/bin/cmake" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
[[ "${HOST_WORKFLOW_MODE:-}" != fuzz ]] || { printf 'unexpected AFL configure\n' >&2; exit 92; }
for argument in "$@"; do
  if [[ "$argument" == -P ]]; then exec "$HOST_WORKFLOW_REAL_CMAKE" "$@"; fi
done
# The archive caller's packaging and extraction are real; compilation is outside
# this dependency-order probe and is covered by the ordinary source archive test.
[[ "$1" != --build ]] || exit 0
build=
while [[ $# -gt 0 ]]; do
  if [[ "$1" == -B ]]; then build=$2; shift; fi
  shift
done
[[ -n "$build" ]]
mkdir -p "$build"
printf 'CMAKE_PROJECT_VERSION:STATIC=9.8.7\n' >"$build/CMakeCache.txt"
SH
cat >"$work/bin/bash" <<'SH'
#!/bin/bash
set -euo pipefail
case "${1:-}" in
  */tests/test_bench_baseline_history.sh)
    [[ -f "$2/build/luarocks/module" && -d "$2/build/lua-sdk/lib" ]] || exit 73
    printf 'archive benchmark prepared\n' >>"$HOST_WORKFLOW_EVENTS"
    exit 79 ;;
esac
exec "$HOST_WORKFLOW_REAL_BASH" "$@"
SH
chmod +x "$work/bin/"*

# Unsupported AFL hosts must still validate the Make surface, without looking
# for a Linux compiler or bundle. A broken portable assertion must still fail.
for host in Darwin:arm64 Linux:aarch64; do
  export HOST_WORKFLOW_OS=${host%:*} HOST_WORKFLOW_ARCH=${host#*:}
  : >"$HOST_WORKFLOW_EVENTS"
  PATH="$work/bin:$PATH" HOST_WORKFLOW_MODE=fuzz \
    bash "$repo_root/tests/test_makefile_fuzz_targets.sh" "$repo_root" >"$work/fuzz.log" 2>&1 || {
    cat "$work/fuzz.log" >&2
    printf 'portable fuzz assertions entered an unsupported AFL host path: %s\n' "$host" >&2; exit 1;
  }
  [[ $(grep -c '^make ' "$HOST_WORKFLOW_EVENTS") == 2 ]]
  if PATH="$work/bin:$PATH" HOST_WORKFLOW_MODE=fuzz HOST_WORKFLOW_BAD_PLAN=1 \
      bash "$repo_root/tests/test_makefile_fuzz_targets.sh" "$repo_root" >"$work/bad-plan.log" 2>&1; then
    printf 'unsupported AFL host bypassed portable Make assertions\n' >&2; exit 1
  fi
done

# Exercise the actual direct caller on its Darwin path. Stop at the benchmark
# boundary so this probe neither compiles another SDK nor runs benchmark logic.
export HOST_WORKFLOW_OS=Darwin HOST_WORKFLOW_ARCH=arm64
: >"$HOST_WORKFLOW_EVENTS"
status=0
PATH="$work/bin:$PATH" bash "$repo_root/tests/test_source_release_tarball.sh" \
  "$repo_root" "$work/bin/cmake" >"$work/archive.log" 2>&1 || status=$?
if [[ "$status" != 79 ]]; then
  cat "$work/archive.log" >&2
  printf 'archive benchmark did not reach its prepared runtime (status %s)\n' "$status" >&2; exit 1
fi
[[ $(grep -c '^make .* lua-rock' "$HOST_WORKFLOW_EVENTS") == 1 ]]
[[ $(tail -1 "$HOST_WORKFLOW_EVENTS") == 'archive benchmark prepared' ]]
: >"$HOST_WORKFLOW_EVENTS"
status=0
PATH="$work/bin:$PATH" HOST_WORKFLOW_FAIL_PREPARE=1 \
  bash "$repo_root/tests/test_source_release_tarball.sh" \
  "$repo_root" "$work/bin/cmake" >"$work/archive-failed.log" 2>&1 || status=$?
if [[ "$status" != 81 ]]; then
  cat "$work/archive-failed.log" >&2
  printf 'archive caller ignored failed Lua preparation (status %s)\n' "$status" >&2; exit 1
fi
! grep -F 'archive benchmark prepared' "$HOST_WORKFLOW_EVENTS"
printf 'Portable fuzz assertions and Darwin source-archive Lua preparation passed\n'
