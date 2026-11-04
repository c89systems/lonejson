#!/usr/bin/env bash
set -euo pipefail
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
scope=full
if [[ "${1:-}" == --binary ]]; then scope=binary; shift; fi
[[ $# -le 1 ]] || { printf 'usage: %s [--binary] [CHECKSUMS]\n' "$0" >&2; exit 2; }
version=$("$repo_root/scripts/release_version.sh")
checksums=${1:-$repo_root/dist/lonejson-$version-CHECKSUMS}
if [[ "$scope" == binary && $# == 0 ]]; then
  checksums="$repo_root/build/packaging/checksums/lonejson-$version-binary-CHECKSUMS"
fi
checksums_dir=$(CDPATH= cd -- "$(dirname -- "$checksums")" && pwd -P)
checksums="$checksums_dir/$(basename -- "$checksums")"
dist_dir=${LONEJSON_DIST_DIR:-$repo_root/dist}
if [[ "$scope" == full ]]; then dist_dir=$checksums_dir; fi
dist_dir=$(CDPATH= cd -- "$dist_dir" && pwd -P)
if [[ ! ${LONEJSON_RELEASE_TARGETS+x} ]]; then
  LONEJSON_RELEASE_TARGETS='x86_64-linux-gnu;x86_64-linux-musl;aarch64-linux-gnu;aarch64-linux-musl;armhf-linux-gnu;armhf-linux-musl'
  if "$repo_root/scripts/osxcross_available.sh" >/dev/null 2>&1; then
    LONEJSON_RELEASE_TARGETS+=';arm64-apple-darwin'
  fi
fi
export LONEJSON_RELEASE_TARGETS
cmake "-DLONEJSON_ROOT=$repo_root" "-DLONEJSON_VERSION=$version" \
  "-DLONEJSON_DIST_DIR=$dist_dir" \
  "-DLONEJSON_CHECKSUM_SCOPE=$scope" "-DLONEJSON_CHECKSUM_MANIFEST=$checksums" \
  -DVERIFY_ONLY=ON "-DLONEJSON_RELEASE_TARGETS=$LONEJSON_RELEASE_TARGETS" \
  -P "$repo_root/cmake/package_checksums.cmake"
"$repo_root/scripts/verify_release_artifacts.sh" "$repo_root" "$checksums" "$dist_dir" "$scope"
exec "$repo_root/scripts/verify_release_archives.sh" "$repo_root" "$checksums" "" "$dist_dir"
