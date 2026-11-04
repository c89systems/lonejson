#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

detect_target_id() {
  "$repo_root/scripts/detect_native_target.sh"
}

target_id="${LONEJSON_CPKT_TARGET_ID:-}"
if [[ -z "${target_id}" ]]; then
  target_id="$(detect_target_id)"
fi
bundle_root="${repo_root}/.cache/cpkt/${target_id}/root"

if [[ ! -d "${bundle_root}" ]]; then
  printf '%s\n' "missing cpkt bundle at ${bundle_root}" >&2
  printf '%s\n' "hint: run 'make deps-host' first" >&2
  exit 1
fi

printf '%s\n' "${bundle_root}"
