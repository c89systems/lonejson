#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd "$1" && pwd -P)
native_os=$(uname -s)
native_taskset=
native_cpu=
if [[ "$native_os" == Linux ]]; then
  native_taskset=$(command -v taskset)
  native_cpu=$(awk '/^Cpus_allowed_list:/ { split($2, range, /[-,]/); print range[1] }' /proc/self/status)
  native_last_cpu=$(awk '/^Cpus_allowed_list:/ { count=split($2, range, /[-,]/); print range[count] }' /proc/self/status)
fi
work=$(mktemp -d "$repo_root/build/benchmark-runner-test.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/bin"
cat >"$work/bin/uname" <<'SH'
#!/usr/bin/env bash
printf '%s\n' "$TEST_BENCH_OS"
SH
cat >"$work/bin/taskset" <<'SH'
#!/usr/bin/env bash
printf '%s\n' "$1" "$2" >>"$TEST_BENCH_LOG"
shift 2
exec "$@"
SH
cat >"$work/bin/probe" <<'SH'
#!/usr/bin/env bash
printf '%s\n' "$LONEJSON_BENCH_AFFINITY" "$LONEJSON_BENCH_SOURCE_REVISION" "$@" >>"$TEST_BENCH_LOG"
SH
chmod +x "$work/bin/"*
export PATH="$work/bin:$PATH" TEST_BENCH_LOG="$work/log" LONEJSON_BENCH_SOURCE_REVISION=fixture
TEST_BENCH_OS=Linux LONEJSON_BENCH_CPU=3 "$repo_root/scripts/run_benchmark.sh" probe 'a b' 'c;d'
printf '%s\n' -c 3 cpu:3 fixture 'a b' 'c;d' >"$work/expected"
cmp "$work/expected" "$work/log"
: >"$work/log"
TEST_BENCH_OS=Darwin "$repo_root/scripts/run_benchmark.sh" probe 'a b'
printf '%s\n' unbound fixture 'a b' >"$work/expected"
cmp "$work/expected" "$work/log"
if TEST_BENCH_OS=Linux LONEJSON_BENCH_CPU='0,1' "$repo_root/scripts/run_benchmark.sh" probe >"$work/error" 2>&1; then
  printf 'accepted invalid CPU selection\n' >&2; exit 1
fi
grep -F 'must be one allowed CPU number' "$work/error" >/dev/null
"$repo_root/scripts/run_benchmark.sh" --identity > "$work/identity"
printf '%s\n' fixture > "$work/expected-identity"
if [[ $(git -C "$repo_root" rev-parse --show-toplevel 2>/dev/null || true) == "$repo_root" ]]; then
  git -C "$repo_root" rev-parse HEAD:src HEAD:include HEAD:lua >> "$work/expected-identity"
else
  printf '%s\n' unversioned unversioned unversioned >> "$work/expected-identity"
fi
cmp "$work/expected-identity" "$work/identity"
: > "$work/log"
TEST_BENCH_OS=Linux LONEJSON_BENCH_CPU=3 "$repo_root/scripts/run_benchmark.sh" --affinity > "$work/affinity"
printf 'cpu:3\n' > "$work/expected-affinity"
cmp "$work/expected-affinity" "$work/affinity"
[[ ! -s "$work/log" ]]
if [[ -n "$native_taskset" ]]; then
  TEST_BENCH_OS=Linux "$repo_root/scripts/run_benchmark.sh" --affinity > "$work/default-affinity"
  printf 'cpu:%s\n' "$native_last_cpu" > "$work/expected-affinity"
  cmp "$work/expected-affinity" "$work/default-affinity"
  TEST_BENCH_OS=Linux LONEJSON_BENCH_CPU="$native_cpu" "$native_taskset" -c "$native_cpu" \
    "$repo_root/scripts/run_benchmark.sh" probe inherited
  printf '%s\n' "cpu:$native_cpu" fixture inherited > "$work/expected"
  cmp "$work/expected" "$work/log"
fi
mkdir -p "$work/archive/scripts"
cp "$repo_root/scripts/run_benchmark.sh" "$work/archive/scripts/"
printf '0.0.0\n' > "$work/archive/VERSION"
(unset LONEJSON_BENCH_SOURCE_REVISION LONEJSON_BENCH_SRC_TREE LONEJSON_BENCH_INCLUDE_TREE LONEJSON_BENCH_LUA_TREE
 "$work/archive/scripts/run_benchmark.sh" --identity) > "$work/archive-identity"
printf '%s\n' release:0.0.0 unversioned unversioned unversioned > "$work/expected-identity"
cmp "$work/expected-identity" "$work/archive-identity"
printf 'PASS benchmark affinity, provenance, argv and invalid CPU selection\n'
