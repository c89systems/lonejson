#!/usr/bin/env bash
# A fixed sequence of whole-suite workers; timeout owns the complete process group.
set -euo pipefail
if [[ ${1:-} != --bounded ]]; then
  for timer in timeout gtimeout; do
    if command -v "$timer" >/dev/null 2>&1; then
      exec "$timer" -k 1s 299s bash "$0" --bounded "$@"
    fi
  done
  printf 'benchmark requires timeout (GNU coreutils; gtimeout on macOS)\n' >&2
  exit 1
fi
shift
action=${1:?usage: benchmark.sh run|check|compare|freeze SOURCE OUTPUT [REFERENCE_SOURCE]}
root=${2:?source required}
out=${3:?output directory required}
reference=${4:-}
case "$action" in run|check|compare|freeze) ;; *) exit 2;; esac
[[ "$action" != compare || -n "$reference" ]] || exit 2
root=$(cd "$root" && pwd -P)
mkdir -p "$out"
out=$(mktemp -d "$(cd "$out" && pwd -P)/run.XXXXXX")
export LONEJSON_BENCH_HOST_ID=${LONEJSON_BENCH_HOST_ID:-$("$root/scripts/bench_host_id.sh")}
engine="$root/build/host/lonejson_bench"
rounds=$(awk '/^#define BENCH_PROCESS_COUNT / {gsub(/u/, "", $3); print $3}' "$root/bench/lonejson_measure.h")
[[ "$rounds" =~ ^[1-9][0-9]*$ ]] || exit 1
settle=$(awk '/^#define BENCH_SETTLE_SECONDS / {gsub(/u/, "", $3); print $3}' "$root/bench/lonejson_measure.h")
[[ "$settle" =~ ^[0-9]+$ ]] || exit 1
variants=(candidate)
reference_src_tree=unversioned
reference_include_tree=unversioned
reference_lua_tree=unversioned
baseline_root="$root/perflogs/hosts/$LONEJSON_BENCH_HOST_ID"
if [[ "$action" == freeze ]]; then
  [[ $(git -C "$root" rev-parse --show-toplevel 2>/dev/null || true) == "$root" && -z $(git -C "$root" status --porcelain) ]] || {
    printf 'baseline freezing requires a clean, committed Git checkout\n' >&2; exit 1;
  }
fi
if [[ "$action" == check ]]; then
  if [[ ! -f "$baseline_root/baseline.json" || ! -f "$baseline_root/lua/baseline.json" ]]; then
    printf 'bench-check skipped: missing frozen benchmark baseline for host %s\n' "$LONEJSON_BENCH_HOST_ID" >&2
    printf "  C baseline: %s\n  Lua baseline: %s\n" "$baseline_root/baseline.json" "$baseline_root/lua/baseline.json" >&2
    exit 0
  fi
  preflight=$(cmake "-DPOLICY_HEADER=$root/bench/lonejson_measure.h" \
    "-DC_BASELINE=$baseline_root/baseline.json" "-DLUA_BASELINE=$baseline_root/lua/baseline.json" \
    "-DREFERENCE_INFO_DIR=$out/reference-info" \
    -P "$root/cmake/check_benchmark_baselines.cmake")
  printf '%s\n' "$preflight"
  LONEJSON_BENCH_REFERENCE_REVISION=$(< "$out/reference-info/source_revision")
  reference_src_tree=$(< "$out/reference-info/source_src_tree")
  reference_include_tree=$(< "$out/reference-info/source_include_tree")
  reference_lua_tree=$(< "$out/reference-info/source_lua_tree")
  [[ -n "$LONEJSON_BENCH_REFERENCE_REVISION" && -n "$reference_src_tree" && -n "$reference_include_tree" && -n "$reference_lua_tree" ]] || exit 1
  export LONEJSON_BENCH_REFERENCE_REVISION
  if [[ -z "$reference" ]]; then
    reference=$("$root/scripts/prepare_benchmark_reference.sh" "$root" "$reference_src_tree" "$reference_include_tree" "$reference_lua_tree")
  fi
elif [[ -n "$reference" ]]; then
  if trees=$(git -C "$root" rev-parse "${LONEJSON_BENCH_REFERENCE_REVISION:-}:src" "${LONEJSON_BENCH_REFERENCE_REVISION:-}:include" "${LONEJSON_BENCH_REFERENCE_REVISION:-}:lua" 2>/dev/null); then
    read -r reference_src_tree reference_include_tree reference_lua_tree <<< "${trees//$'\n'/ }"
  fi
fi
if [[ -n "$reference" ]]; then
  reference=$(cd "$reference" && pwd -P)
  [[ "$reference" != "$root" ]] || { printf "reference must be a separate source checkout\n" >&2; exit 2; }
  variants=(reference candidate)
fi
export LONEJSON_BENCH_IMPLEMENTATION_COUNT=${#variants[@]}
for variant in "${variants[@]}"; do
  source="$root"
  [[ "$variant" != reference ]] || source=$reference
  make --no-print-directory -C "$source" benchmark-prepare
done
trap 'status=$?; printf "benchmark diagnostics retained: %s\n" "$out" >&2; exit "$status"' EXIT
candidate_identity=$(unset LONEJSON_BENCH_SOURCE_REVISION LONEJSON_BENCH_SRC_TREE LONEJSON_BENCH_INCLUDE_TREE LONEJSON_BENCH_LUA_TREE
  "$root/scripts/run_benchmark.sh" --identity)
{
  read -r candidate_revision
  read -r candidate_src_tree
  read -r candidate_include_tree
  read -r candidate_lua_tree
} <<< "$candidate_identity"
affinity=$("$root/scripts/run_benchmark.sh" --affinity)
if [[ "$affinity" == cpu:* ]]; then
  export LONEJSON_BENCH_CPU=${affinity#cpu:}
  taskset -pc "$LONEJSON_BENCH_CPU" "$BASHPID" >/dev/null
fi
worker() {
  local variant=$1 runtime=$2 index=$3 source="$root" plan=- output lua_build
  [[ "$variant" != reference ]] || source=$reference
  output="$out/$variant/$runtime-$index.json"
  [[ "$index" == pilot || "$index" == warmup-* ]] || plan="$out/$variant/$runtime-pilot.json"
  mkdir -p "$out/$variant"
  (
    cd "$source"
    export LONEJSON_BENCH_ROUND=$index
    export LONEJSON_BENCH_PLAN="$source/bench/workloads.tsv"
    # Each checkout stamps its own identity; an archive reference must declare it.
    if [[ "$variant" == reference ]]; then
      export LONEJSON_BENCH_SOURCE_REVISION=${LONEJSON_BENCH_REFERENCE_REVISION:?set reference source revision}
      export LONEJSON_BENCH_SRC_TREE=$reference_src_tree
      export LONEJSON_BENCH_INCLUDE_TREE=$reference_include_tree
      export LONEJSON_BENCH_LUA_TREE=$reference_lua_tree
    else
      export LONEJSON_BENCH_SOURCE_REVISION=$candidate_revision
      export LONEJSON_BENCH_SRC_TREE=$candidate_src_tree
      export LONEJSON_BENCH_INCLUDE_TREE=$candidate_include_tree
      export LONEJSON_BENCH_LUA_TREE=$candidate_lua_tree
    fi
    if [[ "$runtime" == c ]]; then
      ./scripts/run_benchmark.sh ./build/host/lonejson_bench round \
        tests/fixtures/vendor/json_test_suite/test_parsing "$output" "$plan"
    else
      lua_build="$source/build/host"
      if [[ $(uname -s) == Linux ]]; then
        lua_build+=-curl
      else
        export LUA=${LUA:-$(./scripts/resolve_lua55.sh)} LUAROCKS=${LUAROCKS:-luarocks}
        export LUA_ROCK_TREE="$source/build/luarocks" LONEJSON_LUA_BENCH_LIBDIR="$source/build/host"
      fi
      ./scripts/run_lua_benchmark.sh "$source" "$lua_build" \
        "$source/bench/lonejson_lua_bench.lua" round "$output" "$plan"
    fi
  )
}
# Settle the transition from compilation to sustained benchmark work.
# Warmup results remain separate; no recorded sample is selected or discarded.
warmup_started=$SECONDS
warmup_round=0
while ((SECONDS - warmup_started < settle)); do
  warmup_round=$((warmup_round + 1))
  order=("${variants[@]}")
  if [[ ${#variants[@]} == 2 && $((warmup_round % 2)) == 0 ]]; then order=(candidate reference); fi
  for runtime in c lua; do
    for variant in "${order[@]}"; do worker "$variant" "$runtime" "warmup-$warmup_round"; done
  done
done
printf 'benchmark excluded warmup complete: %d rounds in %d seconds\n' "$warmup_round" "$((SECONDS - warmup_started))" >&2
for variant in "${variants[@]}"; do
  worker "$variant" c pilot
  worker "$variant" lua pilot
done
for ((round=1; round<=rounds; round++)); do
  order=("${variants[@]}")
  if [[ ${#variants[@]} == 2 && $((round % 2)) == 0 ]]; then order=(candidate reference); fi
  for runtime in c lua; do
    for variant in "${order[@]}"; do worker "$variant" "$runtime" "$round"; done
  done
  printf 'benchmark round %d/%d complete\n' "$round" "$rounds" >&2
done
for variant in "${variants[@]}"; do
  for runtime in c lua; do
    files=()
    for ((round=1; round<=rounds; round++)); do files+=("$out/$variant/$runtime-$round.json"); done
    "$engine" merge "$out/$variant/$runtime.json" "$out/$variant/$runtime-history.jsonl" \
      "$out/$variant/$runtime-runs" "${files[@]}"
  done
done
status=0
if [[ "$action" == check || "$action" == compare ]]; then
  for runtime in c lua; do
    baseline="$out/reference/$runtime.json"
    "$engine" compare "$baseline" "$out/candidate/$runtime.json" || status=1
    "$engine" gate "$baseline" "$out/candidate/$runtime.json" || status=$?
  done
elif [[ "$action" == freeze ]]; then
  # Validate both before updating either baseline. Preserve previous bytes.
  "$engine" freeze-baseline "$out/candidate/c-history.jsonl" "$out/c-baseline.json"
  "$engine" freeze-baseline "$out/candidate/lua-history.jsonl" "$out/lua-baseline.json"
  mkdir -p "$baseline_root/lua" "$baseline_root/runs" "$baseline_root/lua/runs"
  stamp=$(date +%s)
  if [[ -f "$baseline_root/baseline.json" ]]; then cp "$baseline_root/baseline.json" "$baseline_root/runs/baseline-$stamp.json"; fi
  if [[ -f "$baseline_root/lua/baseline.json" ]]; then cp "$baseline_root/lua/baseline.json" "$baseline_root/lua/runs/baseline-$stamp.json"; fi
  cp "$out/c-baseline.json" "$baseline_root/baseline.json"
  cp "$out/lua-baseline.json" "$baseline_root/lua/baseline.json"
fi
exit "$status"
