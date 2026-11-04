#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
keep_services="${LONEJSON_E2E_KEEP_DEVSERVICES:-0}"
started_services=0

cleanup() {
  status=$?
  if [[ "$status" -ne 0 ]]; then
    printf '%s\n' 'lonejson e2e failed; Podman state and recent logs follow.' >&2
    "$repo_root/scripts/devenv.sh" ps >&2 || true
    "$repo_root/scripts/devenv.sh" logs --tail 120 >&2 || true
  fi
  if [[ "$started_services" = 1 && "$keep_services" != 1 ]]; then
    if [[ "$status" == 0 ]]; then
      "$repo_root/scripts/devenv.sh" reset || status=1
    else
      "$repo_root/scripts/devenv.sh" down || status=1
    fi
  fi
  exit "$status"
}
trap cleanup EXIT

cd "$repo_root"

"$repo_root/scripts/devenv.sh" up
started_services=1
make --no-print-directory e2e-build
ctest --preset e2e
