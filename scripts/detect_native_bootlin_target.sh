#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

exec cmake \
  -D "LONEJSON_ROOT=$repo_root" \
  -P "$repo_root/cmake/toolchains/print_native_bootlin_target.cmake"
