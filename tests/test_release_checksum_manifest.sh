#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Rationale: release uploads are selected from the checksum manifest, so stale
# or omitted release-looking artifacts must fail before publishing.

repo_root=$1
tmp_dir="$(mktemp -d "$workspace_root/build/test_release_checksum_manifest.XXXXXX")"
# Lifecycle checks never mutate tags. This test also
# runs from source archives nested inside a checkout and must not touch its refs.
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM
trap 'printf "checksum manifest check failed at line %s: %s\n" "$LINENO" "$BASH_COMMAND" >&2' ERR

dist_dir="$tmp_dir/dist"
mkdir -p "$dist_dir"
mkdir -p "$tmp_dir/cmake"
mkdir -p "$tmp_dir/scripts"
cp "$repo_root/cmake/"{lonejson_dist_dir,lonejson_build_workspace,release_artifacts}.cmake "$tmp_dir/cmake/"
cp "$repo_root/scripts/require_build_workspace.sh" "$tmp_dir/scripts/"

for artifact in \
  lonejson-1.2.3.tar.gz \
  lonejson-1.2.3.h.gz \
  lonejson-lua-1.2.3.tar.gz \
  lonejson-1.2.3-1.rockspec \
  lonejson-1.2.3-1.src.rock \
  liblonejson-1.2.3-x86_64-linux-gnu.tar.gz \
  liblonejson-1.2.3-x86_64-linux-musl.tar.gz \
  liblonejson-1.2.3-aarch64-linux-gnu.tar.gz \
  liblonejson-1.2.3-aarch64-linux-musl.tar.gz \
  liblonejson-1.2.3-armhf-linux-gnu.tar.gz \
  liblonejson-1.2.3-armhf-linux-musl.tar.gz \
  liblonejson-1.2.3-arm64-apple-darwin.tar.gz \
  liblonejson-1.2.3-arm64-apple-darwin-smoke-test.zip; do
  printf '%s\n' "$artifact" >"$dist_dir/$artifact"
done

cmake -D "LONEJSON_ROOT=$tmp_dir" -D LONEJSON_VERSION=1.2.3 \
  '-DLONEJSON_RELEASE_TARGETS=x86_64-linux-gnu;x86_64-linux-musl;aarch64-linux-gnu;aarch64-linux-musl;armhf-linux-gnu;armhf-linux-musl;arm64-apple-darwin' \
  -P "$repo_root/cmake/package_checksums.cmake"

manifest="$dist_dir/lonejson-1.2.3-CHECKSUMS"
if [[ ! -f "$manifest" ]]; then
  printf 'missing checksum manifest: %s\n' "$manifest" >&2
  exit 1
fi

for artifact in \
  lonejson-lua-1.2.3.tar.gz \
  liblonejson-1.2.3-arm64-apple-darwin-smoke-test.zip; do
  if ! grep -F "  $artifact" "$manifest" >/dev/null; then
    printf 'checksum manifest is missing release artifact: %s\n' "$artifact" >&2
    cat "$manifest" >&2
    exit 1
  fi
done

# The configured artifact directory must flow through the CMake target rather
# than silently falling back to the checkout's default dist/ directory.
custom_dist_dir="$tmp_dir/custom-dist"
custom_build_dir="$tmp_dir/custom-build"
default_version=$(env -u LONEJSON_VERSION_OVERRIDE \
  "$repo_root/scripts/release_version.sh")
"$repo_root/scripts/stage_release_sources.sh" "$repo_root" "$tmp_dir/source" "$default_version"
mkdir -p "$custom_dist_dir"
printf '%s\n' 'lonejson lifecycle artifact directory' >"$custom_dist_dir/.lonejson-dist"
printf '%s\n' custom >"$custom_dist_dir/lonejson-${default_version}.tar.gz"
for artifact in "${default_version}.h.gz" "lua-${default_version}.tar.gz" \
    "${default_version}-1.rockspec" "${default_version}-1.src.rock"; do
  printf '%s\n' custom >"$custom_dist_dir/lonejson-$artifact"
done
printf '%s\n' custom >"$custom_dist_dir/liblonejson-${default_version}-x86_64-linux-gnu.tar.gz"
env -u LONEJSON_VERSION_OVERRIDE cmake \
  -S "$tmp_dir/source" -B "$custom_build_dir" -G Ninja \
  -D LONEJSON_DIST_DIR="$custom_dist_dir" \
  -D LONEJSON_RELEASE_TARGETS=x86_64-linux-gnu \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF >/dev/null
cmake --build "$custom_build_dir" --target package-checksums >/dev/null
custom_manifest="$custom_dist_dir/lonejson-${default_version}-CHECKSUMS"
[[ -f "$custom_manifest" ]]
grep -F "  lonejson-${default_version}.tar.gz" "$custom_manifest" >/dev/null

# Warm Make invocations must replace the host cache's previous selection.
printf '%s\n' custom >"$custom_dist_dir/liblonejson-${default_version}-aarch64-linux-gnu.tar.gz"
env -u LONEJSON_VERSION_OVERRIDE cmake --preset host -S "$tmp_dir/source" \
  -D "LONEJSON_DIST_DIR=$custom_dist_dir" \
  -D LONEJSON_RELEASE_TARGETS=x86_64-linux-gnu \
  -D LONEJSON_BUILD_TESTS=OFF -D LONEJSON_BUILD_EXAMPLES=OFF >/dev/null
for selected in x86_64-linux-gnu aarch64-linux-gnu; do
  env -u LONEJSON_VERSION_OVERRIDE make -s -C "$tmp_dir/source" package-checksums \
    RELEASE_BUILD_PRESETS="$selected-release" >"$tmp_dir/make-selection.log" 2>&1 || {
    cat "$tmp_dir/make-selection.log" >&2; exit 1;
  }
  grep -F "  liblonejson-${default_version}-${selected}.tar.gz" "$custom_manifest" >/dev/null || {
    printf 'Make reused the previous release inventory instead of %s\n' "$selected" >&2; exit 1;
  }
  [[ $(grep -c '  liblonejson-' "$custom_manifest") == 1 ]]
  cmake -D "LONEJSON_ROOT=$tmp_dir/source" -D "LONEJSON_VERSION=$default_version" \
    -D "LONEJSON_DIST_DIR=$custom_dist_dir" -D "LONEJSON_RELEASE_TARGETS=$selected" \
    -D VERIFY_ONLY=ON -P "$repo_root/cmake/package_checksums.cmake"
done

# The default Make inventory also follows changes in optional Darwin readiness.
cat >"$tmp_dir/source/scripts/osxcross_available.sh" <<'SH'
#!/usr/bin/env bash
[[ ${TEST_DARWIN_AVAILABLE:-0} == 1 ]]
SH
for selected in x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl armhf-linux-gnu armhf-linux-musl arm64-apple-darwin; do
  printf '%s\n' custom >"$custom_dist_dir/liblonejson-${default_version}-${selected}.tar.gz"
done
printf '%s\n' custom >"$custom_dist_dir/liblonejson-${default_version}-arm64-apple-darwin-smoke-test.zip"
for available in 1 0 1; do
  TEST_DARWIN_AVAILABLE=$available env -u LONEJSON_VERSION_OVERRIDE \
    make -s -C "$tmp_dir/source" package-checksums >"$tmp_dir/make-optional.log" 2>&1 || {
    cat "$tmp_dir/make-optional.log" >&2; exit 1;
  }
  darwin_entries=$(grep -c apple-darwin "$custom_manifest" || true)
  [[ $darwin_entries == $((available * 2)) ]] || {
    printf 'Make retained a stale optional Darwin inventory\n' >&2; exit 1;
  }
done

# Missing products fail before replacing a valid complete upload manifest.
cp "$manifest" "$tmp_dir/preserved"
rm "$dist_dir/lonejson-1.2.3.tar.gz"
if cmake -DLONEJSON_ROOT="$tmp_dir" -DLONEJSON_VERSION=1.2.3 \
    -P "$repo_root/cmake/package_checksums.cmake" >"$tmp_dir/missing" 2>&1; then
  printf 'full checksum generation accepted a missing source product\n' >&2; exit 1
fi
cmp "$manifest" "$tmp_dir/preserved"
# Binary preparation preserves the final manifest and unrelated generated bytes.
printf retained >"$dist_dir/unrelated.tar.gz"
cmake -DLONEJSON_ROOT="$tmp_dir" -DLONEJSON_VERSION=1.2.3 \
  -DLONEJSON_CHECKSUM_SCOPE=binary -P "$repo_root/cmake/package_checksums.cmake"
binary="$tmp_dir/build/packaging/checksums/lonejson-1.2.3-binary-CHECKSUMS"
[[ -f "$binary" ]]
! grep -F '  lonejson-1.2.3.tar.gz' "$binary"
cmp "$manifest" "$tmp_dir/preserved"
[[ $(cat "$dist_dir/unrelated.tar.gz") == retained ]]
cmake -DLONEJSON_ROOT="$tmp_dir" -DLONEJSON_VERSION=1.2.3 \
  -DLONEJSON_CHECKSUM_SCOPE=binary -DVERIFY_ONLY=ON \
  -P "$repo_root/cmake/package_checksums.cmake"
cp "$binary" "$tmp_dir/binary-preserved"
sed -i '/liblonejson-1.2.3-armhf-linux-musl.tar.gz/d' "$binary"
if cmake -DLONEJSON_ROOT="$tmp_dir" -DLONEJSON_VERSION=1.2.3 \
    -DLONEJSON_CHECKSUM_SCOPE=binary -DVERIFY_ONLY=ON \
    -P "$repo_root/cmake/package_checksums.cmake" >"$tmp_dir/incomplete" 2>&1; then
  printf 'verification accepted an incomplete binary manifest\n' >&2; exit 1
fi
cp "$tmp_dir/binary-preserved" "$binary"
printf corrupt >"$dist_dir/liblonejson-1.2.3-armhf-linux-musl.tar.gz"
if cmake -DLONEJSON_ROOT="$tmp_dir" -DLONEJSON_VERSION=1.2.3 \
    -DLONEJSON_CHECKSUM_SCOPE=binary -DVERIFY_ONLY=ON \
    -P "$repo_root/cmake/package_checksums.cmake" >"$tmp_dir/corrupt" 2>&1; then
  printf 'verification accepted changed shipped bytes\n' >&2; exit 1
fi
cmp "$binary" "$tmp_dir/binary-preserved"
rm "$dist_dir/liblonejson-1.2.3-armhf-linux-musl.tar.gz"
if cmake -DLONEJSON_ROOT="$tmp_dir" -DLONEJSON_VERSION=1.2.3 \
    -DLONEJSON_CHECKSUM_SCOPE=binary -P "$repo_root/cmake/package_checksums.cmake" >"$tmp_dir/missing-sdk" 2>&1; then
  printf 'binary checksum generation accepted a missing required SDK\n' >&2; exit 1
fi
cmp "$binary" "$tmp_dir/binary-preserved"
