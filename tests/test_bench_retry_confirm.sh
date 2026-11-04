#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Rationale: benchmark retries should confirm material regressions without
# hiding real slowdowns behind a single noisy measurement.

repo_root=$1
lua_exec=${2:-lua}
luarocks_exec=${3:-luarocks}
runner_build=${4:-}
tmp_dir=$(mktemp -d "$workspace_root/build/test_bench_retry_confirm.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

if [[ "$(uname -s)" == Linux &&
      ! -x "$runner_build/lonejson_lua_target_runner" ]]; then
  printf 'missing required Bootlin target runner for Lua benchmark fixture\n'
  exit 1
fi

run_lua_bench() {
  if [[ -x "$runner_build/lonejson_lua_target_runner" ]]; then
    "$repo_root/scripts/run_lua_benchmark.sh" "$repo_root" "$runner_build" \
      "$repo_root/bench/lonejson_lua_bench.lua" "$@"
  else
    (cd "$repo_root" && "$lua_exec" bench/lonejson_lua_bench.lua "$@")
  fi
}

if [[ ! -x "$runner_build/lonejson_lua_target_runner" ]]; then
  resolved_environment=$("$luarocks_exec" path --tree "$repo_root/build/luarocks") || exit $?
  eval "$resolved_environment"
  export DYLD_LIBRARY_PATH="${LONEJSON_LIBDIR:-$repo_root/build/lua-sdk/lib}:${DYLD_LIBRARY_PATH:-}"
fi

write_run() {
  local path=$1
  shift
  printf '{"schema_version":1,"host":"test","compiler":"test","toolchain":"test","iterations":1,"parser_buffer_size":4096,"push_parser_buffer_size":4096,"reader_buffer_size":4096,"stream_buffer_size":4096,"results":[' >"$path"
  local first=1
  while [ "$#" -gt 0 ]; do
    local name=$1
    local mib=$2
    local mismatch=$3
    shift 3
    if [ "$first" -eq 0 ]; then
      printf ',' >>"$path"
    fi
    first=0
    printf '{"name":"%s","mib_per_sec":%s,"mismatch_count":%s}' \
      "$name" "$mib" "$mismatch" >>"$path"
  done
  printf ']}\n' >>"$path"
}

write_lua_run() {
  local path=$1
  local name=$2
  local mib=$3
  local mismatch=$4
  printf '{"schema_version":30,"timestamp_epoch_ns":1,"timestamp_utc":"1970-01-01T00:00:00Z","host":"test","lua_version":"Lua 5.5","c_latest_path":"","iterations":1,"results":[' >"$path"
  printf '{"name":"%s","group":"decode","elapsed_ns":1,"total_bytes":1,"total_documents":1,"mismatch_count":%s,"mib_per_sec":%s,"docs_per_sec":1,"ns_per_byte":1}' \
    "$name" "$mismatch" "$mib" >>"$path"
  printf ']}\n' >>"$path"
}

fake_bench="$tmp_dir/fake_bench.sh"
fake_log="$tmp_dir/fake_bench.log"
cat >"$fake_bench" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
log_path=${FAKE_BENCH_LOG:?}
mode=${FAKE_BENCH_MODE:?}
printf '%s\n' "$*" >>"$log_path"
if [ "$1" != "case" ]; then
  exit 2
fi
case_name=$2
if [ "$mode" = "pass-one" ]; then
  printf '%s stream 101.000 MiB/s 1.0 docs/s mismatch=0 c_sib=- ratio=-\n' "$case_name"
  exit 0
fi
if [ "$mode" = "fail-first" ]; then
  if [ "$case_name" = "case/a" ]; then
    printf '%s stream 90.000 MiB/s 1.0 docs/s mismatch=0 c_sib=- ratio=-\n' "$case_name"
  else
    printf '%s stream 150.000 MiB/s 1.0 docs/s mismatch=0 c_sib=- ratio=-\n' "$case_name"
  fi
  exit 0
fi
exit 3
EOF
chmod +x "$fake_bench"

baseline="$tmp_dir/baseline.json"
latest="$tmp_dir/latest.json"

write_run "$baseline" "case/a" 100 0 "case/b" 100 0
write_run "$latest" "case/a" 90 0 "case/b" 100 0
rm -f "$fake_log"
FAKE_BENCH_LOG="$fake_log" FAKE_BENCH_MODE="pass-one" \
  LONEJSON_BENCH_CONFIRM_COOLDOWN_SECONDS=0 \
  run_lua_bench confirm-c "$fake_bench" "$baseline" "$latest" 1 >/dev/null
grep -qx 'case case/a 1' "$fake_log"
if grep -q 'case case/b 1' "$fake_log"; then
  printf 'confirm-c reran a non-failing case\n' >&2
  exit 1
fi

write_run "$baseline" "case/a" 100 0
write_run "$latest" "case/a" 97 0
rm -f "$fake_log"
FAKE_BENCH_LOG="$fake_log" FAKE_BENCH_MODE="pass-one" \
  LONEJSON_BENCH_CONFIRM_COOLDOWN_SECONDS=0 \
  run_lua_bench confirm-c "$fake_bench" "$baseline" "$latest" 1 >/dev/null
if [ -f "$fake_log" ] && [ -s "$fake_log" ]; then
  printf 'confirm-c retried a small-only regression\n' >&2
  exit 1
fi

write_run "$baseline" "case/a" 100 0 "case/b" 100 0 "case/c" 100 0
write_run "$latest" "case/a" 90 0 "case/b" 80 0 "case/c" 100 0
rm -f "$fake_log"
if FAKE_BENCH_LOG="$fake_log" FAKE_BENCH_MODE="fail-first" \
  LONEJSON_BENCH_CONFIRM_COOLDOWN_SECONDS=0 \
  run_lua_bench confirm-c "$fake_bench" "$baseline" "$latest" 1 >/dev/null 2>&1; then
  printf 'confirm-c succeeded despite a confirmed failure\n' >&2
  exit 1
fi
grep -qx 'case case/a 1' "$fake_log"
if grep -q 'case case/b 1' "$fake_log"; then
  printf 'confirm-c retried a later case after a stable confirmed failure\n' >&2
  exit 1
fi
if [ "$(grep -cx 'case case/a 1' "$fake_log")" -ne 3 ]; then
  printf 'confirm-c did not exhaust confirmation attempts for a stable failure\n' >&2
  exit 1
fi

write_run "$baseline" "case/a" 100 0
printf '{"schema_version":2,"host":"test","compiler":"test","toolchain":"test","iterations":1,"parser_buffer_size":4096,"push_parser_buffer_size":4096,"reader_buffer_size":4096,"stream_buffer_size":4096,"results":[{"name":"case/a","mib_per_sec":100,"mismatch_count":0}]}\n' >"$latest"
rm -f "$fake_log"
if FAKE_BENCH_LOG="$fake_log" FAKE_BENCH_MODE="pass-one" \
  LONEJSON_BENCH_CONFIRM_COOLDOWN_SECONDS=0 \
  run_lua_bench confirm-c "$fake_bench" "$baseline" "$latest" 1 >/dev/null 2>&1; then
  printf 'confirm-c succeeded despite a schema mismatch\n' >&2
  exit 1
fi
if [ -f "$fake_log" ] && [ -s "$fake_log" ]; then
  printf 'confirm-c retried cases despite a schema mismatch\n' >&2
  exit 1
fi

for field in host compiler toolchain; do
  write_run "$baseline" "case/a" 100 0
  sed "s/\"$field\":\"test\"/\"$field\":\"different\"/" "$baseline" >"$latest"
  rm -f "$fake_log"
  if FAKE_BENCH_LOG="$fake_log" FAKE_BENCH_MODE="pass-one" \
    run_lua_bench confirm-c "$fake_bench" "$baseline" "$latest" 1 >"$tmp_dir/environment.out" 2>&1; then
    printf 'confirm-c accepted a %s mismatch\n' "$field" >&2
    exit 1
  fi
  grep -q 'host/compiler/toolchain mismatch' "$tmp_dir/environment.out"
  test ! -s "$fake_log"
done

c_latest="$tmp_dir/c-latest.json"
write_run "$c_latest" "parse/buffer_fixed/lonejson" 100 0
write_lua_run "$baseline" "decode/record_fixed/lua" 100 0
write_lua_run "$latest" "decode/record_fixed/lua" 97 0
if ! LONEJSON_BENCH_CONFIRM_COOLDOWN_SECONDS=0 \
  run_lua_bench confirm-lua "$c_latest" "$baseline" "$latest" 1 >"$tmp_dir/confirm-lua-small.out" 2>"$tmp_dir/confirm-lua-small.err"; then
  printf 'confirm-lua failed a small-only regression\n' >&2
  cat "$tmp_dir/confirm-lua-small.err" >&2
  exit 1
fi
if grep -q 'retrying lua benchmark case' "$tmp_dir/confirm-lua-small.err"; then
  printf 'confirm-lua retried a small-only regression\n' >&2
  exit 1
fi

# Force the retry branch, then execute a real case in the child interpreter.
# A tiny baseline avoids making this routing check a performance gate.
write_lua_run "$baseline" "decode/record_fixed/lua" 0.000000000001 0
write_lua_run "$latest" "decode/record_fixed/lua" 0 0
LONEJSON_BENCH_CONFIRM_COOLDOWN_SECONDS=0 \
  run_lua_bench confirm-lua "$c_latest" "$baseline" "$latest" 1 \
  >"$tmp_dir/confirm-lua-retry.out" 2>"$tmp_dir/confirm-lua-retry.err"
grep -F 'retrying lua benchmark case 1/' "$tmp_dir/confirm-lua-retry.err" >/dev/null
