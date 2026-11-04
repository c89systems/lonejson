#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
makefile="$repo_root/Makefile"

require_text() {
  local text=$1
  if ! grep -F -- "$text" "$makefile" >/dev/null; then
    printf 'missing lifecycle Make surface: %s\n' "$text" >&2
    exit 1
  fi
}

require_text 'make lua-env                Print shell exports for using the repo-local Lua rock and debug C library.'
require_text 'make test-e2e               Run all deterministic local e2e gates serially; set LONEJSON_*_E2E_PORT to avoid host-port conflicts.'
require_text 'make dev-ps                 Show the checkout-specific Podman pod status.'

require_text 'lua-env: lua-rock'
require_text 'lua-target-runner: deps-host'
require_text 'lua-host-runtime: lua-rock'
require_text 'lua-bench-prerequisite: $(LUA_BENCH_PREPARE)'
require_text 'scripts/run_lua_benchmark.sh'
require_text '@$(LUAROCKS) path --tree "$(LUA_ROCK_TREE)"'
require_text 'dev-ps:'
require_text './scripts/devenv.sh ps'
require_text 'test-e2e:'
require_text './scripts/test-e2e.sh'
require_text 'fuzz: deps-host toolchains-aflpp'
require_text 'make toolchains-all         Install pinned Bootlin Linux toolchains in the shared lifecycle cache.'
require_text 'make toolchains-x86_64-linux-gnu Install the pinned x86_64 glibc Bootlin collection.'
require_text 'make release-lua-artifacts  Build the standalone Lua source package, release rockspec, and source rock in dist/.'
require_text 'make clean                  Remove build/, dist/, .cache/, examples/bin/, and generated Lua module artifacts; preserve shared caches.'
require_text 'make cross-build            Configure and build every supported Linux cross-release preset.'
require_text 'make cross-test             Standard alias for make test-cross.'
require_text 'make package-single-header  Build the version-stamped standalone-header artifact in dist/.'
require_text 'cross-build: deps-cross'
require_text 'cross-test: deps-cross'
require_text 'test-cross: cross-test'
require_text './scripts/build.sh $(DEBUG_PRESET) --stage-examples'
require_text './scripts/test.sh $(DEBUG_PRESET)'
require_text './scripts/test.sh $(HOST_PRESET)'
require_text './scripts/cross_build.sh $(CROSS_RELEASE_PRESETS)'
require_text './scripts/cross_test.sh $(CROSS_RELEASE_PRESETS)'
require_text 'target_id="$$(./scripts/detect_native_target.sh)" && ./scripts/deps.sh "$$target_id"'
require_text './scripts/detect_native_target.sh'
require_text './scripts/package.sh'
require_text './scripts/package-verify.sh'
require_text 'verify-release-privacy: package-verify'
require_text './scripts/smoke_lua_src_rock.sh'
require_text './scripts/smoke_lua_src_rock.sh "$(RELEASE_ROCK)" "$(LONEJSON_LUA_LIBDIR)" "$(LUA)" "$(LUAROCKS)"'
require_text './scripts/run_release_matrix.sh'
require_text 'ctest --preset fuzz'

bench_freeze_plan=$(make --no-print-directory -C "$repo_root" -n bench-freeze-baseline)
printf '%s\n' "$bench_freeze_plan" | grep -F 'lonejson_bench run' >/dev/null
printf '%s\n' "$bench_freeze_plan" | grep -F 'lonejson_bench freeze-baseline' >/dev/null
if [[ $(printf '%s\n' "$bench_freeze_plan" | grep -n -F 'lonejson_bench run' | head -n 1 | cut -d: -f1) -ge \
      $(printf '%s\n' "$bench_freeze_plan" | grep -n -F 'lonejson_bench freeze-baseline' | head -n 1 | cut -d: -f1) ]]; then
  printf 'C benchmark baseline freeze must record a fresh run first\n' >&2
  exit 1
fi

lua_bench_freeze_plan=$(make --no-print-directory -C "$repo_root" -n lua-bench-freeze-baseline)
printf '%s\n' "$lua_bench_freeze_plan" | grep -F 'lonejson_bench run' >/dev/null
printf '%s\n' "$lua_bench_freeze_plan" | grep -F 'lonejson_lua_bench.lua run' >/dev/null
printf '%s\n' "$lua_bench_freeze_plan" | grep -F 'lonejson_lua_bench.lua freeze-baseline' >/dev/null
if [[ $(printf '%s\n' "$lua_bench_freeze_plan" | grep -n -F 'lonejson_lua_bench.lua run' | head -n 1 | cut -d: -f1) -ge \
      $(printf '%s\n' "$lua_bench_freeze_plan" | grep -n -F 'lonejson_lua_bench.lua freeze-baseline' | head -n 1 | cut -d: -f1) ]]; then
  printf 'Lua benchmark baseline freeze must record a fresh run first\n' >&2
  exit 1
fi

if grep -F './scripts/run_afl_fuzz.sh' "$makefile" >/dev/null; then
  printf 'fuzz must route through scripts/fuzz.sh\n' >&2
  exit 1
fi

help_text=$(make -C "$repo_root" help)
while IFS= read -r target; do
  [[ "$target" == help ]] && continue
  grep -F "make $target" <<<"$help_text" >/dev/null || {
    printf 'phony lifecycle target is missing from make help: %s\n' "$target" >&2
    exit 1
  }
done < <(awk '
  /^\.PHONY:/ { collecting = 1; next }
  collecting {
    line = $0
    sub(/^[[:space:]]*/, "", line)
    sub(/[[:space:]]*\\$/, "", line)
    if (line == "") next
    count = split(line, targets, /[[:space:]]+/)
    for (target_index = 1; target_index <= count; ++target_index) if (targets[target_index] != "") print targets[target_index]
    if ($0 !~ /\\$/) exit
  }
' "$makefile")

for script in \
  deps build test cross_build cross_test fuzz package package-verify \
  time_step osxcross_available smoke_lua_src_rock run_release_matrix \
  devenv test-e2e; do
  if [[ ! -x "$repo_root/scripts/$script.sh" ]]; then
    printf 'missing executable lifecycle script: scripts/%s.sh\n' "$script" >&2
    exit 1
  fi
done

grep -F 'verify_release_artifacts.sh' "$repo_root/scripts/package-verify.sh" >/dev/null
grep -F 'verify_release_archives.sh' "$repo_root/scripts/package-verify.sh" >/dev/null
grep -F 'cpkt-toolchains.sh" discover arm64-apple-darwin' "$repo_root/scripts/osxcross_available.sh" >/dev/null
grep -Fx 'set -euo pipefail' "$repo_root/scripts/run_release_matrix.sh" >/dev/null
grep -Fx 'set -euo pipefail' "$repo_root/scripts/smoke_darwin_release.sh" >/dev/null

[[ -f "$repo_root/devenv.yaml.in" ]]
[[ ! -e "$repo_root/docker-compose.yaml" ]]
[[ ! -e "$repo_root/scripts/compose.sh" ]]

grep -F 'NAMES lua lua5.5 lua5.4 lua5.3 luajit' "$repo_root/CMakeLists.txt" >/dev/null && {
  printf 'obsolete Lua runtime discovery remains in CMakeLists.txt\n' >&2
  exit 1
}
grep -F 'scripts/resolve_lua55.sh' "$repo_root/CMakeLists.txt" >/dev/null
grep -F 'LUA ?= $(shell ./scripts/resolve_lua55.sh 2>/dev/null)' "$repo_root/Makefile" >/dev/null
grep -F 'LUA="$(LUA)" ./scripts/devenv.sh up' "$repo_root/Makefile" >/dev/null
grep -F 'Lua 5.5 executable' "$repo_root/scripts/resolve_lua55.sh" >/dev/null

darwin_lua_test=$(make --no-print-directory -C "$repo_root" -n \
  LONEJSON_HOST_OS=Darwin lua-test)
if grep -F 'lonejson_lua_target_runner' <<<"$darwin_lua_test" >/dev/null; then
  printf 'Darwin Lua tests must not require the Linux target runner\n' >&2
  exit 1
fi
printf '%s\n' "$darwin_lua_test" | grep -F 'luarocks' >/dev/null
if grep -F 'cmake --install' <<<"$darwin_lua_test"; then
  printf 'Darwin Lua preparation must be owned by the CTest fixture\n' >&2; exit 1
fi
darwin_debug_test=$(make --no-print-directory -C "$repo_root" -n LONEJSON_HOST_OS=Darwin test)
if grep -F 'make lua-test' <<<"$darwin_debug_test"; then
  printf 'Darwin debug tests repeat the Lua CTest selection\n' >&2; exit 1
fi

darwin_lua_bench=$(make --no-print-directory -C "$repo_root" -n \
  LONEJSON_HOST_OS=Darwin lua-bench)
printf '%s\n' "$darwin_lua_bench" |
  grep -F 'cmake --build --preset host --target lonejson_shared' >/dev/null

grep -F 'keep-id:uid=0,gid=0' "$repo_root/devenv.yaml.in" >/dev/null
grep -F 'hostIP: 127.0.0.1' "$repo_root/devenv.yaml.in" >/dev/null
grep -F 'service-readiness' "$repo_root/scripts/devenv.sh" >/dev/null
grep -F 'Podman state and recent logs follow' "$repo_root/scripts/test-e2e.sh" >/dev/null

# Exercise native Lua argument boundaries and error propagation with recording tools.
mkdir -p "$repo_root/build"
lua_recipe_tmp=$(mktemp -d "$repo_root/build/lua-recipe.XXXXXX")
trap 'rm -rf "$lua_recipe_tmp"' EXIT
cat >"$lua_recipe_tmp/lua" <<'LUA'
#!/bin/bash
[[ $# == 1 ]] || exit 90
printf '%s\n' "$1" >>"$LUA_RECIPE_LOG"
[[ "$1" != "${LUA_RECIPE_FAIL:-}" ]] || exit 91
LUA
cat >"$lua_recipe_tmp/luarocks" <<'ROCKS'
#!/bin/bash
[[ "$1" == path ]]
ROCKS
chmod +x "$lua_recipe_tmp/lua" "$lua_recipe_tmp/luarocks"
export LUA_RECIPE_LOG="$lua_recipe_tmp/log"
"$repo_root/scripts/test_lua_native.sh" "$lua_recipe_tmp/lua" "$lua_recipe_tmp/luarocks" tests/test_lua.lua tests/test_lua_fuzz.lua
printf '%s\n' tests/test_lua.lua tests/test_lua_fuzz.lua >"$lua_recipe_tmp/expected"
diff -u "$lua_recipe_tmp/expected" "$LUA_RECIPE_LOG"
cat >"$lua_recipe_tmp/luarocks-failed" <<'ROCKS'
#!/bin/bash
exit 92
ROCKS
chmod +x "$lua_recipe_tmp/luarocks-failed"
: >"$LUA_RECIPE_LOG"
if env -u LUA -u LUAROCKS "$repo_root/scripts/test_lua_native.sh" \
    "$lua_recipe_tmp/lua" "$lua_recipe_tmp/luarocks-failed" tests/not-reached.lua; then
  printf 'native Lua driver swallowed a failed prerequisite\n' >&2; exit 1
fi
[[ ! -s "$LUA_RECIPE_LOG" ]]
if LONEJSON_ENABLE_LIVE_TESTS=1 make -s -C "$repo_root" prerelease-live \
    >"$lua_recipe_tmp/live" 2>&1; then
  printf 'undefined live checks reported success\n' >&2; exit 1
fi
grep -F 'No live prerelease checks are defined' "$lua_recipe_tmp/live" >/dev/null
: >"$LUA_RECIPE_LOG"
if LUA_RECIPE_FAIL=tests/test_lua_fuzz.lua "$repo_root/scripts/test_lua_native.sh" "$lua_recipe_tmp/lua" "$lua_recipe_tmp/luarocks" tests/test_lua.lua tests/test_lua_fuzz.lua tests/not-reached.lua; then
  printf 'native Lua driver swallowed a failing test\n' >&2; exit 1
fi
diff -u "$lua_recipe_tmp/expected" "$LUA_RECIPE_LOG"
