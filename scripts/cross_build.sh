#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$repo_root"

if [[ $# -eq 0 ]]; then
  printf 'usage: %s PRESET...\n' "$0" >&2
  exit 2
fi

configure_args=()
if [[ ${LONEJSON_RELEASE_TARGETS+x} ]]; then
  configure_args+=("-DLONEJSON_RELEASE_TARGETS=$LONEJSON_RELEASE_TARGETS")
fi
for preset in "$@"; do
  cmake --preset "$preset" "${configure_args[@]}"
  cmake --build --preset "$preset"
done
