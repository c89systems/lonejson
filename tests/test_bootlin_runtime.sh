#!/usr/bin/env bash
set -euo pipefail

build_dir=$1
executable=$2
run_mode=${3:-}
cache_file="$build_dir/CMakeCache.txt"

cache_value() {
  sed -n "s/^$2:[^=]*=//p" "$1" | tail -n 1
}

readelf_bin="$(cache_value "$cache_file" CMAKE_READELF)"
sysroot="$(cache_value "$cache_file" CMAKE_SYSROOT)"
libc="$(cache_value "$cache_file" LONEJSON_BOOTLIN_LIBC)"
bootlin_root="$(cache_value "$cache_file" LONEJSON_BOOTLIN_TOOLCHAIN_ROOT)"
if [[ -z "$readelf_bin" || -z "$sysroot" || -z "$libc" ||
      -z "$bootlin_root" ]]; then
  printf 'missing Bootlin runtime metadata in %s\n' "$cache_file" >&2
  exit 1
fi

case "$libc" in
  gnu) loader_candidates=("$sysroot"/lib/ld-linux*.so.2) ;;
  musl) loader_candidates=("$sysroot"/lib/ld-musl-*.so.1) ;;
  *) printf 'unsupported Bootlin libc %s\n' "$libc" >&2; exit 1 ;;
esac
if [[ ${#loader_candidates[@]} -ne 1 || ! -x "${loader_candidates[0]}" ]]; then
  printf 'expected one executable Bootlin loader under %s/lib\n' "$sysroot" >&2
  exit 1
fi
loader=$(realpath -e "${loader_candidates[0]}")

interpreter="$($readelf_bin -l "$executable" | sed -n 's/.*Requesting program interpreter: \(.*\)].*/\1/p')"
if [[ "$interpreter" != "$loader" ]]; then
  printf 'wrong ELF interpreter for %s: expected %s, got %s\n' \
    "$executable" "$loader" "$interpreter" >&2
  exit 1
fi

dynamic="$($readelf_bin -d "$executable")"
if grep -F '(RUNPATH)' <<<"$dynamic" >/dev/null; then
  printf 'development executable uses non-transitive RUNPATH: %s\n' "$executable" >&2
  exit 1
fi
if ! grep -F '(RPATH)' <<<"$dynamic" | grep -F "$sysroot/lib" >/dev/null; then
  printf 'development executable lacks Bootlin RPATH: %s\n' "$executable" >&2
  exit 1
fi

if [[ ${executable##*/} == lonejson_bench ]]; then
  rpath=$(sed -n 's/.*(RPATH).*\[\(.*\)\].*/\1/p' <<<"$dynamic")
  IFS=: read -r -a runtime_dirs <<<"$rpath"
  for runtime_dir in "${runtime_dirs[@]}"; do
    case "$runtime_dir" in
      "$bootlin_root"/*) ;;
      *)
        printf 'standalone benchmark embeds a checkout-dependent runtime path: %s\n' \
          "$runtime_dir" >&2
        exit 1 ;;
    esac
  done
fi

resolution="$($loader --list "$executable")"
if ! grep -F "$sysroot/lib/libc.so.6" <<<"$resolution" >/dev/null; then
  printf 'development executable resolved libc outside Bootlin sysroot: %s\n' \
    "$executable" >&2
  printf '%s\n' "$resolution" >&2
  exit 1
fi
while IFS= read -r resolved_path; do
  case "$resolved_path" in
    "$sysroot"/*|"$bootlin_root"/*) ;;
    /lib/*|/usr/lib/*)
      printf 'development executable resolved a host library: %s\n' \
        "$executable" >&2
      printf '%s\n' "$resolution" >&2
      exit 1
      ;;
  esac
done < <(sed -n 's/.* => \(\/[^[:space:]]*\).*/\1/p' <<<"$resolution")

if [[ "$run_mode" == --run ]]; then
  "$executable"
fi
