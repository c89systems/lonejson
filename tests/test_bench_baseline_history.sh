#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Rationale: benchmark gates are release blockers only if the host identity,
# baseline format, and comparison semantics are stable across runs.

repo_root="$(CDPATH= cd -- "$1" && pwd)"
lua_exec=${2:-lua}
luarocks_exec=${3:-luarocks}
runner_build=${4:-}
tmp_dir=$(mktemp -d "$workspace_root/build/test_bench_baseline_history.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

if [[ "$(uname -s)" == Linux &&
      ! -x "$runner_build/lonejson_lua_target_runner" ]]; then
  printf 'missing required Bootlin target runner for Lua benchmark fixture\n'
  exit 1
fi

run_lua() {
  if [[ -x "$runner_build/lonejson_lua_target_runner" ]]; then
    "$repo_root/scripts/run_lua_benchmark.sh" "$repo_root" "$runner_build" "$@"
  else
    "$lua_exec" "$@"
  fi
}

if [ "$("$repo_root/scripts/bench_host_id.sh" wopr)" != \
    "f259bcc951a8f53802cc755f08e5e218" ]; then
  printf 'bench host id helper changed the frozen wopr host hash\n' >&2
  exit 1
fi

grep -F 'getenv("LONEJSON_BENCH_HOST_ID")' \
  "$repo_root/bench/lonejson_bench.c" >/dev/null
grep -F 'snprintf(run->host, sizeof(run->host), "%s", host_id);' \
  "$repo_root/bench/lonejson_bench.c" >/dev/null

mkdir -p "$tmp_dir/bin"
cat >"$tmp_dir/bin/md5sum" <<'EOF'
#!/usr/bin/env bash
exit 1
EOF
cat >"$tmp_dir/bin/md5" <<'EOF'
#!/usr/bin/env bash
if [ "${1:-}" != "-q" ]; then
  exit 1
fi
cat >/dev/null
printf '%s\n' F259BCC951A8F53802CC755F08E5E218
EOF
chmod +x "$tmp_dir/bin/md5sum" "$tmp_dir/bin/md5"
if [ "$(PATH="$tmp_dir/bin:/usr/bin:/bin" "$repo_root/scripts/bench_host_id.sh" wopr)" != \
    "f259bcc951a8f53802cc755f08e5e218" ]; then
  printf 'bench host id helper did not normalize md5 -q fallback output\n' >&2
  exit 1
fi

git_top="$(git -C "$repo_root" rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -n "$git_top" ]]; then
  git_top="$(CDPATH= cd -- "$git_top" && pwd)"
fi
if [[ "$git_top" == "$repo_root" ]]; then
  for path in \
    perflogs/hosts/testhost/history.jsonl \
    perflogs/hosts/testhost/latest.json \
    perflogs/hosts/testhost/runs/1.json \
    perflogs/hosts/testhost/lua/history.jsonl \
    perflogs/hosts/testhost/lua/latest.json \
    perflogs/hosts/testhost/lua/runs/1.json; do
    if git -C "$repo_root" ls-files --error-unmatch "$path" >/dev/null 2>&1; then
      printf '%s must be ignored local benchmark output, not tracked source\n' "$path" >&2
      exit 1
    fi
    if ! git -C "$repo_root" check-ignore -q "$path"; then
      printf '%s must be listed in .gitignore\n' "$path" >&2
      exit 1
    fi
  done

  for path in \
    perflogs/hosts/f259bcc951a8f53802cc755f08e5e218/baseline.json \
    perflogs/hosts/f259bcc951a8f53802cc755f08e5e218/lua/baseline.json; do
    if ! git -C "$repo_root" ls-files --error-unmatch "$path" >/dev/null 2>&1; then
      printf '%s must remain tracked as the frozen benchmark contract\n' "$path" >&2
      exit 1
    fi
  done
elif [[ -f "$repo_root/RELEASE_MANIFEST" ]]; then
  for path in \
    perflogs/hosts/f259bcc951a8f53802cc755f08e5e218/baseline.json \
    perflogs/hosts/f259bcc951a8f53802cc755f08e5e218/lua/baseline.json; do
    if [[ ! -f "$repo_root/$path" ]] || ! grep -Fxq "$path" "$repo_root/RELEASE_MANIFEST"; then
      printf '%s must be shipped in the source manifest as the frozen benchmark contract\n' "$path" >&2
      exit 1
    fi
  done
fi

missing_host_output=$(
  make --no-print-directory -C "$repo_root" bench-check \
    PERF_HOST_ID=missing-test-host \
    LUA="$lua_exec" \
    LUAROCKS="$luarocks_exec" 2>&1
)
printf '%s\n' "$missing_host_output" | \
  grep -F 'bench-check skipped: missing frozen benchmark baseline for host missing-test-host' >/dev/null
printf '%s\n' "$missing_host_output" | \
  grep -F 'perflogs/hosts/missing-test-host/baseline.json' >/dev/null
printf '%s\n' "$missing_host_output" | \
  grep -F 'perflogs/hosts/missing-test-host/lua/baseline.json' >/dev/null

if [[ ! -x "$runner_build/lonejson_lua_target_runner" ]]; then
  resolved_environment=$("$luarocks_exec" path --tree "$repo_root/build/luarocks") || exit $?
  eval "$resolved_environment"
  export DYLD_LIBRARY_PATH="${LONEJSON_LIBDIR:-$repo_root/build/lua-sdk/lib}:${DYLD_LIBRARY_PATH:-}"
fi

git -C "$tmp_dir" init -q
git -C "$tmp_dir" config user.email test@example.invalid
git -C "$tmp_dir" config user.name 'lonejson test'
mkdir -p "$tmp_dir/perflogs/hosts/testhost"

write_baseline() {
  local alpha=$1
  local beta=$2
  local gamma=$3
  {
    printf '{"schema_version":1,'
    printf '"timestamp_utc":"2026-01-01T00:00:00Z",'
    printf '"host":"test",'
    printf '"results":['
    printf '{"name":"parse/alpha/lonejson","mib_per_sec":%s}' "$alpha"
    if [ "$beta" != "-" ]; then
      printf ',{"name":"parse/beta/lonejson","mib_per_sec":%s}' "$beta"
    fi
    if [ "$gamma" != "-" ]; then
      printf ',{"name":"stream/gamma/lonejson","mib_per_sec":%s}' "$gamma"
    fi
    printf ']}'
  } >"$tmp_dir/perflogs/hosts/testhost/baseline.json"
}

write_baseline 100 50 -
git -C "$tmp_dir" add perflogs/hosts/testhost/baseline.json
git -C "$tmp_dir" commit -q -m 'test(bench): add first baseline'

write_baseline 120 - 5
git -C "$tmp_dir" add perflogs/hosts/testhost/baseline.json
git -C "$tmp_dir" commit -q -m 'test(bench): add second baseline'

write_baseline 90 55 6
git -C "$tmp_dir" add perflogs/hosts/testhost/baseline.json
git -C "$tmp_dir" commit -q -m 'test(bench): add third baseline'

output=$(run_lua "$repo_root/scripts/bench_baseline_history.lua" --repo "$tmp_dir" --kind c --host-id testhost)

printf '%s\n' "$output" | grep -q 'REGRESSION: 1 metric'
printf '%s\n' "$output" | grep -q 'NOTICE: 1 material regression'
printf '%s\n' "$output" | grep -q 'parse/alpha/lonejson'
printf '%s\n' "$output" | grep -q 'parse/beta/lonejson'
printf '%s\n' "$output" | grep -q 'stream/gamma/lonejson'
printf '%s\n' "$output" | grep -Eq 'parse/beta/lonejson.*50\.00[[:space:]]+\.[[:space:]]+55\.00'

if printf '%s\n' "$output" | awk 'length($0) > 80 { exit 1 }'; then
  exit 0
fi

printf '%s\n' "$output" >&2
printf 'bench baseline history report exceeded 80 columns\n' >&2
exit 1
