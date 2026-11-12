#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:-$(cd "$(dirname "$0")/.." && pwd -P)}
work=$(mktemp -d "$repo_root/build/bench-controller-test.XXXXXX")
trap 'status=$?; if [[ $status != 0 ]]; then cat "$work/log" >&2; fi; rm -rf "$work"; exit "$status"' EXIT
export BENCH_TEST_EVENTS="$work/events" BENCH_TEST_MODE=pass
fixture="$work/source"
mkdir -p "$fixture/scripts" "$fixture/bench" "$fixture/build/host" "$fixture/perflogs/hosts/fixture/lua" "$work/bin"
sed 's/^#define BENCH_SETTLE_SECONDS .*/#define BENCH_SETTLE_SECONDS 1u/' \
  "$repo_root/bench/lonejson_measure.h" > "$fixture/bench/lonejson_measure.h"
printf '{}\n' > "$fixture/perflogs/hosts/fixture/baseline.json"
printf '{}\n' > "$fixture/perflogs/hosts/fixture/lua/baseline.json"
cat > "$fixture/Makefile" <<'EOF'
benchmark-prepare:
	@test "$$BENCH_TEST_MODE" != prepare-fail
EOF
cat > "$fixture/scripts/run_benchmark.sh" <<'EOF'
#!/usr/bin/env bash
if [[ "$1" == --affinity ]]; then printf 'unbound\n'; exit 0; fi
if [[ "$1" == --identity ]]; then
  printf 'identity\n' >> "$BENCH_TEST_EVENTS"
  printf '%s\n' fixture 0000000000000000000000000000000000000000 0000000000000000000000000000000000000000 0000000000000000000000000000000000000000
  exit 0
fi
exec "$@"
EOF
cat > "$fixture/scripts/prepare_benchmark_reference.sh" <<'EOF'
#!/usr/bin/env bash
[[ $# == 4 && ${#2} == 40 && ${#3} == 40 && ${#4} == 40 ]]
printf 'reference-preparation\n' >> "$BENCH_TEST_EVENTS"
[[ "$BENCH_TEST_MODE" != reference-fail ]] || exit 21
printf '%s\n' "$BENCH_TEST_REFERENCE"
EOF
cat > "$fixture/scripts/run_lua_benchmark.sh" <<'EOF'
#!/usr/bin/env bash
[[ "$LONEJSON_BENCH_IMPLEMENTATION_COUNT" == 2 ]]
[[ -n "$LONEJSON_BENCH_SOURCE_REVISION" && -n "$LONEJSON_BENCH_SRC_TREE" && -n "$LONEJSON_BENCH_INCLUDE_TREE" && -n "$LONEJSON_BENCH_LUA_TREE" ]]
kind=lua
[[ "$5" != *-warmup-* ]] || kind=warmup_lua
printf '%s %s %s %s\n' "$kind" "$$" "$5" "$6" >> "$BENCH_TEST_EVENTS"
[[ "$6" == - || -s "$6" ]]
[[ "$BENCH_TEST_MODE" != worker-fail ]] || exit 17
printf 'Lua measurement\n' > "$5"
EOF
cat > "$fixture/build/host/lonejson_bench" <<'EOF'
#!/usr/bin/env bash
set -eu
case "$1" in
  round)
    [[ -n "$LONEJSON_BENCH_SOURCE_REVISION" && -n "$LONEJSON_BENCH_SRC_TREE" && -n "$LONEJSON_BENCH_INCLUDE_TREE" && -n "$LONEJSON_BENCH_LUA_TREE" ]]
    if [[ "$BENCH_TEST_MODE" == timeout ]]; then
      [[ "$LONEJSON_BENCH_IMPLEMENTATION_COUNT" == 1 ]]
    else
      [[ "$LONEJSON_BENCH_IMPLEMENTATION_COUNT" == 2 ]]
    fi
    kind=c
    [[ "$3" != *-warmup-* ]] || kind=warmup_c
    printf '%s %s %s %s\n' "$kind" "$$" "$3" "$4" >> "$BENCH_TEST_EVENTS"
    [[ "$BENCH_TEST_MODE" != warmup-fail || "$kind" != warmup_c ]] || exit 23
    [[ "$4" == - || -s "$4" ]]
    if [[ "$BENCH_TEST_MODE" == timeout ]]; then
      trap 'printf killed >> "$BENCH_TEST_EVENTS"; exit 143' TERM
      sleep 20
    fi
    printf 'C measurement\n' > "$3" ;;
  merge)
    [[ $# == 403 ]]
    for file in "${@:5}"; do [[ -s "$file" ]]; done
    printf 'complete measurement\n' > "$2" ;;
  compare) exit 0 ;;
  gate)
    printf 'gate %s\n' "$3" >> "$BENCH_TEST_EVENTS"
    [[ "$BENCH_TEST_MODE" != c-fail || "$3" != */c.json ]]
    [[ "$BENCH_TEST_MODE" != lua-fail || "$3" != */lua.json ]] ;;
  *) exit 2 ;;
esac
EOF
cat > "$work/bin/cmake" <<'EOF'
#!/usr/bin/env bash
[[ "$BENCH_TEST_MODE" != preflight-fail ]] || { printf 'incompatible reference\n' >&2; exit 19; }
for argument; do
  if [[ "$argument" == -DREFERENCE_INFO_DIR=* ]]; then
    directory=${argument#*=}
    mkdir -p "$directory"
    printf fixture > "$directory/source_revision"
    for field in source_src_tree source_include_tree source_lua_tree; do
      printf '0000000000000000000000000000000000000000' > "$directory/$field"
    done
  fi
done
printf '%s\n' '-- Benchmark reference source: fixture' \
  '-- Benchmark reference src tree: 0000000000000000000000000000000000000000' \
  '-- Benchmark reference include tree: 0000000000000000000000000000000000000000' \
  '-- Benchmark reference lua tree: 0000000000000000000000000000000000000000'
EOF
chmod +x "$fixture/scripts/"* "$fixture/build/host/lonejson_bench" "$work/bin/cmake"
export LONEJSON_BENCH_HOST_ID=fixture PATH="$work/bin:$PATH"
export BENCH_TEST_REFERENCE="$work/reference"
cp -R "$fixture" "$BENCH_TEST_REFERENCE"
for mode in preflight-fail reference-fail prepare-fail worker-fail warmup-fail c-fail lua-fail pass; do
  export BENCH_TEST_MODE=$mode
  : > "$BENCH_TEST_EVENTS"
  status=0
  bash "$repo_root/scripts/benchmark.sh" check "$fixture" "$work/$mode" > "$work/log" 2>&1 || status=$?
  if [[ "$mode" == pass ]]; then [[ $status == 0 ]]; else [[ $status != 0 ]]; fi
  if [[ "$mode" == preflight-fail ]]; then [[ ! -s "$BENCH_TEST_EVENTS" ]]; continue; fi
  if [[ "$mode" == reference-fail || "$mode" == prepare-fail ]]; then
    ! grep -Eq '^(c|lua|gate) ' "$BENCH_TEST_EVENTS"; continue
  fi
  retained=$(sed -n 's/^benchmark diagnostics retained: //p' "$work/log")
  [[ -d "$retained" ]]
  if [[ "$mode" == worker-fail || "$mode" == warmup-fail ]]; then [[ ! -e "$retained/candidate/c.json" ]]; continue; fi
  grep -q '^warmup_c ' "$BENCH_TEST_EVENTS"
  grep -q '^warmup_lua ' "$BENCH_TEST_EVENTS"
  [[ $(grep -c '^identity$' "$BENCH_TEST_EVENTS") == 1 ]]
  [[ $(grep -c '^c ' "$BENCH_TEST_EVENTS") == 800 ]]
  [[ $(grep -c '^lua ' "$BENCH_TEST_EVENTS") == 800 ]]
  [[ $(awk '/^(c|lua) /{print $2}' "$BENCH_TEST_EVENTS" | sort -u | wc -l) == 1600 ]]
  [[ $(grep -c '^gate ' "$BENCH_TEST_EVENTS") == 2 ]]
  [[ -s "$retained/candidate/c.json" && -s "$retained/candidate/lua.json" ]]
done
# Both implementations share the same deadline and alternate order each round.
export BENCH_TEST_MODE=pass LONEJSON_BENCH_REFERENCE_REVISION=fixture
: > "$BENCH_TEST_EVENTS"
bash "$repo_root/scripts/benchmark.sh" compare "$fixture" "$work/compare" "$work/reference" > "$work/log" 2>&1
[[ $(grep -c '^c ' "$BENCH_TEST_EVENTS") == 800 ]]
[[ $(grep -c '^lua ' "$BENCH_TEST_EVENTS") == 800 ]]
awk '/^(c|lua) / && !/-pilot[.]json / {if (/[/]reference[/]/) print "reference"; else print "candidate"; if (++count == 8) exit}' "$BENCH_TEST_EVENTS" > "$work/order"
printf '%s\n' reference candidate reference candidate candidate reference candidate reference > "$work/expected"
cmp "$work/order" "$work/expected"
if bash "$repo_root/scripts/benchmark.sh" freeze "$fixture" "$work/unclean-freeze" > "$work/log" 2>&1; then
  printf 'froze a reference without committed source ownership\n' >&2; exit 1
fi
grep -F 'clean, committed Git checkout' "$work/log" >/dev/null
# Exercise the real timeout implementation with a shorter test deadline.
real_timeout=$(command -v timeout)
cat > "$work/bin/timeout" <<EOF
#!/usr/bin/env bash
set -eu
[[ \$1 == -k && \$2 == 1s && \$3 == 299s ]]
shift 3
exec '$real_timeout' -k 1s 0.3s "\$@"
EOF
chmod +x "$work/bin/timeout"
export BENCH_TEST_MODE=timeout
status=0
bash "$repo_root/scripts/benchmark.sh" run "$fixture" "$work/deadline" > "$work/log" 2>&1 || status=$?
[[ $status == 124 || $status == 137 ]]
grep -F killed "$BENCH_TEST_EVENTS" >/dev/null
printf 'PASS complete rounds, fresh processes, plan reuse, shared gates, retained failures and total timeout\n'
