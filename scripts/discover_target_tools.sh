#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
build_dir=
target_id=

usage() {
  printf 'usage: %s --build-dir DIR --target-id TARGET\n' "$0" >&2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      if [[ $# -lt 2 ]]; then
        usage
        exit 1
      fi
      build_dir=$2
      shift 2
      ;;
    --target-id)
      if [[ $# -lt 2 ]]; then
        usage
        exit 1
      fi
      target_id=$2
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage
      exit 1
      ;;
  esac
done

if [[ -z "$build_dir" || -z "$target_id" ]]; then
  usage
  exit 1
fi

cache_file="$build_dir/CMakeCache.txt"

cache_value() {
  local name=$1
  if [[ -f "$cache_file" ]]; then
    sed -n "s/^${name}:[^=]*=//p" "$cache_file" | tail -n 1
  fi
}

target_default_compiler() {
  case "$target_id" in
    arm64-apple-darwin)
      "$repo_root/scripts/cpkt-toolchains.sh" discover "$target_id" |
        sed -n 's/^cc=//p'
      ;;
    *)
      printf 'unknown target id: %s\n' "$target_id" >&2
      exit 1
      ;;
  esac
}

first_executable() {
  local candidate
  for candidate in "$@"; do
    [[ -n "$candidate" ]] || continue
    if [[ "$candidate" == */* ]]; then
      if [[ -x "$candidate" ]]; then
        printf '%s\n' "$candidate"
        return 0
      fi
    elif command -v "$candidate" >/dev/null 2>&1; then
      command -v "$candidate"
      return 0
    fi
  done
  return 1
}

tool_prefix() {
  case "$target_id" in
    arm64-apple-darwin)
      local compiler_name=${cc##*/}
      case "$compiler_name" in
        *-clang|*-cc) printf '%s-\n' "${compiler_name%-*}" ;;
        *) printf '%s\n' "" ;;
      esac
      ;;
  esac
}

target_host_prefix() {
  local prefix_value
  prefix_value="$(tool_prefix)"
  printf '%s\n' "${prefix_value%-}"
}

# Linux verification must stay within the complete pinned collection. A
# configured host compiler or binutils override is an error, never a fallback.
case "$target_id" in
  *linux*)
    description=$("$repo_root/scripts/cpkt-toolchains.sh" discover "$target_id")
    grep -Fx 'status=ready' <<<"$description" >/dev/null || {
      printf 'selected toolchain unavailable for %s; run scripts/cpkt-toolchains.sh ensure %s\n' "$target_id" "$target_id" >&2
      exit 1
    }
    resolved_value() { printf '%s\n' "$description" | sed -n "s/^$1=//p"; }
    selected_tool() {
      local key=$1 cache_key=$2 override=$3 selected configured
      selected=$(resolved_value "$key")
      configured=$(cache_value "$cache_key")
      for candidate in "$configured" "$override"; do
        [[ -z "$candidate" || "$candidate" == "$selected" ]] || {
          printf 'tool outside selected Bootlin collection: %s=%s (expected %s)\n' "$cache_key" "$candidate" "$selected" >&2
          exit 1
        }
      done
      [[ -x "$selected" ]] || { printf 'selected tool unavailable: %s\n' "$selected" >&2; exit 1; }
      printf '%s\n' "$selected"
    }
    linux_assignment() { printf '%s=%q\n' "$1" "$2"; }
    cc=$(selected_tool cc CMAKE_C_COMPILER '')
    linker=$(selected_tool ld CMAKE_LINKER "${LONEJSON_LINKER:-}")
    ar=$(selected_tool ar CMAKE_AR "${LONEJSON_AR:-}")
    strip=$(selected_tool strip CMAKE_STRIP "${LONEJSON_STRIP:-}")
    nm=$(selected_tool nm CMAKE_NM "${LONEJSON_NM:-}")
    readelf=$(selected_tool readelf CMAKE_READELF "${LONEJSON_READELF:-}")
    prefix=$(resolved_value prefix)
    linux_assignment TARGET_ID "$target_id"
    linux_assignment TARGET_HOST_PREFIX "$prefix"
    linux_assignment TARGET_TOOL_PREFIX "$prefix-"
    linux_assignment CC "$cc"
    linux_assignment LINKER "$linker"
    linux_assignment AR "$ar"
    linux_assignment STRIP "$strip"
    linux_assignment NM "$nm"
    linux_assignment READELF "$readelf"
    linux_assignment OTOOL ''
    linux_assignment INSTALL_NAME_TOOL ''
    linux_assignment TARGET_CFLAGS ''
    exit 0
    ;;
esac

cc="$(cache_value CMAKE_C_COMPILER)"
if [[ -z "$cc" ]]; then
  cc="$(target_default_compiler)"
fi
cc="$(first_executable "$cc" || true)"
if [[ -z "$cc" ]]; then
  printf 'target compiler unavailable for %s\n' "$target_id" >&2
  exit 1
fi

cc_dir=
if [[ "$cc" == */* ]]; then
  cc_dir="$(CDPATH= cd -- "$(dirname -- "$cc")" && pwd)"
fi
prefix="$(tool_prefix)"
host_prefix="$(target_host_prefix)"

configured_strip="$(cache_value CMAKE_STRIP)"
configured_linker="$(cache_value CMAKE_LINKER)"
configured_ar="$(cache_value CMAKE_AR)"
configured_nm="$(cache_value CMAKE_NM)"
configured_install_name_tool="$(cache_value CMAKE_INSTALL_NAME_TOOL)"
configured_otool="$(cache_value CMAKE_OTOOL)"
configured_readelf="$(cache_value CMAKE_READELF)"
if [[ -z "$configured_otool" ]]; then
  configured_otool="${CPKT_OTOOL:-}"
fi

strip_tool="$(first_executable \
  "${LONEJSON_STRIP:-}" \
  "$configured_strip" \
  "${cc_dir:+$cc_dir/${prefix}strip}" \
  "${cc_dir:+$cc_dir/strip}" \
  "${prefix}strip" || true)"

linker_tool="$(first_executable \
  "${LONEJSON_LINKER:-}" \
  "$configured_linker" \
  "${cc_dir:+$cc_dir/${prefix}ld}" \
  "${cc_dir:+$cc_dir/ld}" \
  "${prefix}ld" || true)"

ar_tool="$(first_executable \
  "${LONEJSON_AR:-}" \
  "$configured_ar" \
  "${cc_dir:+$cc_dir/${prefix}ar}" \
  "${cc_dir:+$cc_dir/ar}" \
  "${prefix}ar" || true)"

nm_tool="$(first_executable \
  "${LONEJSON_NM:-}" \
  "$configured_nm" \
  "${cc_dir:+$cc_dir/${prefix}nm}" \
  "${cc_dir:+$cc_dir/nm}" \
  "${prefix}nm" || true)"

readelf_tool=

otool_tool=
install_name_tool=
if [[ "$target_id" == *apple-darwin ]]; then
  otool_tool="$(first_executable \
    "${LONEJSON_OTOOL:-}" \
    "$configured_otool" \
    "${cc_dir:+$cc_dir/${prefix}otool}" \
    "${cc_dir:+$cc_dir/otool}" \
    "${prefix}otool" || true)"
  install_name_tool="$(first_executable \
    "${LONEJSON_INSTALL_NAME_TOOL:-}" \
    "$configured_install_name_tool" \
    "${cc_dir:+$cc_dir/${prefix}install_name_tool}" \
    "${cc_dir:+$cc_dir/install_name_tool}" \
    "${prefix}install_name_tool" || true)"
fi

quote_assignment() {
  local key=$1
  local value=$2
  printf '%s=%q\n' "$key" "$value"
}

quote_assignment TARGET_ID "$target_id"
quote_assignment TARGET_HOST_PREFIX "$host_prefix"
quote_assignment TARGET_TOOL_PREFIX "$prefix"
quote_assignment CC "$cc"
quote_assignment LINKER "$linker_tool"
quote_assignment AR "$ar_tool"
quote_assignment STRIP "$strip_tool"
quote_assignment NM "$nm_tool"
quote_assignment READELF "$readelf_tool"
quote_assignment OTOOL "$otool_tool"
quote_assignment INSTALL_NAME_TOOL "$install_name_tool"

quote_assignment TARGET_CFLAGS ''
