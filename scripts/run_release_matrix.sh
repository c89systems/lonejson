#!/usr/bin/env bash

set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'missing required command: %s\n' "$1" >&2
        exit 1
    fi
}

configure_release_target() {
    preset="$1"
    target_id="$2"

    bundle_root="$repo_root/.cache/cpkt/$target_id/root"
    if [ ! -f "$bundle_root/lib/cmake/CURL/CURLConfig.cmake" ]; then
        printf 'missing cpkt CURL CMake package for %s under %s\n' "$target_id" "$bundle_root/lib/cmake/CURL" >&2
        exit 1
    fi
    cmake --preset "$preset" \
        -D "LONEJSON_RELEASE_TARGETS=$targets" \
        -D LONEJSON_BUILD_WITH_CURL=ON \
        -D LONEJSON_BUILD_WITH_OPENSSL=ON \
        -D LONEJSON_BUILD_WITH_JWT=ON \
        -D LONEJSON_BUILD_WITH_OIDC=ON \
        -D LONEJSON_CPKT_ROOT="$bundle_root"
}

run_target() {
    preset="$1"
    target_id="$2"
    package_preset="$3"

    printf '\n== %s ==\n' "$preset"
    configure_release_target "$preset" "$target_id"
    cmake --build --preset "$preset"
    ctest --no-tests=error --preset "$preset" --output-on-failure
    cmake --build --preset "$package_preset"
}

run_build_only_target() {
    preset="$1"
    target_id="$2"
    package_preset="$3"

    printf '\n== %s ==\n' "$preset"
    configure_release_target "$preset" "$target_id"
    cmake --build --preset "$preset"
    cmake --build --preset "$package_preset"
}

require_command cmake
require_command ctest
require_command make
require_command ninja
require_command luarocks
require_command qemu-x86_64
require_command qemu-aarch64
require_command qemu-arm
"$repo_root/scripts/cpkt-toolchains.sh" ensure all

# LuaRocks and host-tooling checks compile host modules against the configured
# build tree. They are exercised by the native release target, but cannot link
# a foreign target library; QEMU coverage below still executes every runnable
# target test and package check.

darwin_toolchain=
if darwin_description="$("$repo_root/scripts/osxcross_available.sh" 2>/dev/null)"; then
    darwin_toolchain="$(printf '%s\n' "$darwin_description" | sed -n 's/^cc=//p')"
fi

if [ "${LONEJSON_RELEASE_MATRIX_PREFLIGHT_ONLY:-0}" = "1" ]; then
    printf 'Release matrix preflight completed successfully.\n'
    exit 0
fi

cd "$repo_root"

targets="x86_64-linux-gnu;x86_64-linux-musl;aarch64-linux-gnu;aarch64-linux-musl;armhf-linux-gnu;armhf-linux-musl"
if [[ -n "$darwin_toolchain" ]]; then targets+=";arm64-apple-darwin"; fi
cmake --preset host "-DLONEJSON_RELEASE_TARGETS=$targets"

"$repo_root/scripts/deps.sh" x86_64-linux-gnu
"$repo_root/scripts/deps.sh" x86_64-linux-musl
"$repo_root/scripts/deps.sh" aarch64-linux-gnu
"$repo_root/scripts/deps.sh" aarch64-linux-musl
"$repo_root/scripts/deps.sh" armhf-linux-gnu
"$repo_root/scripts/deps.sh" armhf-linux-musl
run_target x86_64-linux-gnu-release x86_64-linux-gnu package-archive-x86_64-linux-gnu
run_target x86_64-linux-musl-release x86_64-linux-musl package-archive-x86_64-linux-musl
run_target aarch64-linux-gnu-release aarch64-linux-gnu package-archive-aarch64-linux-gnu
run_target aarch64-linux-musl-release aarch64-linux-musl package-archive-aarch64-linux-musl
run_target armhf-linux-gnu-release armhf-linux-gnu package-archive-armhf-linux-gnu
run_target armhf-linux-musl-release armhf-linux-musl package-archive-armhf-linux-musl
if [ -n "$darwin_toolchain" ] && [ -x "$darwin_toolchain" ]; then
    darwin_tool_bin="$(dirname -- "$darwin_toolchain")"
    export PATH="$darwin_tool_bin:$PATH"
    "$repo_root/scripts/deps.sh" arm64-apple-darwin
    run_build_only_target arm64-apple-darwin-release arm64-apple-darwin package-archive-arm64-apple-darwin
    ./scripts/smoke_darwin_release.sh arm64-apple-darwin-release
    cmake --build --preset arm64-apple-darwin-release --target package-darwin-smoke-bundle
else
    printf '\n== arm64-apple-darwin-release ==\n'
    printf 'Skipping Darwin release target: local osxcross toolchain is unavailable\n'
fi

cmake --build --preset package-single-header
make release-lua-artifacts
cmake --build --preset host --target package-binary-checksums
make package-verify-binary

printf '\nRelease matrix completed successfully.\n'
