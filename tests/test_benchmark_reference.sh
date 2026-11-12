#!/usr/bin/env bash
set -euo pipefail
repo_root=$1
work=$(mktemp -d "$repo_root/build/benchmark-reference-test.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
source_root="$work/source"
mkdir -p "$source_root"/{src,include,lua,bench,cmake,scripts,tests}
for directory in src include lua bench cmake scripts tests; do
  printf 'original %s\n' "$directory" > "$source_root/$directory/marker"
done
for file in CMakeLists.txt CMakePresets.json Makefile; do printf 'fixture\n' > "$source_root/$file"; done
git -C "$source_root" init -q -b feature
git -C "$source_root" config user.name 'Benchmark fixture'
git -C "$source_root" config user.email fixture@example.invalid
git -C "$source_root" config commit.gpgsign false
git -C "$source_root" add .
git -C "$source_root" commit -q -m 'test(bench): record implementation fixture'
src_tree=$(git -C "$source_root" rev-parse HEAD:src)
include_tree=$(git -C "$source_root" rev-parse HEAD:include)
lua_tree=$(git -C "$source_root" rev-parse HEAD:lua)
printf 'changed implementation\n' > "$source_root/src/marker"
printf 'current harness\n' > "$source_root/bench/marker"
reference=$("$repo_root/scripts/prepare_benchmark_reference.sh" "$source_root" "$src_tree" "$include_tree" "$lua_tree")
for directory in src include lua; do grep -Fx "original $directory" "$reference/$directory/marker"; done
cmp "$source_root/bench/marker" "$reference/bench/marker"
grep -Fx '0.0.0' "$reference/VERSION"
mkdir -p "$reference/build"
printf 'keep compiled artifacts\n' > "$reference/build/keep"
printf 'updated harness\n' > "$source_root/bench/marker"
again=$("$repo_root/scripts/prepare_benchmark_reference.sh" "$source_root" "$src_tree" "$include_tree" "$lua_tree")
[[ "$again" == "$reference" && -f "$reference/build/keep" ]]
cmp "$source_root/bench/marker" "$reference/bench/marker"
grep -Fx 'original src' "$reference/src/marker"
git -C "$source_root" add src bench
git -C "$source_root" commit -q -m 'test(bench): update implementation and harness fixtures'
historical=$(git -C "$source_root" rev-parse HEAD)
src_tree=$(git -C "$source_root" rev-parse HEAD:src)
git -C "$source_root" checkout -q --orphan landed
git -C "$source_root" add CMakeLists.txt CMakePresets.json Makefile src include lua bench cmake scripts tests
git -C "$source_root" commit -q -m 'test(bench): squash verified fixture'
git clone -q --no-local --single-branch --branch landed "$source_root" "$work/clone"
if git -C "$work/clone" cat-file -e "$historical^{commit}" 2>/dev/null; then
  printf 'fixture clone retained the squashed commit\n' >&2; exit 1
fi
reference=$("$repo_root/scripts/prepare_benchmark_reference.sh" "$work/clone" "$src_tree" "$include_tree" "$lua_tree")
grep -Fx 'changed implementation' "$reference/src/marker"
grep -Fx 'original include' "$reference/include/marker"
grep -Fx 'original lua' "$reference/lua/marker"
cmp "$work/clone/bench/marker" "$reference/bench/marker"
if "$repo_root/scripts/prepare_benchmark_reference.sh" "$work/clone" 0000000000000000000000000000000000000000 "$include_tree" "$lua_tree" > "$work/error" 2>&1; then
  printf 'accepted unavailable frozen source\n' >&2; exit 1
fi
grep -F 'Git tree is unavailable' "$work/error"
mkdir -p "$work/archive"
if "$repo_root/scripts/prepare_benchmark_reference.sh" "$work/archive" "$src_tree" "$include_tree" "$lua_tree" > "$work/error" 2>&1; then
  printf 'accepted a different owning checkout\n' >&2; exit 1
fi
grep -F 'owning Git checkout' "$work/error"
printf 'PASS immutable implementation, current harness, owned build reuse and reconstruction after squash\n'
