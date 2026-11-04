#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?usage: check_curl_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
build_dir=${2:?usage: check_curl_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
target_id=${3:?usage: check_curl_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
shared_lib=${4:?usage: check_curl_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
static_lib=${5:?usage: check_curl_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
context=${6:-$target_id}

repo_root="$(CDPATH= cd -- "$repo_root" && pwd)"
case "$build_dir" in
  /*) ;;
  *) build_dir="$PWD/$build_dir" ;;
esac
if [[ -d "$build_dir" ]]; then
  build_dir="$(CDPATH= cd -- "$build_dir" && pwd)"
fi

if [[ ! -f "$shared_lib" ]]; then
  printf 'missing shared library for curl ABI check in %s: %s\n' "$context" "$shared_lib" >&2
  exit 1
fi
if [[ ! -f "$static_lib" ]]; then
  printf 'missing static library for curl ABI check in %s: %s\n' "$context" "$static_lib" >&2
  exit 1
fi

resolved_environment=$("$repo_root/scripts/discover_target_tools.sh" \
  --build-dir "$build_dir" \
  --target-id "$target_id") || exit $?
eval "$resolved_environment"

if [[ -z "${NM:-}" ]]; then
  printf 'missing target nm for %s\n' "$target_id" >&2
  exit 1
fi

if [[ "$target_id" == *apple-darwin ]]; then
  shared_symbols="$("$NM" -gU "$shared_lib")"
  static_symbols="$("$NM" -gU "$static_lib")"
  symbol_prefix_optional=1
else
  shared_symbols="$("$NM" -D --defined-only "$shared_lib")"
  static_symbols="$("$NM" -g --defined-only "$static_lib")"
  symbol_prefix_optional=0
fi

symbol_present() {
  local symbols=$1
  local symbol=$2
  local prefix_optional=$3
  awk -v symbol="$symbol" -v prefix_optional="$prefix_optional" '
    {
      actual = $NF
      if (actual == symbol || (prefix_optional == 1 && actual == "_" symbol)) {
        found = 1
      }
    }
    END { exit found ? 0 : 1 }
  ' <<<"$symbols"
}

for symbol in lonejson_curl_parse_init lonejson_curl_upload_is_rewindable \
    lonejson_curl_upload_rewind lonejson_curl_seek_callback; do
  if ! symbol_present "$shared_symbols" "$symbol" "$symbol_prefix_optional"; then
    printf 'missing %s ABI symbol in shared library for %s\n' "$symbol" "$context" >&2
    exit 1
  fi
  if ! symbol_present "$static_symbols" "$symbol" "$symbol_prefix_optional"; then
    printf 'missing %s ABI symbol in static library for %s\n' "$symbol" "$context" >&2
    exit 1
  fi
done
