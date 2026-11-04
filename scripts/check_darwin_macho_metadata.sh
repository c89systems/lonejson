#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?usage: check_darwin_macho_metadata.sh REPO_ROOT BUILD_DIR TARGET_ID MACHO_FILE [CONTEXT]}
build_dir=${2:?usage: check_darwin_macho_metadata.sh REPO_ROOT BUILD_DIR TARGET_ID MACHO_FILE [CONTEXT]}
target_id=${3:?usage: check_darwin_macho_metadata.sh REPO_ROOT BUILD_DIR TARGET_ID MACHO_FILE [CONTEXT]}
macho_file=${4:?usage: check_darwin_macho_metadata.sh REPO_ROOT BUILD_DIR TARGET_ID MACHO_FILE [CONTEXT]}
context=${5:-$macho_file}

repo_root="$(CDPATH= cd -- "$repo_root" && pwd)"
case "$build_dir" in
  /*) ;;
  *) build_dir="$PWD/$build_dir" ;;
esac
if [[ -d "$build_dir" ]]; then
  build_dir="$(CDPATH= cd -- "$build_dir" && pwd)"
fi

if [[ "$target_id" != *apple-darwin ]]; then
  printf 'Darwin Mach-O metadata check requires a Darwin target, got %s\n' "$target_id" >&2
  exit 1
fi
if [[ ! -f "$macho_file" ]]; then
  printf 'missing Mach-O file for Darwin metadata check in %s: %s\n' "$context" "$macho_file" >&2
  exit 1
fi

resolved_environment=$("$repo_root/scripts/discover_target_tools.sh" \
  --build-dir "$build_dir" \
  --target-id "$target_id") || exit $?
eval "$resolved_environment"

if [[ -z "${OTOOL:-}" ]]; then
  printf 'missing target otool for %s\n' "$target_id" >&2
  exit 1
fi

install_name="$("$OTOOL" -D "$macho_file" | sed -n '1d; /^[[:space:]]*$/d; s/^[[:space:]]*//; s/[[:space:]]*$//; p; q')"
if [[ ! "$install_name" =~ ^@rpath/liblonejson\.[0-9]+\.dylib$ ]]; then
  printf 'non-rpath Darwin install name in %s: %s\n' "$context" "${install_name:-<empty>}" >&2
  exit 1
fi

load_metadata="$("$OTOOL" -L "$macho_file")"
command_metadata="$("$OTOOL" -l "$macho_file")"
dependency_metadata="$(printf '%s\n' "$load_metadata" | sed '1d')"
combined_metadata="$dependency_metadata"

case "$combined_metadata" in
  *libcurl* | *cpkt* | *".cache/"* | *".deps/"* | *"$repo_root"* | *"/home/"* | *"/build/"* | *"/tmp/"* | *"/var/tmp/"*)
    printf 'forbidden Darwin loader metadata in %s\n' "$context" >&2
    exit 1
    ;;
esac

printf '%s\n' "$dependency_metadata" | sed 's/^[[:space:]]*//; s/[[:space:]].*$//' |
while IFS= read -r dependency; do
  [[ -n "$dependency" ]] || continue
  case "$dependency" in
    @rpath/liblonejson.*.dylib | /usr/lib/* | /System/Library/*)
      ;;
    /*)
      printf 'non-system absolute Darwin dependency in %s: %s\n' "$context" "$dependency" >&2
      exit 1
      ;;
    @loader_path/* | @executable_path/*)
      printf 'non-rpath project Darwin dependency in %s: %s\n' "$context" "$dependency" >&2
      exit 1
      ;;
    *)
      printf 'unexpected Darwin dependency in %s: %s\n' "$context" "$dependency" >&2
      exit 1
      ;;
  esac
done

rpath_paths="$(printf '%s\n' "$command_metadata" | sed -n 's/^[[:space:]]*path \([^ ]*\).*/\1/p')"
if [[ -n "$rpath_paths" ]] && printf '%s\n' "$rpath_paths" | grep -Ev '^@(loader_path|executable_path)(/|$)' >/dev/null; then
  printf 'non-loader-relative Darwin rpath in %s\n' "$context" >&2
  exit 1
fi
