#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Rationale: release-matrix is the only non-clean rehearsal for every shipped
# binary SDK. This test prevents Darwin packaging, package verification, and
# optional integration metadata from drifting back into script-local shortcuts
# that bypass package-verify or force hard dependencies into core consumers.

repo_root=$1
matrix_script_path="$repo_root/scripts/run_release_matrix.sh"
matrix_script="$(cat "$matrix_script_path")"
verify_script_path="$repo_root/scripts/verify_release_archives.sh"
verify_script="$(cat "$verify_script_path")"
darwin_smoke_script_path="$repo_root/cmake/package_darwin_smoke_bundle.cmake"
darwin_smoke_script="$(cat "$darwin_smoke_script_path")"
darwin_release_smoke_script_path="$repo_root/scripts/smoke_darwin_release.sh"
darwin_release_smoke_script="$(cat "$darwin_release_smoke_script_path")"
cmake_lists="$(cat "$repo_root/CMakeLists.txt")"
tmp_dir="$(mktemp -d "$workspace_root/build/test_run_release_matrix_darwin_target.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

printf '%s\n' "$verify_script" | grep -F -- 'darwin_deployment_target="$(target_darwin_deployment_target)"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- '-D "LONEJSON_MACOS_DEPLOYMENT_TARGET=$darwin_deployment_target"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- '-D "CMAKE_OSX_DEPLOYMENT_TARGET=$darwin_deployment_target"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- '-D "LONEJSON_CPKT_ROOT=$adapter_dependency_root"' >/dev/null
grep -F 'list(APPEND _lonejson_find_root_path "${LONEJSON_CPKT_ROOT}")' \
  "$repo_root/cmake/toolchains/arm64-apple-darwin.cmake" >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'target_raw_compile_flags()' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'printf '\''%s\n'\'' "-std=c89 -Wall -Wextra -Werror -mmacosx-version-min=$(target_darwin_deployment_target)"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'raw_compile_flags="$(target_raw_compile_flags "$target_id")"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'printf '\''%s\n'\'' "--ld-path=$LINKER"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'target_toolchain_file()' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- '$TARGET_CFLAGS $raw_compile_flags $pkg_config_flags' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- '-D "CMAKE_TOOLCHAIN_FILE=$(target_toolchain_file "$target_id")"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'scripts/discover_target_tools.sh' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- 'package-darwin-smoke-bundle' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- 'make package-verify' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- 'scripts/osxcross_available.sh' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- 'require_command qemu-x86_64' >/dev/null
printf '%s\n' "$matrix_script" | grep -Fx -- 'set -euo pipefail' >/dev/null
grep -F 'liblonejson.${LONEJSON_ABI_VERSION}.dylib' \
  "$darwin_smoke_script_path" >/dev/null
grep -F 'LONEJSON_ABI_VERSION is required for Darwin smoke bundle' \
  "$darwin_smoke_script_path" >/dev/null
if grep -E 'set\(LONEJSON_ABI_VERSION "[0-9]+"\)' \
    "$darwin_smoke_script_path" >/dev/null; then
  printf 'Darwin smoke bundle must not hard-code an ABI fallback\n' >&2
  exit 1
fi
if printf '%s\n' "$darwin_smoke_script" | grep -F -- 'libcrypto' >/dev/null; then
  printf 'Darwin smoke bundle must not link libcrypto for core static SDK smoke\n' >&2
  exit 1
fi
if printf '%s\n' "$darwin_smoke_script" | grep -F -- 'LONEJSON_CPKT_ROOT is required for Darwin static smoke link' >/dev/null; then
  printf 'Darwin smoke bundle must not require cpkt for core static SDK smoke\n' >&2
  exit 1
fi
if printf '%s\n' "$darwin_release_smoke_script" | grep -F -- 'libcrypto' >/dev/null; then
  printf 'Darwin release smoke must not link libcrypto for core static SDK smoke\n' >&2
  exit 1
fi
if printf '%s\n' "$darwin_release_smoke_script" | grep -F -- 'LONEJSON_CPKT_ROOT for Darwin static smoke link' >/dev/null; then
  printf 'Darwin release smoke must not require cpkt for core static SDK smoke\n' >&2
  exit 1
fi
if grep -F 'liblonejson.4.dylib' \
    "$darwin_smoke_script_path" >/dev/null; then
  printf 'Darwin smoke bundle must not hard-code ABI dylib aliases\n' >&2
  exit 1
fi
if printf '%s\n' "$matrix_script" | grep -F -- 'require_archive_contract' >/dev/null; then
  printf 'run_release_matrix.sh must delegate archive checks to package-verify\n' >&2
  exit 1
fi
printf '%s\n' "$matrix_script" | grep -F -- 'missing cpkt CURL CMake package' >/dev/null
if printf '%s\n' "$matrix_script" | grep -F -- 'missing cpkt OpenSSL CMake package' >/dev/null; then
  printf 'release matrix must not add a script-specific OpenSSL precheck\n' >&2
  exit 1
fi
printf '%s\n' "$matrix_script" | grep -F -- '-D LONEJSON_BUILD_WITH_OPENSSL=ON' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- '-D LONEJSON_BUILD_WITH_JWT=ON' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- '-D LONEJSON_BUILD_WITH_OIDC=ON' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- 'run_target x86_64-linux-gnu-release x86_64-linux-gnu package-archive-x86_64-linux-gnu' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- 'ctest --no-tests=error' >/dev/null
for non_host_target in \
    'run_target x86_64-linux-musl-release x86_64-linux-musl package-archive-x86_64-linux-musl' \
    'run_target aarch64-linux-gnu-release aarch64-linux-gnu package-archive-aarch64-linux-gnu' \
    'run_target aarch64-linux-musl-release aarch64-linux-musl package-archive-aarch64-linux-musl' \
    'run_target armhf-linux-gnu-release armhf-linux-gnu package-archive-armhf-linux-gnu' \
    'run_target armhf-linux-musl-release armhf-linux-musl package-archive-armhf-linux-musl'; do
  printf '%s\n' "$matrix_script" | grep -F -- "$non_host_target" >/dev/null
done
for target_id in \
    x86_64-linux-gnu \
    x86_64-linux-musl \
    aarch64-linux-gnu \
    aarch64-linux-musl \
    armhf-linux-gnu \
    armhf-linux-musl \
    arm64-apple-darwin; do
  printf '%s\n' "$matrix_script" | grep -F -- "\"\$repo_root/scripts/deps.sh\" $target_id" >/dev/null
done
printf '%s\n' "$cmake_lists" | grep -F -- '-DLONEJSON_BUILD_WITH_OPENSSL=${LONEJSON_BUILD_WITH_OPENSSL}' >/dev/null
printf '%s\n' "$cmake_lists" | grep -F -- '-DLONEJSON_BUILD_WITH_JWT=${LONEJSON_BUILD_WITH_JWT}' >/dev/null
printf '%s\n' "$cmake_lists" | grep -F -- '-DLONEJSON_BUILD_WITH_OIDC=${LONEJSON_BUILD_WITH_OIDC}' >/dev/null
grep -F 'LINKER:-exported_symbols_list,${policy}' "$repo_root/cmake/lonejson_exports.cmake" >/dev/null
grep -F 'lonejson_limit_exports(lonejson_shared' "$repo_root/CMakeLists.txt" >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- '-D LONEJSON_CPKT_ROOT="$bundle_root"' >/dev/null
if printf '%s\n' "$matrix_script" | grep -F -- '-D CURL_LIBRARY_RELEASE=' >/dev/null; then
  printf 'run_release_matrix.sh must not inject raw CURL_LIBRARY_RELEASE paths\n' >&2
  exit 1
fi
if printf '%s\n' "$matrix_script" | grep -F -- '-D CURL_INCLUDE_DIR=' >/dev/null; then
  printf 'run_release_matrix.sh must not inject raw CURL_INCLUDE_DIR paths\n' >&2
  exit 1
fi
if printf '%s\n' "$matrix_script" | grep -F -- '-U CURL_' >/dev/null; then
  printf 'run_release_matrix.sh must not clean legacy CURL cache variables\n' >&2
  exit 1
fi
if printf '%s\n' "$matrix_script" | grep -F -- '-D OPENSSL_' >/dev/null; then
  printf 'run_release_matrix.sh must not inject raw OPENSSL paths\n' >&2
  exit 1
fi
if printf '%s\n' "$matrix_script" | grep -F -- '-U OPENSSL_' >/dev/null; then
  printf 'run_release_matrix.sh must not clean legacy OPENSSL cache variables\n' >&2
  exit 1
fi

fake_bin="$tmp_dir/bin"
toolchain_cache="$tmp_dir/toolchain-cache"
mkdir -p \
  "$fake_bin"
for tool in \
  cmake \
  ctest \
  gzip \
  tar \
  shasum \
  make \
  lua \
  luarocks \
  qemu-x86_64 \
  qemu-aarch64 \
  qemu-arm; do
  printf '#!/usr/bin/env bash\nexit 0\n' >"$fake_bin/$tool"
  chmod +x "$fake_bin/$tool"
done
while IFS='|' read -r name prefix triple; do
  root="$toolchain_cache/roots/$name"
  mkdir -p "$root/bin" "$root/$triple/sysroot/usr/include" "$root/$triple/sysroot/usr/lib"
  mkdir -p "$root/lib"
  touch "$root/$triple/sysroot/usr/include/stdio.h"
  touch "$root/$triple/sysroot/usr/lib/libc.so"
  touch "$root/lib/libstdc++.a" "$root/lib/libgcc.a"
  for tool in gcc g++ ld ar ranlib strip nm objcopy objdump addr2line gdb readelf; do
    if [[ "$tool" == g++ ]]; then
      printf '#!/usr/bin/env bash\ncase "${1:-}" in\n  -print-file-name=libstdc++.a) printf "%%s\\n" "$(dirname "$0")/../lib/libstdc++.a" ;;\n  -print-file-name=libgcc.a) printf "%%s\\n" "$(dirname "$0")/../lib/libgcc.a" ;;\n  *) exit 0 ;;\nesac\n' >"$root/bin/$prefix-$tool"
    else
      printf '#!/usr/bin/env bash\nexit 0\n' >"$root/bin/$prefix-$tool"
    fi
    chmod +x "$root/bin/$prefix-$tool"
  done
done <<'EOF'
x86-64--glibc--stable-2026.08-1|x86_64-linux|x86_64-buildroot-linux-gnu
x86-64--musl--stable-2026.08-1|x86_64-linux|x86_64-buildroot-linux-musl
aarch64--glibc--stable-2026.08-1|aarch64-linux|aarch64-buildroot-linux-gnu
aarch64--musl--stable-2026.08-1|aarch64-linux|aarch64-buildroot-linux-musl
armv7-eabihf--glibc--stable-2026.08-1|arm-linux|arm-buildroot-linux-gnueabihf
armv7-eabihf--musl--stable-2026.08-1|arm-linux|arm-buildroot-linux-musleabihf
EOF

if [[ $(uname -s):$(uname -m) == Linux:x86_64 ]]; then
  preflight_output="$(PATH="$fake_bin:/usr/bin:/bin" \
    CPKT_TOOLCHAIN_CACHE="$toolchain_cache" \
    LONEJSON_RELEASE_MATRIX_PREFLIGHT_ONLY=1 \
    "$matrix_script_path")"
  printf '%s\n' "$preflight_output" | grep -Fx 'Release matrix preflight completed successfully.' >/dev/null
fi

# Execute the actual matrix selections through CTest and the shipped presets.
# Only runtime may run: policy and service cases fail if a filter leaks them.
# Recording build tools avoid compiling the six targets for this scope check.
fixture="$tmp_dir/matrix-repo"
mkdir -p "$fixture/scripts" "$fixture/dist" "$tmp_dir/recording-bin"
cp "$matrix_script_path" "$fixture/scripts/run_release_matrix.sh"
cp "$repo_root/scripts/time_step.sh" "$fixture/scripts/time_step.sh"
cp "$repo_root/scripts/cross_test.sh" "$fixture/scripts/cross_test.sh"
cp "$repo_root/CMakePresets.json" "$fixture/CMakePresets.json"
export TEST_MATRIX_REAL_CTEST="$(command -v ctest)"
real_cmake="$(command -v cmake)"
for script in deps cpkt-toolchains; do
  cat >"$fixture/scripts/$script.sh" <<'SH'
#!/usr/bin/env bash
printf '%s %s\n' "${0##*/}" "$*" >>"$TEST_MATRIX_EVENTS"
SH
  chmod +x "$fixture/scripts/$script.sh"
done
printf '#!/bin/bash\nexit 1\n' >"$fixture/scripts/osxcross_available.sh"
chmod +x "$fixture/scripts/osxcross_available.sh"
for tool in cmake ctest make ninja luarocks qemu-x86_64 qemu-aarch64 qemu-arm; do
  cat >"$tmp_dir/recording-bin/$tool" <<'SH'
#!/usr/bin/env bash
printf '%s %s\n' "${0##*/}" "$*" >>"$TEST_MATRIX_EVENTS"
if [[ "${0##*/}" == ctest ]]; then exec "$TEST_MATRIX_REAL_CTEST" "$@"; fi
SH
  chmod +x "$tmp_dir/recording-bin/$tool"
done
for target in x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl armhf-linux-gnu armhf-linux-musl; do
  mkdir -p "$fixture/.cache/cpkt/$target/root/lib/cmake/CURL"
  touch "$fixture/.cache/cpkt/$target/root/lib/cmake/CURL/CURLConfig.cmake"
  mkdir -p "$fixture/build/$target-release"
  cat >"$fixture/build/$target-release/CTestTestfile.cmake" <<TESTS
add_test(runtime "$real_cmake" -E true)
set_tests_properties(runtime PROPERTIES LABELS local TIMEOUT 5)
add_test(policy "$real_cmake" -E false)
set_tests_properties(policy PROPERTIES LABELS host-policy TIMEOUT 5)
add_test(service "$real_cmake" -E false)
set_tests_properties(service PROPERTIES LABELS e2e TIMEOUT 5)
TESTS
done
export TEST_MATRIX_EVENTS="$tmp_dir/matrix-events"
printf retained >"$fixture/dist/retained.tar.gz"
if ! PATH="$tmp_dir/recording-bin:$PATH" "$fixture/scripts/run_release_matrix.sh" >"$tmp_dir/matrix-output" 2>&1; then
  cat "$tmp_dir/matrix-output" >&2; exit 1
fi
[[ $(cat "$fixture/dist/retained.tar.gz") == retained ]]
[[ $(grep -c '^ctest ' "$TEST_MATRIX_EVENTS") == 6 ]]
[[ $(grep -c '100% tests passed, 0 tests failed out of 1' "$tmp_dir/matrix-output") == 6 ]]
[[ $(grep -c '^cmake --build --preset package-archive-' "$TEST_MATRIX_EVENTS") == 6 ]]
! grep -E 'clean|package-source|^cmake --build --preset package-checksums$|^make package-verify$' "$TEST_MATRIX_EVENTS"
grep -Fx 'cmake --build --preset host --target package-binary-checksums' "$TEST_MATRIX_EVENTS"
grep -Fx 'make package-verify-binary' "$TEST_MATRIX_EVENTS"

# The ordinary cross-test entry point must preserve the same preset scope.
: >"$TEST_MATRIX_EVENTS"
if ! PATH="$tmp_dir/recording-bin:$PATH" "$fixture/scripts/cross_test.sh" \
  x86_64-linux-gnu-release x86_64-linux-musl-release \
  aarch64-linux-gnu-release aarch64-linux-musl-release \
  armhf-linux-gnu-release armhf-linux-musl-release >"$tmp_dir/cross-output" 2>&1; then
  cat "$tmp_dir/cross-output" >&2; exit 1
fi
[[ $(grep -c '^ctest ' "$TEST_MATRIX_EVENTS") == 6 ]]
[[ $(grep -c '100% tests passed, 0 tests failed out of 1' "$tmp_dir/cross-output") == 6 ]]

# Excluding every registered case must fail, rather than claim target coverage.
sed '/^add_test(runtime /d; /^set_tests_properties(runtime /d' \
  "$fixture/build/x86_64-linux-musl-release/CTestTestfile.cmake" \
  >"$fixture/build/x86_64-linux-gnu-release/CTestTestfile.cmake"
: >"$TEST_MATRIX_EVENTS"
if PATH="$tmp_dir/recording-bin:$PATH" "$fixture/scripts/run_release_matrix.sh" >"$tmp_dir/matrix-empty" 2>&1; then
  printf 'binary matrix accepted an empty runtime selection\n' >&2; exit 1
fi
grep -F 'No tests were found' "$tmp_dir/matrix-empty" >/dev/null
[[ $(grep -c '^ctest ' "$TEST_MATRIX_EVENTS") == 1 ]]
! grep -E 'package-archive-|^make ' "$TEST_MATRIX_EVENTS"
cp "$fixture/build/x86_64-linux-musl-release/CTestTestfile.cmake" \
  "$fixture/build/x86_64-linux-gnu-release/CTestTestfile.cmake"

# A real failing runtime test stops before packaging that target or later ones.
sed 's/-E true)/-E false)/' \
  "$fixture/build/x86_64-linux-musl-release/CTestTestfile.cmake" \
  >"$fixture/build/aarch64-linux-gnu-release/CTestTestfile.cmake"
: >"$TEST_MATRIX_EVENTS"
if PATH="$tmp_dir/recording-bin:$PATH" \
    "$fixture/scripts/run_release_matrix.sh" >"$tmp_dir/matrix-failed" 2>&1; then
  printf 'binary matrix continued after a required target test failed\n' >&2; exit 1
fi
[[ $(grep -c '^ctest ' "$TEST_MATRIX_EVENTS") == 3 ]]
! grep -E '^make |package-binary-checksums|package-archive-aarch64-linux-gnu|armhf-linux-gnu-release' "$TEST_MATRIX_EVENTS"
[[ $(cat "$fixture/dist/retained.tar.gz") == retained ]]
