#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

repo_root=${1:?usage: verify_release_artifacts.sh REPO_ROOT [CHECKSUMS]}
checksums=${2:-}
scope=${4:-full}
case "$scope" in full|binary) ;; *) printf 'unknown artifact scope: %s\n' "$scope" >&2; exit 2 ;; esac

repo_root="$(CDPATH= cd -- "$repo_root" && pwd)"
if [[ -z "$checksums" ]]; then
  version="$("$repo_root/scripts/release_version.sh")"
  checksums="$repo_root/dist/lonejson-$version-CHECKSUMS"
fi
if [[ ! -f "$checksums" ]]; then
  printf 'missing release checksum manifest: %s\n' "$checksums" >&2
  exit 1
fi

dist_dir="$(CDPATH= cd -- "${3:-$(dirname -- "$checksums")}" && pwd)"
checksums="$(CDPATH= cd -- "$(dirname -- "$checksums")" && pwd -P)/$(basename -- "$checksums")"
checksums_name=${checksums##*/}
tmp_dir="$(mktemp -d "$workspace_root/build/verify_release_artifacts.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'missing required command: %s\n' "$1" >&2
    exit 1
  fi
}

fail_artifact() {
  local artifact=$1
  local path=$2
  local reason=$3
  printf 'release artifact verification failed: %s: %s: %s\n' \
    "$artifact" "$path" "$reason" >&2
  exit 1
}

require_command sha256sum
require_command tar
require_command gzip
require_command grep
require_command strings
require_command file

(cd "$dist_dir" && sha256sum -c "$checksums" >/dev/null)

declare -a artifacts=()
while read -r _hash artifact; do
  [[ -n "${artifact:-}" ]] || continue
  case "$artifact" in
    /* | *".."* | *"/.."* | *"../"*)
      printf 'unsafe artifact path in checksum manifest: %s\n' "$artifact" >&2
      exit 1
      ;;
  esac
  if [[ ! -f "$dist_dir/$artifact" ]]; then
    printf 'checksum-listed artifact missing under dist: %s\n' "$artifact" >&2
    exit 1
  fi
  artifacts+=("$artifact")
done <"$checksums"

if [[ ${#artifacts[@]} -eq 0 ]]; then
  printf 'checksum manifest lists no release artifacts: %s\n' "$checksums" >&2
  exit 1
fi

if [[ "$scope" == full ]]; then
while IFS= read -r dist_file; do
  file_name="$(basename -- "$dist_file")"
  listed=0
  for artifact in "${artifacts[@]}"; do
    if [[ "$artifact" == "$file_name" ]]; then
      listed=1
      break
    fi
  done
  if [[ "$listed" -ne 1 ]]; then
    printf 'release-looking artifact under dist is not checksum-listed: %s\n' \
      "$file_name" >&2
    exit 1
  fi
done < <(find "$dist_dir" -maxdepth 1 -type f \
  \( -name '*.tar.gz' -o -name '*.tgz' -o -name '*.zip' -o -name '*.rock' \
     -o -name '*.src.rock' -o -name '*.rockspec' -o -name '*.h.gz' \) |
  sort)
fi

scan_text_for_local_paths() {
  local artifact=$1
  local path=$2
  local file_path=$3
  local home_path=${HOME:-}

  if grep -aF "$repo_root" "$file_path" >/dev/null 2>&1; then
    fail_artifact "$artifact" "$path" "repository path leaked"
  fi
  if [[ -n "$home_path" ]] && grep -aF "$home_path" "$file_path" >/dev/null 2>&1; then
    fail_artifact "$artifact" "$path" "home path leaked"
  fi
  if grep -aE 'file:///(home|tmp|var/tmp|private/tmp)/' \
      "$file_path" >/dev/null 2>&1; then
    fail_artifact "$artifact" "$path" "local file URL or build/cache path leaked"
  fi
}

scan_payload_for_local_paths() {
  local artifact=$1
  local root=$2

  while IFS= read -r file_path; do
    rel_path="${file_path#"$root"/}"
    scan_text_for_local_paths "$artifact" "$rel_path" "$file_path"
  done < <(find "$root" -type f | sort)
}

scan_shipped_payload_for_bootlin_toolchain_paths() {
  local artifact=$1
  local root=$2
  local toolchain_path_pattern

  # A development executable may record this collection's loader and RPATH,
  # but a distributed library or its package metadata must never name a local
  # Bootlin collection.  Match both the lifecycle cache layout and the stable
  # collection directory itself so a CPKT_TOOLCHAIN_CACHE override cannot hide
  # the leak behind a different parent directory.
  toolchain_path_pattern='/(cpkt/)?toolchains(/roots)?/|/[^[:space:]:;]*(x86-64|aarch64|armv7-eabihf)--(glibc|musl)--stable-[^[:space:]:;]*'

  while IFS= read -r file_path; do
    rel_path="${file_path#"$root"/}"
    case "$rel_path" in
      *liblonejson*.a | *liblonejson*.so* | *liblonejson*.dylib | \
      */lib/pkgconfig/*.pc | */lib/cmake/lonejson/*.cmake | \
      */share/lonejson/dependencies.json | *.rockspec | *.so | *.dylib)
        if grep -aE "$toolchain_path_pattern" "$file_path" >/dev/null 2>&1; then
          fail_artifact "$artifact" "$rel_path" \
            "pinned Bootlin toolchain path leaked"
        fi
        ;;
    esac
  done < <(find "$root" -type f | sort)
}

scan_shipped_payload_for_instrumentation() {
  local artifact=$1
  local root=$2

  while IFS= read -r file_path; do
    rel_path="${file_path#"$root"/}"
    case "$rel_path" in
      *.c | *.h | *.lua | *.md | *.txt | */RELEASE_MANIFEST | */VERSION)
        continue
        ;;
    esac
    case "$rel_path" in
      *liblonejson*.a | *liblonejson*.so* | *liblonejson*.dylib | \
      *.rockspec | *.h | *.pc | *.cmake | *.so | *.dylib)
        if strings "$file_path" | grep -E \
          '(__asan|__tsan|libasan|libtsan|AddressSanitizer|ThreadSanitizer|-fsanitize)' \
          >/dev/null; then
          fail_artifact "$artifact" "$rel_path" \
            "sanitizer or fuzzer instrumentation marker leaked"
        fi
        ;;
    esac
  done < <(find "$root" -type f | sort)
}

scan_loader_metadata() {
  local artifact=$1
  local root=$2
  local bootlin_loader_path_pattern
  local tools_loaded=0 target_id candidate
  local READELF OTOOL
  load_loader_tools() {
    [[ "$tools_loaded" == 0 ]] || return 0
    target_id=$("$repo_root/scripts/detect_native_target.sh")
    for candidate in x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl armhf-linux-gnu armhf-linux-musl arm64-apple-darwin; do
      if [[ "$artifact" == *"-$candidate"* ]]; then target_id=$candidate; break; fi
    done
    local description
    description=$("$repo_root/scripts/discover_target_tools.sh" \
      --build-dir "$repo_root/build/$target_id-release" --target-id "$target_id") || exit 1
    eval "$description"
    tools_loaded=1
  }

  # Keep this separate from the general path scan: ELF interpreter and
  # RPATH metadata must never point at a pinned Bootlin collection.
  bootlin_loader_path_pattern='cpkt/toolchains/|/(x86-64|aarch64|armv7-eabihf)--(glibc|musl)--stable-'

  while IFS= read -r file_path; do
    rel_path="${file_path#"$root"/}"
    description="$(file -b "$file_path")"
    case "$description" in
      *ELF*shared\ object* | *ELF*executable*)
        load_loader_tools
        if [[ -z "$READELF" ]]; then fail_artifact "$artifact" "$rel_path" "missing target readelf"; fi
        metadata="$("$READELF" -l -d "$file_path")"
          if printf '%s\n' "$metadata" |
              grep -E '(Requesting program interpreter|RUNPATH|RPATH)' |
              grep -E "$bootlin_loader_path_pattern" >/dev/null; then
            fail_artifact "$artifact" "$rel_path" \
              "pinned Bootlin ELF interpreter or RPATH leaked"
          fi
          if printf '%s\n' "$metadata" | grep -E \
              '(libasan|libtsan|RUNPATH|RPATH|Requesting program interpreter).*(/home/|/tmp/|/var/tmp|/build/|/\.cache/|/\.deps/|cpkt/toolchains/|--(glibc|musl)--stable-)' \
              >/dev/null; then
            fail_artifact "$artifact" "$rel_path" \
              "non-relocatable, pinned Bootlin, or sanitizer ELF loader metadata"
          fi
        ;;
      *Mach-O* | *Mach-O\ 64-bit*)
        load_loader_tools
        if [[ -z "$OTOOL" ]]; then fail_artifact "$artifact" "$rel_path" "missing target otool"; fi
        # otool prints the inspected filename, including our extraction
        # workspace. Scan load-command values rather than that tool header.
        metadata="$("$OTOOL" -l "$file_path" | awk '
          $1 == "name" || $1 == "path" { print }
        ')"
          if printf '%s\n' "$metadata" | grep -E \
              '(libasan|libtsan|/home/|/tmp/|/var/tmp|/build/|/\.cache/|/\.deps/|cpkt/toolchains/|--(glibc|musl)--stable-)' \
              >/dev/null; then
            fail_artifact "$artifact" "$rel_path" \
              "non-relocatable, pinned Bootlin, or sanitizer Mach-O loader metadata"
          fi
        ;;
    esac
  done < <(find "$root" -type f | sort)
}

extract_artifact() {
  local artifact=$1
  local source_path="$dist_dir/$artifact"
  local artifact_root="$tmp_dir/extract/$artifact"

  mkdir -p "$artifact_root"
  case "$artifact" in
    *.tar.gz | *.tgz)
      tar -xzf "$source_path" -C "$artifact_root"
      ;;
    *.src.rock | *.rock | *.zip)
      require_command unzip
      unzip -q "$source_path" -d "$artifact_root"
      ;;
    *.h.gz)
      gzip -cd "$source_path" >"$artifact_root/${artifact%.gz}"
      ;;
    *)
      cp "$source_path" "$artifact_root/"
      ;;
  esac

  while IFS= read -r nested; do
    nested_root="$nested.expanded"
    mkdir -p "$nested_root"
    case "$nested" in
      *.tar.gz | *.tgz)
        tar -xzf "$nested" -C "$nested_root"
        ;;
      *.src.rock | *.rock | *.zip)
        require_command unzip
        unzip -q "$nested" -d "$nested_root"
        ;;
      *.gz)
        gzip -cd "$nested" >"$nested_root/$(basename "${nested%.gz}")"
        ;;
    esac
  done < <(find "$artifact_root" -type f \
    \( -name '*.tar.gz' -o -name '*.tgz' -o -name '*.zip' -o -name '*.rock' \
       -o -name '*.src.rock' -o -name '*.h.gz' \) |
    sort)

  if [[ "$artifact" == *-arm64-apple-darwin-smoke-test.zip &&
        ! -s "$artifact_root/${artifact%.zip}/LICENSE" ]]; then
    fail_artifact "$artifact" LICENSE "missing Darwin smoke bundle project license"
  fi
  scan_payload_for_local_paths "$artifact" "$artifact_root"
  scan_shipped_payload_for_bootlin_toolchain_paths "$artifact" "$artifact_root"
  case "$artifact" in
    lonejson-[0-9]*.tar.gz | lonejson-[0-9]*.tgz)
      ;;
    *)
      scan_shipped_payload_for_instrumentation "$artifact" "$artifact_root"
      ;;
  esac
  scan_loader_metadata "$artifact" "$artifact_root"
}

for artifact in "${artifacts[@]}"; do
  scan_text_for_local_paths "$checksums_name" "$artifact" "$dist_dir/$artifact"
  extract_artifact "$artifact"
done

printf 'release artifact verification passed: %s\n' "$checksums"
