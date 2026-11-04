#!/usr/bin/env bash
set -euo pipefail
[[ $# == 3 ]] || { printf 'usage: %s LUA GENERATOR OUTPUT_DIR\n' "$0" >&2; exit 2; }
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
"$repo_root/scripts/require_build_workspace.sh" "$3"
# The operation owns this workspace; the generator reuses matching fixture data.
exec "$1" "$2" "$3"
