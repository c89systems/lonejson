#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
[[ $# -eq 1 ]] || { printf 'usage: %s <lifecycle-target-id>\n' "$0" >&2; exit 2; }
target_id=$1

case "$target_id" in
  x86_64-linux-gnu|x86_64-linux-musl|aarch64-linux-gnu|aarch64-linux-musl|armhf-linux-gnu|armhf-linux-musl)
    "$repo_root/scripts/cpkt-toolchains.sh" ensure "$target_id" >/dev/null
    ;;
  arm64-apple-darwin)
    "$repo_root/scripts/cpkt-toolchains.sh" ensure "$target_id" >/dev/null
    ;;
  *)
    printf 'usage: %s <lifecycle-target-id>\n' "$0" >&2
    exit 2
    ;;
esac

exec cmake \
  -D "LONEJSON_SOURCE_DIR=$repo_root" \
  -D "LONEJSON_CPKT_TARGET_ID=$target_id" \
  -P "$repo_root/cmake/fetch_cpkt.cmake"
