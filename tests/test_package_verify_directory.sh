#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/package-verify-directory.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
fixture="$work/repo"
custom="$work/custom artifacts"
mkdir -p "$fixture/scripts" "$fixture/cmake" "$custom"
cp "$repo_root/scripts/"{package-verify,require_build_workspace}.sh "$fixture/scripts/"
cp "$repo_root/cmake/"{package_checksums,release_artifacts,lonejson_dist_dir,lonejson_build_workspace}.cmake "$fixture/cmake/"
printf '#!/usr/bin/env bash\nprintf "1.2.3\\n"\n' >"$fixture/scripts/release_version.sh"
cat >"$fixture/scripts/verify_release_artifacts.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
[[ $# == 4 && $3 == "$EXPECTED_PAYLOAD" && $4 == "$EXPECTED_SCOPE" ]]
[[ "$LONEJSON_RELEASE_TARGETS" == "$EXPECTED_TARGETS" ]]
printf 'privacy\n' >>"$VERIFY_EVENTS"
SH
cat >"$fixture/scripts/verify_release_archives.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
[[ $# == 4 && $4 == "$EXPECTED_PAYLOAD" ]]
[[ "$LONEJSON_RELEASE_TARGETS" == "$EXPECTED_TARGETS" ]]
printf 'consumers\n' >>"$VERIFY_EVENTS"
SH
cat >"$fixture/scripts/osxcross_available.sh" <<'SH'
#!/usr/bin/env bash
[[ ${VERIFY_DARWIN_READY:-0} == 1 ]]
SH
chmod +x "$fixture/scripts/"*.sh
for artifact in lonejson-1.2.3.tar.gz lonejson-1.2.3.h.gz lonejson-lua-1.2.3.tar.gz \
    lonejson-1.2.3-1.rockspec lonejson-1.2.3-1.src.rock liblonejson-1.2.3-x86_64-linux-gnu.tar.gz; do
  printf '%s\n' "$artifact" >"$custom/$artifact"
done
printf 'lonejson lifecycle artifact directory\n' >"$custom/.lonejson-dist"
export LONEJSON_RELEASE_TARGETS=x86_64-linux-gnu
export EXPECTED_TARGETS=$LONEJSON_RELEASE_TARGETS
cmake -D "LONEJSON_ROOT=$fixture" -D LONEJSON_VERSION=1.2.3 \
  -D "LONEJSON_DIST_DIR=$custom" -D LONEJSON_RELEASE_TARGETS=x86_64-linux-gnu \
  -P "$fixture/cmake/package_checksums.cmake"
export EXPECTED_PAYLOAD="$custom" EXPECTED_SCOPE=full VERIFY_EVENTS="$work/events"
(cd "$work" && "$fixture/scripts/package-verify.sh" 'custom artifacts/lonejson-1.2.3-CHECKSUMS')
printf 'privacy\nconsumers\n' >"$work/expected"
cmp "$work/expected" "$VERIFY_EVENTS"
[[ ! -e "$fixture/dist" ]]
: >"$VERIFY_EVENTS"
printf changed >>"$custom/lonejson-1.2.3.h.gz"
if "$fixture/scripts/package-verify.sh" "$custom/lonejson-1.2.3-CHECKSUMS" >"$work/changed" 2>&1; then
  printf 'package verification accepted changed artifact bytes\n' >&2; exit 1
fi
grep -F 'checksums differ' "$work/changed" >/dev/null
[[ ! -s "$VERIFY_EVENTS" ]]
# Binary manifests live under build/, separately from the dist/ payload.
mkdir -p "$fixture/dist"
cp "$custom/"lonejson-* "$custom/"liblonejson-* "$fixture/dist/"
cmake -D "LONEJSON_ROOT=$fixture" -D LONEJSON_VERSION=1.2.3 \
  -D LONEJSON_RELEASE_TARGETS=x86_64-linux-gnu -D LONEJSON_CHECKSUM_SCOPE=binary \
  -P "$fixture/cmake/package_checksums.cmake"
export EXPECTED_PAYLOAD="$fixture/dist" EXPECTED_SCOPE=binary
"$fixture/scripts/package-verify.sh" --binary
cmp "$work/expected" "$VERIFY_EVENTS"

# A binary-scope manifest remains under build/ when its payload is external.
cmake -D "LONEJSON_ROOT=$fixture" -D LONEJSON_VERSION=1.2.3 \
  -D "LONEJSON_DIST_DIR=$custom" -D LONEJSON_RELEASE_TARGETS=x86_64-linux-gnu \
  -D LONEJSON_CHECKSUM_SCOPE=binary -P "$fixture/cmake/package_checksums.cmake"
: >"$VERIFY_EVENTS"
export EXPECTED_PAYLOAD="$custom"
LONEJSON_DIST_DIR="$custom" "$fixture/scripts/package-verify.sh" --binary
cmp "$work/expected" "$VERIFY_EVENTS"
: >"$VERIFY_EVENTS"
mv "$custom/lonejson-1.2.3.h.gz" "$work/header.gz"
if LONEJSON_DIST_DIR="$custom" "$fixture/scripts/package-verify.sh" --binary >"$work/missing" 2>&1; then
  printf 'binary verification borrowed the default payload directory\n' >&2; exit 1
fi
grep -F 'missing or unsafe required release artifact' "$work/missing" >/dev/null
[[ ! -s "$VERIFY_EVENTS" ]]
mv "$work/header.gz" "$custom/lonejson-1.2.3.h.gz"
make -s -n -C "$repo_root" package-verify-binary DIST_DIR="$custom" >"$work/make-plan"
grep -F "LONEJSON_DIST_DIR=\"$custom\" ./scripts/package-verify.sh --binary" "$work/make-plan" >/dev/null

# Direct callers use the same optional-Darwin default as Make/the matrix, in
# both full/custom-directory and binary scopes. Exercise warm readiness changes.
linux_targets='x86_64-linux-gnu;x86_64-linux-musl;aarch64-linux-gnu;aarch64-linux-musl;armhf-linux-gnu;armhf-linux-musl'
export EXPECTED_PAYLOAD="$fixture/dist"
for ready in 1 0 1; do
  targets=$linux_targets
  if [[ $ready == 1 ]]; then targets+=';arm64-apple-darwin'; fi
  payload="$custom/default-$ready"
  mkdir -p "$payload"
  printf 'lonejson lifecycle artifact directory\n' >"$payload/.lonejson-dist"
  for artifact in lonejson-1.2.3.tar.gz lonejson-1.2.3.h.gz lonejson-lua-1.2.3.tar.gz \
      lonejson-1.2.3-1.rockspec lonejson-1.2.3-1.src.rock; do
    printf '%s\n' "$artifact" >"$payload/$artifact"
  done
  IFS=';' read -r -a selected_targets <<<"$targets"
  for target in "${selected_targets[@]}"; do
    printf '%s\n' "$target" >"$payload/liblonejson-1.2.3-$target.tar.gz"
    if [[ $target == arm64-apple-darwin ]]; then
      printf smoke >"$payload/liblonejson-1.2.3-$target-smoke-test.zip"
    fi
  done
  cmake -D "LONEJSON_ROOT=$fixture" -D LONEJSON_VERSION=1.2.3 \
    -D "LONEJSON_DIST_DIR=$payload" -D "LONEJSON_RELEASE_TARGETS=$targets" \
    -P "$fixture/cmake/package_checksums.cmake"
  : >"$VERIFY_EVENTS"
  EXPECTED_PAYLOAD="$payload" EXPECTED_SCOPE=full EXPECTED_TARGETS="$targets" \
    env -u LONEJSON_RELEASE_TARGETS VERIFY_DARWIN_READY="$ready" \
    "$fixture/scripts/package-verify.sh" "$payload/lonejson-1.2.3-CHECKSUMS"
  cmp "$work/expected" "$VERIFY_EVENTS"
  cp "$payload/"* "$fixture/dist/"
  cmake -D "LONEJSON_ROOT=$fixture" -D LONEJSON_VERSION=1.2.3 \
    -D "LONEJSON_RELEASE_TARGETS=$targets" -D LONEJSON_CHECKSUM_SCOPE=binary \
    -P "$fixture/cmake/package_checksums.cmake"
  : >"$VERIFY_EVENTS"
  EXPECTED_TARGETS="$targets" env -u LONEJSON_RELEASE_TARGETS VERIFY_DARWIN_READY="$ready" \
    "$fixture/scripts/package-verify.sh" --binary
  cmp "$work/expected" "$VERIFY_EVENTS"
done

# Explicit inventories take precedence over host readiness, including deliberate
# Linux-only verification on a Darwin-capable host and Darwin on an unready host.
for ready in 0 1; do
  payload="$custom/default-$((1 - ready))"
  targets=$linux_targets
  if [[ $ready == 0 ]]; then targets+=';arm64-apple-darwin'; fi
  : >"$VERIFY_EVENTS"
  EXPECTED_PAYLOAD="$payload" EXPECTED_SCOPE=full EXPECTED_TARGETS="$targets" \
    LONEJSON_RELEASE_TARGETS="$targets" VERIFY_DARWIN_READY="$ready" \
    "$fixture/scripts/package-verify.sh" "$payload/lonejson-1.2.3-CHECKSUMS"
  cmp "$work/expected" "$VERIFY_EVENTS"
done
: >"$VERIFY_EVENTS"
if LONEJSON_RELEASE_TARGETS='' "$fixture/scripts/package-verify.sh" --binary >"$work/empty" 2>&1; then
  printf 'package verification accepted an empty explicit inventory\n' >&2; exit 1
fi
grep -F 'release target inventory is empty' "$work/empty" >/dev/null
[[ ! -s "$VERIFY_EVENTS" ]]
printf 'Custom full-scope and separate binary-manifest directories passed\n'
