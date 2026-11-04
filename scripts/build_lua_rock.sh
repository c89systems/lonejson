#!/usr/bin/env bash

set -eu

if [ "$#" -ne 7 ]; then
  printf 'usage: %s CC CFLAGS LIBFLAG OBJ_EXTENSION LIB_EXTENSION LUA_INCDIR LONEJSON_LIBDIR\n' "$0" >&2
  exit 1
fi

cc="$1"
cflags="$2"
libflag="$3"
obj_ext="$4"
lib_ext="$5"
lua_incdir="$6"
lonejson_libdir="${LONEJSON_LIBDIR:-$7}"

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
native_system=$(uname -s)
if [ "$native_system" = "Linux" ] || [ "$native_system" = "Darwin" ]; then
  if [ "$native_system" = "Darwin" ]; then
    target_id=arm64-apple-darwin
  else
    target_id=$("$repo_root/scripts/detect_native_bootlin_target.sh")
  fi
  toolchain=$("$repo_root/scripts/cpkt-toolchains.sh" ensure "$target_id")
  cc=$(printf '%s\n' "$toolchain" | sed -n 's/^cc=//p')
  if [ ! -x "$cc" ]; then
    printf 'Native toolchain resolver did not provide an executable compiler\n' >&2
    exit 1
  fi
  if [ "$native_system" = "Darwin" ]; then
    apple_sdk=$(printf '%s\n' "$toolchain" | sed -n 's/^sysroot=//p')
  fi
fi
lonejson_incdir="${lonejson_libdir%/}/../include"
if [ -z "${LONEJSON_LIBDIR:-}" ] &&
   command -v pkg-config >/dev/null 2>&1 && pkg-config --exists lonejson; then
  lonejson_libdir=$(pkg-config --variable=libdir lonejson)
  lonejson_incdir=$(pkg-config --variable=includedir lonejson)
fi
if [ ! -f "$lonejson_incdir/lonejson.h" ]; then
  printf 'missing installed public lonejson.h beside %s; run make lua-rock or set LONEJSON_LIBDIR to an installed SDK lib directory\n' "$lonejson_libdir" >&2
  exit 1
fi
build_root="${repo_root}/build/luarocks-native"
module_dir="${build_root}/lonejson"
object_path="${build_root}/lonejson_lua.${obj_ext}"
module_path="${module_dir}/core.${lib_ext}"
probe_dir="${build_root}/probe"
probe_c="${probe_dir}/curl_probe.c"
probe_bin="${probe_dir}/curl_probe"

if [ -z "${cc}" ]; then
  printf 'compiler command is empty\n' >&2
  exit 1
fi

run_cc() {
  if [ "$native_system" = "Darwin" ]; then
    "$cc" -arch arm64 -isysroot "$apple_sdk" "$@"
    return "$?"
  fi
  if [ -x "${cc}" ]; then
    "${cc}" "$@"
    return "$?"
  fi
  CC_LONEJSON="${cc}" sh -c '
    eval "set -- ${CC_LONEJSON} \"\$@\""
    exec "$@"
  ' sh "$@"
}

mkdir -p "${module_dir}" "${probe_dir}"
rm -f "${object_path}" "${module_path}" "${probe_bin}"

if [ "$native_system" = "Linux" ]; then
  "$repo_root/scripts/deps.sh" "$target_id" >/dev/null
  bundle_root="$repo_root/.cache/cpkt/$target_id/root"
  lua_incdir="$bundle_root/include"
fi

common_cflags="${cflags} -Wall -Wextra -Werror"
export_policy="${build_root}/exports"
if [ "$native_system" = "Darwin" ]; then
  sed 's/^/_/' "$repo_root/cmake/lonejson_lua.exports" >"$export_policy"
  warning_linkflag=-Wl,-fatal_warnings
  linkflags="${LDFLAGS:-} -Wl,-fatal_warnings -Wl,-exported_symbols_list,$export_policy"
else
  { printf '{ global:\n'; sed 's/$/;/' "$repo_root/cmake/lonejson_lua.exports"; printf 'local: *; };\n'; } >"$export_policy"
  warning_linkflag=-Wl,--fatal-warnings
  linkflags="${LDFLAGS:-} -Wl,--fatal-warnings -Wl,--version-script,$export_policy"
fi
if [ "$(uname -s)" = "Linux" ]; then
  linkflags="${linkflags} -Wl,--allow-shlib-undefined"
fi

curl_cflags=""
curl_libs=""
if [ "$native_system" = "Linux" ]; then
  curl_cflags=""
  curl_libs=""
elif command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libcurl; then
  curl_cflags="$(pkg-config --cflags libcurl)"
  curl_libs="$(pkg-config --libs libcurl)"
elif command -v curl-config >/dev/null 2>&1; then
  curl_cflags="$(curl-config --cflags)"
  curl_libs="$(curl-config --libs)"
else
  curl_libs="-lcurl"
fi

cat >"${probe_c}" <<'EOF'
#define LONEJSON_WITH_CURL 1
#include "lonejson.h"
int main(void) {
  curl_off_t size = 0;
  return (int)size;
}
EOF

if [ "$native_system" = "Linux" ]; then
  set -- "$bundle_root/lib/libcurl.so"
else
  set -- ${curl_libs}
fi
use_curl=0
if [ -z "${LONEJSON_LUA_DISABLE_CURL:-}" ]; then
  if run_cc ${common_cflags} -I"${lonejson_incdir}" -I"${lua_incdir}" ${curl_cflags} "${probe_c}" "$warning_linkflag" -o "${probe_bin}" "$@" >/dev/null 2>&1; then
    use_curl=1
  elif [ -n "${LONEJSON_LUA_FORCE_CURL:-}" ]; then
    printf 'curl probe failed while LONEJSON_LUA_FORCE_CURL is set\n' >&2
    exit 1
  fi
fi

cppflags=""
lonejson_shared="$lonejson_libdir/liblonejson.so"
if [ "$native_system" = "Darwin" ]; then lonejson_shared="$lonejson_libdir/liblonejson.dylib"; fi
if [ ! -f "$lonejson_shared" ]; then
  printf 'missing public shared lonejson SDK library: %s\n' "$lonejson_shared" >&2
  exit 1
fi
if [ "${use_curl}" -eq 1 ]; then
  cppflags="-DLONEJSON_WITH_CURL ${curl_cflags}"
  printf 'Lua rock build: enabling curl support\n'
else
  set --
  printf 'Lua rock build: building without curl support\n'
fi

run_cc ${common_cflags} -I"${lonejson_incdir}" -I"${lua_incdir}" ${cppflags} -c "${repo_root}/src/lua/lonejson_lua.c" -o "${object_path}"
run_cc ${libflag} -o "${module_path}" "${object_path}" ${linkflags} "${lonejson_shared}" "$@"

if [ "$native_system" = "Linux" ] || [ "$native_system" = "Darwin" ]; then
  nm_tool=$(printf '%s\n' "$toolchain" | sed -n 's/^nm=//p')
  bash "$repo_root/scripts/check_library_exports.sh" "$nm_tool" "$native_system" \
    "$module_path" "$repo_root/cmake/lonejson_lua.exports"
fi
