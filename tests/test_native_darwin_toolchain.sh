#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"
repo_root=$1
tmp_dir=$(mktemp -d "$workspace_root/build/test_native_darwin_toolchain.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT
fixture="$tmp_dir/repo"
mkdir -p "$tmp_dir/lib" "$tmp_dir/include" "$fixture/scripts" "$fixture/cmake/toolchains" "$tmp_dir/bin" "$tmp_dir/SDK"
for name in cpkt-toolchains cpkt-native-apple require_cache_path build_lua_rock detect_cpkt_bundle detect_native_target; do
  cp "$repo_root/scripts/$name.sh" "$fixture/scripts/"
done
mkdir -p "$fixture/tools/lifecycle/scripts"
cp "$repo_root/tools/lifecycle/scripts/"*.sh "$fixture/tools/lifecycle/scripts/"
cp "$repo_root/cmake/toolchains/"{native,lonejson_native_darwin,arm64-apple-darwin}.cmake "$fixture/cmake/toolchains/"
cp "$repo_root/cmake/LonejsonLifecycle.cmake" "$repo_root/cmake/LonejsonCompiler.cmake" "$repo_root/cmake/lonejson_lua.exports" "$repo_root/cmake/lonejson.exports" "$fixture/cmake/"
cp "$repo_root/include/lonejson.h" "$tmp_dir/include/"
: >"$tmp_dir/lib/liblonejson.dylib"
cp "$repo_root/scripts/check_library_exports.sh" "$fixture/scripts/"
cat >"$tmp_dir/bin/uname" <<'EOF'
#!/usr/bin/env bash
case "$1" in
  -s) printf '%s\n' "${TEST_SYSTEM:-Darwin}" ;;
  -m) printf '%s\n' "${TEST_ARCH:-arm64}" ;;
esac
EOF
cat >"$tmp_dir/bin/xcrun" <<'EOF'
#!/usr/bin/env bash
[[ "${FAIL_XCRUN:-0}" == 0 ]] || exit 1
[[ "$1" == --sdk && "$2" == macosx ]]
case "$3" in
  --show-sdk-path) printf '%s\n' "$TEST_ROOT/SDK" ;;
  --find) printf '%s/bin/%s\n' "$TEST_ROOT" "$4" ;;
  *) exit 1 ;;
esac
EOF
cat >"$tmp_dir/bin/clang" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "${0##*/} $*" >>"$TEST_ROOT/compiler.log"
case "$*" in
  *pkce_provider_no_openssl.c*)
    while [[ $# -gt 0 ]]; do
      if [[ "$1" == -o ]]; then
        printf '#!/usr/bin/env bash\nprintf "PKCE executed\\n" >>"$TEST_ROOT/compiler.log"\n' >"$2"
        chmod +x "$2"
        break
      fi
      shift
    done
    ;;
esac
EOF
for tool in clang++ ld ar ranlib strip nm otool install_name_tool; do
  cp "$tmp_dir/bin/clang" "$tmp_dir/bin/$tool"
done
cat >"$tmp_dir/bin/nm" <<'EOF_NM'
#!/usr/bin/env bash
[[ "$1" != -gu ]] || exit 0
while read -r symbol; do printf '_%s T 0 1\n' "$symbol"; done <"$TEST_ROOT/repo/cmake/lonejson_lua.exports"
EOF_NM
cat >"$tmp_dir/bin/pkg-config" <<'EOF'
#!/usr/bin/env bash
case "$*" in
  '--exists lonejson') [[ "${TEST_PKG_CORE:-0}" == 1 ]] ;;
  '--variable=libdir lonejson') printf '%s/lib\n' "$TEST_ROOT" ;;
  '--variable=includedir lonejson') printf '%s/include\n' "$TEST_ROOT" ;;
  *--cflags*) printf '%s\n' '-DTEST_CURL' ;;
  *--libs*) printf '%s\n' '-lcurl' ;;
esac
EOF
chmod +x "$tmp_dir/bin/"*
export TEST_ROOT="$tmp_dir" UNAME="$tmp_dir/bin/uname"
export PATH="$tmp_dir/bin:$PATH" OSXCROSS_ROOT="$tmp_dir/missing-osxcross" CC=/bin/false
export CPKT_OSXCROSS_HOST=arm64-apple-darwin25 PKG_CONFIG="$tmp_dir/bin/pkg-config"
export CPKT_TOOLCHAIN_CACHE="$fixture/build/toolchain-cache"
unset LONEJSON_CPKT_TARGET_ID CPKT_SYSROOT
description=$("$fixture/scripts/cpkt-toolchains.sh" ensure arm64-apple-darwin)
grep -Fx 'source=apple' <<<"$description"
grep -Fx "cc=$tmp_dir/bin/clang" <<<"$description"
grep -Fx "sysroot=$tmp_dir/SDK" <<<"$description"
environment=$("$fixture/scripts/cpkt-toolchains.sh" env arm64-apple-darwin)
grep -Fx 'export CPKT_TARGET=arm64-apple-darwin' <<<"$environment"
grep -Fx 'export CPKT_TOOLCHAIN_SOURCE=apple' <<<"$environment"
grep -Fx "export CC=$tmp_dir/bin/clang" <<<"$environment"
for target in x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu \
    aarch64-linux-musl armhf-linux-gnu armhf-linux-musl unsupported-target; do
  if "$fixture/scripts/cpkt-toolchains.sh" env "$target" >"$tmp_dir/env" 2>"$tmp_dir/error"; then
    echo "Apple tools accepted for unavailable target: $target" >&2; exit 1
  fi
  [[ ! -s "$tmp_dir/env" ]]
  if [[ "$target" == unsupported-target ]]; then
    grep -F 'unsupported target' "$tmp_dir/error" >/dev/null
  else
    grep -F 'target is missing' "$tmp_dir/error" >/dev/null
  fi
done
[[ "$("$fixture/scripts/detect_native_target.sh")" == arm64-apple-darwin ]]
# Curl examples use CMake targets, so the native Darwin toolchain is selected
# by the same entry point that configures every other development executable.
grep -F 'cmake --preset host-curl' "$repo_root/scripts/build_curl_examples.sh" >/dev/null
grep -F 'example_curl_get example_curl_put' "$repo_root/scripts/build_curl_examples.sh" >/dev/null
# LuaRocks must likewise use the native compiler even if its CC is overridden.
sh "$fixture/scripts/build_lua_rock.sh" /bin/false '-O2 -fPIC' -shared o so "$tmp_dir/headers" "$tmp_dir/lib"
[[ $(wc -l <"$tmp_dir/compiler.log") -eq 3 ]]
grep -F 'clang -arch arm64 -isysroot' "$tmp_dir/compiler.log" >/dev/null

# With no explicit prefix, pkg-config discovers the installed public SDK.
TEST_PKG_CORE=1 sh "$fixture/scripts/build_lua_rock.sh" /bin/false '-O2 -fPIC' -shared o so "$tmp_dir/headers" "$tmp_dir/missing-lib"
grep -F -- "-I$tmp_dir/include" "$tmp_dir/compiler.log" >/dev/null
grep -F -- '-Wl,-fatal_warnings' "$tmp_dir/compiler.log" >/dev/null

# The PKCE fixture compiles and executes through native Apple discovery.
: >"$tmp_dir/compiler.log"
bash "$repo_root/tests/test_oidc_pkce_provider_no_openssl.sh" "$fixture"
grep -F "clang -arch arm64 -isysroot $tmp_dir/SDK -std=c89" "$tmp_dir/compiler.log" >/dev/null
grep -Fx 'PKCE executed' "$tmp_dir/compiler.log"
if FAIL_XCRUN=1 bash "$repo_root/tests/test_oidc_pkce_provider_no_openssl.sh" "$fixture" >"$tmp_dir/error" 2>&1; then exit 1; fi
grep -F 'install Xcode or Command Line Tools' "$tmp_dir/error"

# Every CMake entry point uses the Apple tools without marking the build cross.
cat >"$tmp_dir/check.cmake" <<'EOF'
set(CMAKE_HOST_SYSTEM_NAME Darwin)
include("${ENTRY}")
if(NOT CMAKE_C_COMPILER STREQUAL "$ENV{TEST_ROOT}/bin/clang" OR
    NOT CMAKE_CXX_COMPILER STREQUAL "$ENV{TEST_ROOT}/bin/clang++" OR
    NOT CMAKE_OSX_SYSROOT STREQUAL "$ENV{TEST_ROOT}/SDK" OR
    NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR
    NOT CMAKE_OTOOL STREQUAL "$ENV{TEST_ROOT}/bin/otool" OR
    CMAKE_CROSSCOMPILING OR DEFINED CMAKE_SYSTEM_NAME OR
    CMAKE_EXE_LINKER_FLAGS MATCHES "--ld-path|fuse-ld")
  message(FATAL_ERROR "Native Apple configuration used cross or host Linux tools")
endif()
EOF
for entry in cmake/LonejsonCompiler.cmake cmake/toolchains/native.cmake cmake/toolchains/arm64-apple-darwin.cmake; do
  cmake -D "ENTRY=$fixture/$entry" -P "$tmp_dir/check.cmake"
done
if FAIL_XCRUN=1 "$fixture/scripts/cpkt-toolchains.sh" ensure arm64-apple-darwin >"$tmp_dir/error" 2>&1; then exit 1; fi
grep -F 'install Xcode or Command Line Tools' "$tmp_dir/error"
if TEST_ARCH=x86_64 "$fixture/scripts/cpkt-toolchains.sh" ensure arm64-apple-darwin >"$tmp_dir/error" 2>&1; then exit 1; fi
grep -F 'require Apple Silicon' "$tmp_dir/error"

# Linux-to-Darwin still uses osxcross and Darwin linker flags through CMake.
export TEST_SYSTEM=Linux
export CPKT_SYSROOT=/not-an-apple-sdk
mkdir -p "$OSXCROSS_ROOT/bin"
for tool in clang clang++ ld ar ranlib strip nm otool; do
  cp "$tmp_dir/bin/clang" "$OSXCROSS_ROOT/bin/arm64-apple-darwin25-$tool"
done
# Linux native builds resolve Bootlin regardless of CC.
cat >"$fixture/scripts/detect_native_bootlin_target.sh" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' x86_64-linux-gnu
EOF
cat >"$fixture/scripts/cpkt-toolchains.sh" <<'EOF'
#!/usr/bin/env bash
[[ "$1" == env && "$2" == x86_64-linux-gnu ]]
printf 'export CC=%q\n' "$TEST_ROOT/bin/bootlin-gcc"
EOF
cp "$tmp_dir/bin/clang" "$tmp_dir/bin/bootlin-gcc"
chmod +x "$fixture/scripts/"*.sh
: >"$tmp_dir/compiler.log"
bash "$repo_root/tests/test_oidc_pkce_provider_no_openssl.sh" "$fixture"
grep -F 'bootlin-gcc -std=c89' "$tmp_dir/compiler.log" >/dev/null
grep -Fx 'PKCE executed' "$tmp_dir/compiler.log"
if grep -E -- '-arch|-isysroot|/not-an-apple-sdk' "$tmp_dir/compiler.log"; then exit 1; fi

# Native Lua validation must execute every requested file in order and keep
# each file's temporary data inside the repository workspace.
cat >"$tmp_dir/bin/lua" <<'EOF_LUA'
#!/usr/bin/env bash
set -euo pipefail
[[ -d "$LONEJSON_TEST_TEMP_DIR" ]]
[[ "$LONEJSON_TEST_TEMP_DIR" == "$TEST_REPO_ROOT/build/"* ]]
printf '%s\n' "$1" >>"$TEST_ROOT/lua.log"
EOF_LUA
chmod +x "$tmp_dir/bin/lua"
TEST_SYSTEM=Darwin TEST_REPO_ROOT="$repo_root" "$repo_root/scripts/run_installed_lua.sh" \
  "$tmp_dir/tree" "$tmp_dir/lib" "$tmp_dir/bin/lua" first.lua second.lua
printf '%s\n' first.lua second.lua >"$tmp_dir/lua.expected"
diff -u "$tmp_dir/lua.expected" "$tmp_dir/lua.log"
