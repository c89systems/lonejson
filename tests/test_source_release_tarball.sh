#!/usr/bin/env bash
set -euo pipefail

# Rationale: source archives must build outside git from injected release
# metadata and must not depend on generated state from the live worktree.

repo_root=$1
cmake_bin=${2:-cmake}

repo_root="$(CDPATH= cd -- "$repo_root" && pwd)"
test_root="$repo_root/build/source-release-tarball-test"
extract_dir="$test_root/extract"
stage_dir="$extract_dir/lonejson-9.8.7"
build_dir="$test_root/build"
missing_version_dir="$test_root/missing-version"
missing_version_build_dir="$test_root/missing-version-build"
tarball="$repo_root/dist/lonejson-9.8.7.tar.gz"

cleanup() {
  rm -rf "$test_root"
  rm -f "$tarball"
  rm -f "$repo_root/build/source-release-ignore-sentinel"
  rm -f "$repo_root/dist/source-release-ignore-sentinel"
}

trap cleanup EXIT INT TERM

cleanup
git_top="$(git -C "$repo_root" rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -n "$git_top" ]]; then
  git_top="$(CDPATH= cd -- "$git_top" && pwd)"
fi
if [[ "$git_top" == "$repo_root" ]]; then
  if [[ -e "$repo_root/VERSION" ]]; then
    printf 'VERSION must not be present in the git checkout\n' >&2
    exit 1
  fi
  if git -C "$repo_root" ls-files --error-unmatch VERSION >/dev/null 2>&1; then
    printf 'VERSION must not be tracked in git\n' >&2
    exit 1
  fi
fi

mkdir -p "$repo_root/build" "$repo_root/dist"
printf '%s\n' ignored >"$repo_root/build/source-release-ignore-sentinel"
printf '%s\n' ignored >"$repo_root/dist/source-release-ignore-sentinel"

"$cmake_bin" -D LONEJSON_ROOT="$repo_root" -D LONEJSON_VERSION=9.8.7 \
  -P "$repo_root/cmake/package_source.cmake"

if [[ ! -f "$tarball" ]]; then
  printf 'missing source release tarball: %s\n' "$tarball" >&2
  exit 1
fi

mkdir -p "$extract_dir"
tar -xzf "$tarball" -C "$extract_dir"

require_file() {
  local path=$1
  if [[ ! -f "$stage_dir/$path" ]]; then
    printf 'missing required source release file: %s\n' "$path" >&2
    exit 1
  fi
}

reject_path() {
  local path=$1
  if [[ -e "$stage_dir/$path" ]]; then
    printf 'ignored or private path leaked into source release: %s\n' "$path" >&2
    exit 1
  fi
}

require_file VERSION
require_file RELEASE_MANIFEST
require_file CMakeLists.txt
require_file CMakePresets.json
require_file Makefile
require_file devenv.yaml.in
require_file devenv/nginx/nginx.conf
require_file scripts/devenv.sh
require_file scripts/render_devenv.cmake
require_file tests/test_devenv.sh
require_file tests/test_devenv_ownership.sh
require_file include/lonejson.h
require_file scripts/bench_host_id.sh
require_file scripts/run_lua_benchmark.sh
require_file src/lonejson.c
require_file src/lua/lonejson_lua.c
require_file lua/lonejson/init.lua
require_file tests/test_bootlin_runtime_targets.sh
require_file tests/test_bootlin_runtime_symlink_cache.sh
require_file tests/test_lua_embedded_runner_args.lua

grep -qx '9.8.7' "$stage_dir/VERSION"
grep -qx 'VERSION' "$stage_dir/RELEASE_MANIFEST"
grep -qx 'RELEASE_MANIFEST' "$stage_dir/RELEASE_MANIFEST"

reject_path .git
reject_path build/source-release-ignore-sentinel
reject_path dist/source-release-ignore-sentinel

# Non-git source trees must provide a manifest; never infer a payload from
# an unfiltered directory copy. Generated Podman state remains excluded.
fallback_root="$test_root/non-git-source"
fallback_stage="$test_root/non-git-stage"
mkdir -p "$fallback_root/build/devenv/credentials"
printf '%s\n' public >"$fallback_root/README.md"
printf '%s\n' private >"$fallback_root/build/devenv/credentials/server.key"
if GIT_DIR="$test_root/not-a-git" "$repo_root/scripts/stage_release_sources.sh" \
  "$fallback_root" "$fallback_stage" 9.8.7 >"$test_root/manifest-error" 2>&1; then
  printf 'non-git source without manifest accepted\n' >&2; exit 1
fi
grep -F 'requires a git worktree or RELEASE_MANIFEST' "$test_root/manifest-error" >/dev/null
printf '%s\n' README.md RELEASE_MANIFEST >"$fallback_root/RELEASE_MANIFEST"
GIT_DIR="$test_root/not-a-git" "$repo_root/scripts/stage_release_sources.sh" \
  "$fallback_root" "$fallback_stage" 9.8.7
[[ -f "$fallback_stage/README.md" ]]
if [[ -e "$fallback_stage/build/devenv" ]]; then
  printf 'non-git source staging leaked generated Podman state\n' >&2
  exit 1
fi

LONEJSON_VERSION_OVERRIDE=7.8.9 \
  "$stage_dir/scripts/release_version.sh" | grep -qx '9.8.7'
LONEJSON_VERSION_OVERRIDE=7.8.9 "$cmake_bin" \
  -S "$stage_dir" -B "$build_dir" -G Ninja \
  -D CMAKE_BUILD_TYPE=Release \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  -D LONEJSON_BUILD_BENCHMARKS=OFF
grep -qx 'CMAKE_PROJECT_VERSION:STATIC=9.8.7' "$build_dir/CMakeCache.txt"
"$cmake_bin" --build "$build_dir" --target lonejson_static

# These ordinary checks must also work under a parent checkout, where Git
# discovery succeeds but the archive has its own VERSION and source manifest.
# Block tag operations so a regression cannot mutate the parent repository.
mkdir -p "$test_root/bin"
export LONEJSON_TEST_REAL_GIT="$(command -v git)"
export LONEJSON_TEST_TAG_ATTEMPT="$test_root/tag-attempt"
cat >"$test_root/bin/git" <<'EOF'
#!/usr/bin/env bash
for arg in "$@"; do
  if [[ "$arg" == tag ]]; then
    printf '%s\n' 'source archive tests must not mutate Git tags' >"$LONEJSON_TEST_TAG_ATTEMPT"
    cat "$LONEJSON_TEST_TAG_ATTEMPT" >&2
    exit 1
  fi
done
exec "$LONEJSON_TEST_REAL_GIT" "$@"
EOF
chmod +x "$test_root/bin/git"
lua_exec=$("$repo_root/scripts/resolve_lua55.sh")
if [[ "$(uname -s)" == Linux ]]; then
  make --no-print-directory -C "$stage_dir" lua-target-runner >/dev/null
  PATH="$test_root/bin:$PATH" bash "$stage_dir/tests/test_bench_baseline_history.sh" \
    "$stage_dir" "$lua_exec" luarocks "$stage_dir/build/host-curl"
else
  make --no-print-directory -C "$stage_dir" lua-rock LUA="$lua_exec" >/dev/null
  PATH="$test_root/bin:$PATH" bash "$stage_dir/tests/test_bench_baseline_history.sh" "$stage_dir" "$lua_exec"
fi
PATH="$test_root/bin:$PATH" bash "$stage_dir/tests/test_release_checksum_manifest.sh" "$stage_dir"
if [[ -f "$LONEJSON_TEST_TAG_ATTEMPT" ]]; then
  cat "$LONEJSON_TEST_TAG_ATTEMPT" >&2
  exit 1
fi

cp -R "$stage_dir" "$missing_version_dir"
rm -f "$missing_version_dir/VERSION"
if LONEJSON_VERSION_OVERRIDE=7.8.9 "$cmake_bin" \
    -S "$missing_version_dir" -B "$missing_version_build_dir" \
    -G Ninja \
    -D LONEJSON_BUILD_TESTS=OFF \
    -D LONEJSON_BUILD_EXAMPLES=OFF \
    >"$test_root/missing-version.out" 2>"$test_root/missing-version.err"; then
  printf 'non-git source tree configured without VERSION\n' >&2
  exit 1
fi
grep -q 'non-git source tree is missing VERSION' "$test_root/missing-version.err"
