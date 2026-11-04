#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
preset=${1:-}

cd "$repo_root"

if [[ -z "$preset" ]]; then
  printf 'usage: %s PRESET [--exclude REGEX]\n' "$0" >&2
  exit 2
fi
shift
if [[ "${1:-}" == --exclude && $# -eq 2 ]]; then
  exec ctest --no-tests=error --preset "$preset" --output-on-failure -E "$2"
fi
if [[ $# -ne 0 ]]; then
  printf 'usage: %s PRESET [--exclude REGEX]\n' "$0" >&2
  exit 2
fi
exec ctest --no-tests=error --preset "$preset" --output-on-failure
