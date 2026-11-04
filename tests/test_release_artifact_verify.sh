#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Rationale: checksum-listed release artifacts are the upload set and must fail
# on sanitizer markers, repository paths, and other non-releasable payloads.

repo_root=$1

tmp_dir="$(mktemp -d "$workspace_root/build/test_release_artifact_verify.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

write_checksums() {
  local dist_dir=$1
  local manifest=$2
  shift
  shift
  (cd "$dist_dir" && sha256sum "$@") >"$manifest"
}

dist_dir="$tmp_dir/good/dist"
mkdir -p "$dist_dir"
good_header="$dist_dir/lonejson-1.2.3.h.gz"
printf '%s\n' '#define LONEJSON_VERSION_MAJOR 1' | gzip -9 >"$good_header"
write_checksums "$dist_dir" "$dist_dir/lonejson-1.2.3-CHECKSUMS" \
  "$(basename "$good_header")"
"$repo_root/scripts/verify_release_artifacts.sh" \
  "$repo_root" "$dist_dir/lonejson-1.2.3-CHECKSUMS" >/dev/null
mkdir "$tmp_dir/manifests"
cp "$dist_dir/lonejson-1.2.3-CHECKSUMS" "$tmp_dir/manifests/binary-CHECKSUMS"
printf retained >"$dist_dir/unrelated.tar.gz"
"$repo_root/scripts/verify_release_artifacts.sh" "$repo_root" \
  "$tmp_dir/manifests/binary-CHECKSUMS" "$dist_dir" binary >/dev/null
[[ $(cat "$dist_dir/unrelated.tar.gz") == retained ]]
if "$repo_root/scripts/verify_release_artifacts.sh" "$repo_root" \
    "$tmp_dir/manifests/binary-CHECKSUMS" "$dist_dir" full >"$tmp_dir/unlisted" 2>&1; then
  printf 'full artifact verification accepted an unlisted release payload\n' >&2; exit 1
fi
grep -F 'not checksum-listed' "$tmp_dir/unlisted" >/dev/null

dist_dir="$tmp_dir/source-dist/dist"
source_root="$tmp_dir/source-dist/lonejson-1.2.3/cmake"
mkdir -p "$dist_dir" "$source_root"
printf '%s\n' 'set(LONEJSON_TOOLCHAIN_DIR "/toolchains/")' \
  >"$source_root/fetch_cpkt.cmake"
tar -czf "$dist_dir/lonejson-1.2.3.tar.gz" \
  -C "$tmp_dir/source-dist" lonejson-1.2.3
write_checksums "$dist_dir" "$dist_dir/lonejson-1.2.3-CHECKSUMS" \
  lonejson-1.2.3.tar.gz
"$repo_root/scripts/verify_release_artifacts.sh" \
  "$repo_root" "$dist_dir/lonejson-1.2.3-CHECKSUMS" >/dev/null

dist_dir="$tmp_dir/bad-sanitizer-dist/dist"
mkdir -p "$dist_dir"
bad_sanitizer_root="$tmp_dir/bad-sanitizer/liblonejson-1.2.3-x86_64-linux-gnu/lib"
mkdir -p "$bad_sanitizer_root"
printf '%s\n' '__asan_init' >"$bad_sanitizer_root/liblonejson.so"
tar -czf "$dist_dir/liblonejson-1.2.3-x86_64-linux-gnu.tar.gz" \
  -C "$tmp_dir/bad-sanitizer" liblonejson-1.2.3-x86_64-linux-gnu
write_checksums "$dist_dir" "$dist_dir/bad-sanitizer-CHECKSUMS" \
  liblonejson-1.2.3-x86_64-linux-gnu.tar.gz
if "$repo_root/scripts/verify_release_artifacts.sh" \
    "$repo_root" "$dist_dir/bad-sanitizer-CHECKSUMS" \
    >"$tmp_dir/bad-sanitizer.out" 2>"$tmp_dir/bad-sanitizer.err"; then
  printf 'expected sanitizer marker leak to fail release artifact verification\n' >&2
  exit 1
fi
grep -q 'sanitizer or fuzzer instrumentation marker leaked' \
  "$tmp_dir/bad-sanitizer.err"

dist_dir="$tmp_dir/bad-path-dist/dist"
mkdir -p "$dist_dir"
bad_path_rockspec="$dist_dir/lonejson-1.2.3-1.rockspec"
cat >"$bad_path_rockspec" <<EOF
package = "lonejson"
version = "1.2.3-1"
source = {
  url = "file://${repo_root}/dist/lonejson-1.2.3.tar.gz",
}
EOF
write_checksums "$dist_dir" "$dist_dir/bad-path-CHECKSUMS" \
  "$(basename "$bad_path_rockspec")"
if "$repo_root/scripts/verify_release_artifacts.sh" \
    "$repo_root" "$dist_dir/bad-path-CHECKSUMS" \
    >"$tmp_dir/bad-path.out" 2>"$tmp_dir/bad-path.err"; then
  printf 'expected repository path leak to fail release artifact verification\n' >&2
  exit 1
fi
grep -q 'repository path leaked' "$tmp_dir/bad-path.err"

dist_dir="$tmp_dir/bad-bootlin-dist/dist"
mkdir -p "$dist_dir"
bad_bootlin_root="$tmp_dir/bad-bootlin/liblonejson-1.2.3-x86_64-linux-gnu/lib"
mkdir -p "$bad_bootlin_root"
printf '%s\n' \
  '/srv/build-cache/cpkt/toolchains/roots/x86-64--glibc--stable-2026.08-1/sysroot/lib' \
  >"$bad_bootlin_root/liblonejson.so"
tar -czf "$dist_dir/liblonejson-1.2.3-x86_64-linux-gnu.tar.gz" \
  -C "$tmp_dir/bad-bootlin" liblonejson-1.2.3-x86_64-linux-gnu
write_checksums "$dist_dir" "$dist_dir/bad-bootlin-CHECKSUMS" \
  liblonejson-1.2.3-x86_64-linux-gnu.tar.gz
if "$repo_root/scripts/verify_release_artifacts.sh" \
    "$repo_root" "$dist_dir/bad-bootlin-CHECKSUMS" \
    >"$tmp_dir/bad-bootlin.out" 2>"$tmp_dir/bad-bootlin.err"; then
  printf 'expected pinned Bootlin path leak to fail release artifact verification\n' >&2
  exit 1
fi
grep -q 'pinned Bootlin toolchain path leaked' "$tmp_dir/bad-bootlin.err"

dist_dir="$tmp_dir/bad-custom-cache-dist/dist"
mkdir -p "$dist_dir"
bad_custom_cache_root="$tmp_dir/bad-custom-cache/liblonejson-1.2.3-x86_64-linux-gnu/lib/pkgconfig"
mkdir -p "$bad_custom_cache_root"
printf '%s\n' \
  'Libs.private: -L/srv/cpkt-cache/roots/x86-64--glibc--stable-2026.08-1/sysroot/lib' \
  >"$bad_custom_cache_root/lonejson.pc"
tar -czf "$dist_dir/liblonejson-1.2.3-x86_64-linux-gnu.tar.gz" \
  -C "$tmp_dir/bad-custom-cache" liblonejson-1.2.3-x86_64-linux-gnu
write_checksums "$dist_dir" "$dist_dir/bad-custom-cache-CHECKSUMS" \
  liblonejson-1.2.3-x86_64-linux-gnu.tar.gz
if "$repo_root/scripts/verify_release_artifacts.sh" \
    "$repo_root" "$dist_dir/bad-custom-cache-CHECKSUMS" \
    >"$tmp_dir/bad-custom-cache.out" 2>"$tmp_dir/bad-custom-cache.err"; then
  printf 'expected custom toolchain cache path leak to fail release artifact verification\n' >&2
  exit 1
fi
grep -q 'pinned Bootlin toolchain path leaked' "$tmp_dir/bad-custom-cache.err"

if [[ "$(uname -s)" == Linux ]]; then
  loader_dist_dir="$tmp_dir/bad-bootlin-loader-dist/dist"
  loader_root="$tmp_dir/bad-bootlin-loader/liblonejson-1.2.3-x86_64-linux-gnu"
  mkdir -p "$loader_dist_dir" "$loader_root/bin"
  cat >"$tmp_dir/release-probe.c" <<'EOF'
int main(void) {
  return 0;
}
EOF
  toolchain_env=$("$repo_root/scripts/cpkt-toolchains.sh" env x86_64-linux-gnu)
  eval "$toolchain_env"
  "$CC" "$tmp_dir/release-probe.c" \
    -Wl,--dynamic-linker,/srv/build-cache/cpkt/toolchains/roots/x86-64--glibc--stable-2026.08-1/sysroot/lib/ld-linux-x86-64.so.2 \
    -Wl,-rpath,/srv/build-cache/cpkt/toolchains/roots/x86-64--glibc--stable-2026.08-1/sysroot/lib \
    -o "$loader_root/bin/release-probe"
  tar -czf "$loader_dist_dir/liblonejson-1.2.3-x86_64-linux-gnu.tar.gz" \
    -C "$tmp_dir/bad-bootlin-loader" liblonejson-1.2.3-x86_64-linux-gnu
  write_checksums "$loader_dist_dir" "$loader_dist_dir/bad-bootlin-loader-CHECKSUMS" \
    liblonejson-1.2.3-x86_64-linux-gnu.tar.gz
  if "$repo_root/scripts/verify_release_artifacts.sh" \
      "$repo_root" "$loader_dist_dir/bad-bootlin-loader-CHECKSUMS" \
      >"$tmp_dir/bad-bootlin-loader.out" 2>"$tmp_dir/bad-bootlin-loader.err"; then
    printf 'expected Bootlin ELF loader metadata to fail release artifact verification\n' >&2
    exit 1
  fi
  grep -q 'pinned Bootlin ELF interpreter or RPATH leaked' \
    "$tmp_dir/bad-bootlin-loader.err"
fi

# Real Mach-O output includes the absolute inspected filename. That diagnostic
# is not loader metadata; actual non-relocatable load commands still fail.
if descriptor=$("$repo_root/scripts/cpkt-toolchains.sh" discover arm64-apple-darwin 2>/dev/null) &&
    grep -q '^status=ready$' <<<"$descriptor"; then
  cc=$(sed -n 's/^cc=//p' <<<"$descriptor")
  ld=$(sed -n 's/^ld=//p' <<<"$descriptor")
  darwin_dist="$tmp_dir/darwin/dist"
  darwin_root="$tmp_dir/darwin/liblonejson-1.2.3-arm64-apple-darwin"
  mkdir -p "$darwin_dist" "$darwin_root/bin"
  printf 'int main(void) { return 0; }\n' >"$tmp_dir/darwin-probe.c"
  for mode in good bad; do
    flags=()
    [[ "$mode" != bad ]] || flags=(-Wl,-rpath,/build/nonrelocatable)
    PATH="$(dirname -- "$ld"):$PATH" "$cc" --ld-path="$ld" \
      -Wall -Wextra -Werror -Wl,-fatal_warnings "${flags[@]}" \
      "$tmp_dir/darwin-probe.c" -o "$darwin_root/bin/probe"
    tar -czf "$darwin_dist/liblonejson-1.2.3-arm64-apple-darwin.tar.gz" \
      -C "$tmp_dir/darwin" liblonejson-1.2.3-arm64-apple-darwin
    write_checksums "$darwin_dist" "$darwin_dist/CHECKSUMS" \
      liblonejson-1.2.3-arm64-apple-darwin.tar.gz
    status=0
    "$repo_root/scripts/verify_release_artifacts.sh" "$repo_root" \
      "$darwin_dist/CHECKSUMS" >"$tmp_dir/darwin-$mode.log" 2>&1 || status=$?
    if [[ "$mode" == good ]]; then
      [[ "$status" == 0 ]] || { cat "$tmp_dir/darwin-$mode.log" >&2; exit 1; }
    else
      [[ "$status" != 0 ]]
      grep -q 'non-relocatable.*Mach-O loader metadata' "$tmp_dir/darwin-$mode.log"
    fi
  done
else
  echo 'SKIP: Mach-O loader regression requires installed osxcross'
fi

# The shipped Darwin smoke archive is a separate licensed artifact.
smoke_name=liblonejson-1.2.3-arm64-apple-darwin-smoke-test
mkdir -p "$tmp_dir/smoke/$smoke_name" "$tmp_dir/smoke/dist"
for mode in missing present; do
  if [[ "$mode" == present ]]; then
    cp "$repo_root/LICENSE" "$tmp_dir/smoke/$smoke_name/LICENSE"
  fi
  (cd "$tmp_dir/smoke" && zip -qr "dist/$smoke_name.zip" "$smoke_name")
  write_checksums "$tmp_dir/smoke/dist" "$tmp_dir/smoke/dist/CHECKSUMS" "$smoke_name.zip"
  status=0
  "$repo_root/scripts/verify_release_artifacts.sh" "$repo_root" \
    "$tmp_dir/smoke/dist/CHECKSUMS" >"$tmp_dir/smoke-$mode.log" 2>&1 || status=$?
  if [[ "$mode" == present ]]; then
    [[ "$status" == 0 ]] || { cat "$tmp_dir/smoke-$mode.log" >&2; exit 1; }
  else
    [[ "$status" != 0 ]]
    grep -q 'missing Darwin smoke bundle project license' "$tmp_dir/smoke-$mode.log"
  fi
 done
