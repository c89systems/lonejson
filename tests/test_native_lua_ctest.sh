#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/native-lua-ctest.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
fixture="$work/repo"
mkdir -p "$fixture/scripts" "$fixture/tests" "$fixture/bench" "$work/bin"
cp "$repo_root/scripts/"{test_lua_native,require_build_workspace,run_installed_lua}.sh "$fixture/scripts/"
cp "$repo_root/scripts/bench_host_id.sh" "$fixture/scripts/"
cp "$repo_root/tests/"test_lua_{schema_cache,encode_stats}.sh "$fixture/tests/"
cp "$repo_root/tests/"test_bench_{baseline_history,retry_confirm}.sh "$fixture/tests/"
cp "$repo_root/bench/lonejson_bench.c" "$fixture/bench/"
cat >"$work/bin/make" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
if [[ "$*" == *' bench-check '* ]]; then
  printf '%s\n' 'bench-check skipped: missing frozen benchmark baseline for host missing-test-host' \
    'perflogs/hosts/missing-test-host/baseline.json' \
    'perflogs/hosts/missing-test-host/lua/baseline.json'
  exit 0
fi
[[ "$1" == -C && "$2" == "$NATIVE_TEST_ROOT" && "$3" == lua-rock ]]
[[ "${@: -1}" == "LONEJSON_LUA_BUILD_DIR=$NATIVE_TEST_BUILD" ]]
printf 'prepare\n' >>"$NATIVE_TEST_EVENTS"
[[ ${FAIL_NATIVE_PREPARE:-0} == 0 ]]
mkdir -p "$NATIVE_TEST_ROOT/build/luarocks" "$NATIVE_TEST_ROOT/build/lua-sdk/lib"
touch "$NATIVE_TEST_ROOT/build/luarocks/module"
printf '%s\n' "$NATIVE_TEST_BUILD" >"$NATIVE_TEST_ROOT/build/lua-sdk/lib/selected-build"
SH
cat >"$work/bin/lua" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
[[ -f "$TEST_ACTIVE_ROCK/module" ]] || { printf "module 'lonejson' not found\n" >&2; exit 1; }
case "${1##*/}" in
  bench_baseline_history.lua|lonejson_lua_bench.lua)
    [[ $(cat "$NATIVE_TEST_ROOT/build/lua-sdk/lib/selected-build") == "$NATIVE_TEST_BUILD" ]]
    [[ "$DYLD_LIBRARY_PATH" == "$NATIVE_TEST_ROOT/build/lua-sdk/lib:"* ]]
    printf 'lua %s\n' "${1##*/}" >>"$NATIVE_TEST_EVENTS"
    # Stop at the interpreter boundary: this fixture tests SDK ownership, while
    # the ordinary benchmark tests exercise the actual Lua behavior.
    exit 79 ;;
esac
printf 'lua %s\n' "${1##*/}" >>"$NATIVE_TEST_EVENTS"
SH
cat >"$work/bin/luarocks" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
mode=$1; shift
tree=
while [[ $# -gt 0 ]]; do
  if [[ $1 == --tree ]]; then tree=$2; shift; fi
  shift
done
case $mode in
  config) printf 'so\n' ;;
  path)
    [[ ${FAIL_LUA_PATH:-0} == 0 ]] || exit 39
    printf 'export TEST_ACTIVE_ROCK=%q\n' "$tree" ;;
  make)
    [[ -d "$LONEJSON_LIBDIR" ]]
    [[ "$LONEJSON_LIBDIR" == "$EXPECTED_LUA_LIBDIR" ]]
    mkdir -p "$tree"; touch "$tree/module"
    printf 'instrument %s\n' "$CFLAGS" >>"$NATIVE_TEST_EVENTS" ;;
  *) exit 1 ;;
esac
SH
cat >"$work/bin/uname" <<'SH'
#!/usr/bin/env bash
case $1 in -s) printf 'Darwin\n' ;; -m) printf 'arm64\n' ;; *) exit 1 ;; esac
SH
printf '#!/usr/bin/env bash\ntouch "$2"\n' >"$fixture/scripts/render_release_rockspec.sh"
chmod +x "$work/bin/"* "$fixture/scripts/"*.sh
cat >"$fixture/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.21)
project(native_lua_fixture NONE)
enable_testing()
set(CMAKE_HOST_SYSTEM_NAME "${TEST_NATIVE_HOST}")
set(LONEJSON_CAN_RUN_LUA_NATIVE_TESTS ON)
add_test(NAME core COMMAND "${CMAKE_COMMAND}" -E true)
foreach(case baseline_history retry_confirm)
  add_test(NAME lonejson_bench_${case}_tests COMMAND bash
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bench_${case}.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${LONEJSON_LUA_EXECUTABLE}"
    "${LONEJSON_LUAROCKS_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(lonejson_bench_${case}_tests PROPERTIES LABELS "local;host-policy")
endforeach()
include("${LONEJSON_MODULE}")
CMAKE
export NATIVE_TEST_ROOT="$fixture" NATIVE_TEST_EVENTS="$work/events"
export NATIVE_TEST_BUILD="$fixture/build/native"
unset LONEJSON_LIBDIR
export EXPECTED_LUA_LIBDIR="$fixture/build/lua-sdk/lib"
cmake -S "$fixture" -B "$fixture/build/native" -G Ninja \
  -D TEST_NATIVE_HOST=Darwin -D "LONEJSON_MODULE=$repo_root/cmake/LonejsonNativeLuaTests.cmake" \
  -D "LONEJSON_MAKE_EXECUTABLE=$work/bin/make" \
  -D "LONEJSON_LUA_EXECUTABLE=$work/bin/lua" \
  -D "LONEJSON_LUAROCKS_EXECUTABLE=$work/bin/luarocks" >"$work/configure.log" 2>&1
run_tests() {
  PATH="$work/bin:$PATH" ctest --test-dir "$fixture/build/native" --no-tests=error --output-on-failure -j 1 "$@"
}
run_tests -R '^lonejson_lua_(native|schema_cache|encode_stats)_tests$' >"$work/main.log" 2>&1 || {
  cat "$work/main.log" >&2; exit 1;
}
[[ $(head -1 "$NATIVE_TEST_EVENTS") == prepare ]]
for script in test_lua.lua test_lua_fuzz.lua test_lua_schema_cache.lua test_lua_encode_stats.lua; do
  grep -Fx "lua $script" "$NATIVE_TEST_EVENTS" >/dev/null
done
grep -F -- '-DLONEJSON_TEST_MAP_ANALYSIS_COUNTER=1' "$NATIVE_TEST_EVENTS" >/dev/null
grep -F -- '-DLONEJSON_TEST_LUA_ENCODE_STATS=1' "$NATIVE_TEST_EVENTS" >/dev/null
[[ $(grep -c '^prepare$' "$NATIVE_TEST_EVENTS") == 1 ]]
# The Make entrypoint's installed-SDK override also reaches the instrumented
# modules when CTest invokes the scripts without an explicit fourth argument.
mkdir -p "$fixture/build/custom-sdk/lib"
: >"$NATIVE_TEST_EVENTS"
LONEJSON_LIBDIR="$fixture/build/custom-sdk/lib" \
  EXPECTED_LUA_LIBDIR="$fixture/build/custom-sdk/lib" \
  run_tests -R '^lonejson_lua_(schema_cache|encode_stats)_tests$' >"$work/custom.log" 2>&1 || {
  cat "$work/custom.log" >&2; exit 1;
}
[[ $(grep -c '^instrument ' "$NATIVE_TEST_EVENTS") == 2 ]]
: >"$NATIVE_TEST_EVENTS"
run_tests -R '^lonejson_lua_native_fuzz_tests$' >"$work/fuzz.log" 2>&1
printf 'prepare\nlua test_lua_fuzz.lua\n' >"$work/expected"
cmp "$work/expected" "$NATIVE_TEST_EVENTS"
: >"$NATIVE_TEST_EVENTS"
if run_tests -R '^lonejson_(bench_(baseline_history|retry_confirm)|lua_native)_tests$' >"$work/bench.log" 2>&1; then
  printf 'benchmark ownership probes did not stop at the Lua boundary\n' >&2; exit 1
fi
printf 'prepare\nlua bench_baseline_history.lua\nlua lonejson_lua_bench.lua\nlua test_lua.lua\nlua test_lua_fuzz.lua\n' >"$work/expected"
if ! cmp "$work/expected" "$NATIVE_TEST_EVENTS"; then
  cat "$work/bench.log" >&2; exit 1
fi
grep -E 'lonejson_lua_native_tests .*Passed' "$work/bench.log" >/dev/null
grep -F '2 tests failed out of 4' "$work/bench.log" >/dev/null
: >"$NATIVE_TEST_EVENTS"
if FAIL_NATIVE_PREPARE=1 run_tests -R '^lonejson_lua_native_tests$' >"$work/failed.log" 2>&1; then
  printf 'native tests accepted a failed Lua prerequisite\n' >&2; exit 1
fi
[[ $(cat "$NATIVE_TEST_EVENTS") == prepare ]]
grep -F 'Not Run' "$work/failed.log" >/dev/null
: >"$NATIVE_TEST_EVENTS"
# Failed path discovery cannot fall through to a previously usable module.
if FAIL_LUA_PATH=1 TEST_ACTIVE_ROCK="$fixture/build/luarocks" \
    run_tests -R '^lonejson_lua_(schema_cache|encode_stats)_tests$' >"$work/path-failed.log" 2>&1; then
  printf 'instrumented tests accepted failed Lua path discovery\n' >&2; exit 1
fi
! grep '^lua ' "$NATIVE_TEST_EVENTS"
# An ordinary Lua interpreter cannot load an instrumented Darwin library.
# Preserve core sanitizer coverage without preparing or loading that module.
for sanitizer in ASAN TSAN; do
  cmake -S "$fixture" -B "$fixture/build/native" \
    -D LONEJSON_ENABLE_ASAN=OFF -D LONEJSON_ENABLE_TSAN=OFF \
    -D "LONEJSON_ENABLE_$sanitizer=ON" >"$work/sanitizer-configure.log" 2>&1
  : >"$NATIVE_TEST_EVENTS"
  run_tests -LE host-policy >"$work/sanitizer.log" 2>&1 || {
    cat "$work/sanitizer.log" >&2; exit 1;
  }
  grep -F '100% tests passed, 0 tests failed out of 1' "$work/sanitizer.log" >/dev/null || {
    cat "$work/sanitizer.log" >&2; exit 1;
  }
  [[ ! -s "$NATIVE_TEST_EVENTS" ]]
  ctest --test-dir "$fixture/build/native" -N >"$work/sanitizer-tests"
  ! grep -F lonejson_lua_ "$work/sanitizer-tests"
done
cmake -S "$fixture" -B "$fixture/build/native" -D TEST_NATIVE_HOST=Linux >"$work/linux.log" 2>&1
ctest --test-dir "$fixture/build/native" -N >"$work/linux-tests"
! grep -F lonejson_lua_ "$work/linux-tests"
printf 'Darwin Lua preparation, instrumentation, selection and failure propagation passed\n'
