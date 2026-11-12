#!/usr/bin/env bash
set -euo pipefail
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
[[ $# -gt 0 ]] || { printf 'usage: run_benchmark.sh COMMAND [ARGUMENT...]\n' >&2; exit 2; }
resolve_affinity() {
  local field value
  bench_allowed=
  cpu=
  if [[ $(uname -s) == Linux ]]; then
    command -v taskset >/dev/null || { printf 'taskset is required for Linux benchmark affinity\n' >&2; exit 1; }
    if [[ -r /proc/self/status ]]; then
      while read -r field value; do
        if [[ "$field" == Cpus_allowed_list: ]]; then bench_allowed=$value; break; fi
      done < /proc/self/status
    fi
    # Avoid the boot CPU, which commonly handles disproportionate IRQ work.
    # Keep selection deterministic and respect an already restricted mask.
    cpu=${LONEJSON_BENCH_CPU:-${bench_allowed##*[-,]}}
    [[ "$cpu" =~ ^[0-9]+$ ]] || { printf 'LONEJSON_BENCH_CPU must be one allowed CPU number\n' >&2; exit 2; }
    export LONEJSON_BENCH_AFFINITY="cpu:$cpu"
  else
    export LONEJSON_BENCH_AFFINITY=unbound
  fi
}
if [[ "$1" == --affinity ]]; then
  resolve_affinity
  printf '%s\n' "$LONEJSON_BENCH_AFFINITY"
  exit 0
fi
owning_git_root=
if [[ -z ${LONEJSON_BENCH_SOURCE_REVISION:-} || -z ${LONEJSON_BENCH_SRC_TREE:-} || -z ${LONEJSON_BENCH_INCLUDE_TREE:-} || -z ${LONEJSON_BENCH_LUA_TREE:-} ]]; then
  owning_git_root=$(git -C "$repo_root" rev-parse --show-toplevel 2>/dev/null || true)
fi
if [[ -z ${LONEJSON_BENCH_SOURCE_REVISION:-} ]]; then
  if [[ "$owning_git_root" == "$repo_root" ]]; then
    LONEJSON_BENCH_SOURCE_REVISION=$(git -C "$repo_root" rev-parse HEAD)
    if [[ -n $(git -C "$repo_root" status --porcelain --untracked-files=normal) ]]; then
      LONEJSON_BENCH_SOURCE_REVISION+=:dirty
    fi
  elif [[ -f "$repo_root/VERSION" ]]; then
    LONEJSON_BENCH_SOURCE_REVISION="release:$(cat "$repo_root/VERSION")"
  else
    printf 'benchmark source revision is unknown; set LONEJSON_BENCH_SOURCE_REVISION\n' >&2
    exit 1
  fi
fi
export LONEJSON_BENCH_SOURCE_REVISION
if [[ -z ${LONEJSON_BENCH_SRC_TREE:-} || -z ${LONEJSON_BENCH_INCLUDE_TREE:-} || -z ${LONEJSON_BENCH_LUA_TREE:-} ]]; then
  if [[ "$owning_git_root" == "$repo_root" ]]; then
    trees=$(git -C "$repo_root" rev-parse HEAD:src HEAD:include HEAD:lua)
    read -r LONEJSON_BENCH_SRC_TREE LONEJSON_BENCH_INCLUDE_TREE LONEJSON_BENCH_LUA_TREE <<< "${trees//$'\n'/ }"
  else
    LONEJSON_BENCH_SRC_TREE=unversioned
    LONEJSON_BENCH_INCLUDE_TREE=unversioned
    LONEJSON_BENCH_LUA_TREE=unversioned
  fi
fi
export LONEJSON_BENCH_SRC_TREE LONEJSON_BENCH_INCLUDE_TREE LONEJSON_BENCH_LUA_TREE
if [[ "$1" == --identity ]]; then
  printf '%s\n' "$LONEJSON_BENCH_SOURCE_REVISION" "$LONEJSON_BENCH_SRC_TREE" \
    "$LONEJSON_BENCH_INCLUDE_TREE" "$LONEJSON_BENCH_LUA_TREE"
  exit 0
fi
resolve_affinity
if [[ "$LONEJSON_BENCH_AFFINITY" != unbound && "$bench_allowed" != "$cpu" ]]; then
  exec taskset -c "$cpu" "$@"
fi
exec "$@"
