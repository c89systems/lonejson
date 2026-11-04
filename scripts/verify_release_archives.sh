#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"
owned_workspaces=()
cleanup() {
  for workspace in "${owned_workspaces[@]}"; do rm -rf -- "$workspace"; done
}
trap cleanup EXIT

repo_root=${1:?usage: verify_release_archives.sh REPO_ROOT [CHECKSUMS] [BUILD_ROOT]}
checksums=${2:-}
build_root=${3:-}

repo_root="$(CDPATH= cd -- "$repo_root" && pwd)"
if [[ -z "$checksums" ]]; then
  version="$("$repo_root/scripts/release_version.sh")"
  checksums="$repo_root/dist/lonejson-$version-CHECKSUMS"
fi
if [[ -z "$build_root" ]]; then
  build_root="$repo_root/build"
fi
if [[ ! -f "$checksums" ]]; then
  printf 'missing release checksum manifest: %s\n' "$checksums" >&2
  exit 1
fi

dist_dir="$(CDPATH= cd -- "${4:-$(dirname -- "$checksums")}" && pwd)"
if [[ -d "$build_root" ]]; then
  build_root="$(CDPATH= cd -- "$build_root" && pwd)"
fi

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'missing required command: %s\n' "$1" >&2
    exit 1
  fi
}

require_file() {
  if [[ ! -e "$1" ]]; then
    printf 'missing required file: %s\n' "$1" >&2
    exit 1
  fi
}

target_preset() {
  local target_id=$1
  case "$target_id" in
    x86_64-linux-gnu) printf '%s\n' x86_64-linux-gnu-release ;;
    x86_64-linux-musl) printf '%s\n' x86_64-linux-musl-release ;;
    aarch64-linux-gnu) printf '%s\n' aarch64-linux-gnu-release ;;
    aarch64-linux-musl) printf '%s\n' aarch64-linux-musl-release ;;
    armhf-linux-gnu) printf '%s\n' armhf-linux-gnu-release ;;
    armhf-linux-musl) printf '%s\n' armhf-linux-musl-release ;;
    arm64-apple-darwin) printf '%s\n' arm64-apple-darwin-release ;;
    *)
      printf 'unknown release target id: %s\n' "$target_id" >&2
      exit 1
      ;;
  esac
}

target_toolchain_file() {
  local target_id=$1
  case "$target_id" in
    x86_64-linux-gnu) printf '%s\n' "$repo_root/cmake/toolchains/linux-x86_64-gnu.cmake" ;;
    x86_64-linux-musl) printf '%s\n' "$repo_root/cmake/toolchains/linux-x86_64-musl.cmake" ;;
    aarch64-linux-gnu) printf '%s\n' "$repo_root/cmake/toolchains/linux-aarch64-gnu.cmake" ;;
    aarch64-linux-musl) printf '%s\n' "$repo_root/cmake/toolchains/linux-aarch64-musl.cmake" ;;
    armhf-linux-gnu) printf '%s\n' "$repo_root/cmake/toolchains/linux-armhf-gnu.cmake" ;;
    armhf-linux-musl) printf '%s\n' "$repo_root/cmake/toolchains/linux-armhf-musl.cmake" ;;
    *)
      printf 'unknown Linux target id: %s\n' "$target_id" >&2
      exit 1
      ;;
  esac
}

target_darwin_deployment_target() {
  printf '%s\n' "${LONEJSON_MACOS_DEPLOYMENT_TARGET:-15.0}"
}

target_raw_compile_flags() {
  local target_id=$1
  case "$target_id" in
    arm64-apple-darwin)
      printf '%s\n' "-std=c89 -Wall -Wextra -Werror -mmacosx-version-min=$(target_darwin_deployment_target)"
      ;;
    *) printf '%s\n' "-std=c89 -Wall -Wextra -Werror" ;;
  esac
}

target_raw_link_flags() {
  local target_id=$1
  case "$target_id" in
    arm64-apple-darwin)
      if [[ -z "${LINKER:-}" ]]; then
        printf 'missing target linker for %s\n' "$target_id" >&2
        exit 1
      fi
      printf '%s\n' "--ld-path=$LINKER"
      printf '%s\n' "-Wl,-fatal_warnings"
      ;;
    *) printf '%s\n' "-Wl,--fatal-warnings" ;;
  esac
}

target_development_runtime_flags() {
  local target_id=$1
  local preset=$2
  local extra_dirs=$3
  local sysroot loader libc runtime_dirs

  # Only the native x86_64 glibc target can execute on this lifecycle host.
  # Cross targets retain their configured QEMU/sysroot route and Darwin uses
  # its native loader metadata.
  if [[ "$target_id" != x86_64-linux-gnu || "$(uname -m)" != x86_64 ]]; then
    printf '%s\n' ""
    return
  fi
  sysroot="$(target_cache_value "$preset" CMAKE_SYSROOT)"
  libc="$(target_cache_value "$preset" LONEJSON_BOOTLIN_LIBC)"
  if [[ -z "$sysroot" && -z "$libc" ]]; then
    # Metadata-only archive checks may deliberately run without the original
    # build tree. Runtime execution is covered whenever that configured state
    # is available.
    printf '%s\n' ""
    return
  fi
  if [[ "$libc" != gnu || ! -d "$sysroot/lib" ]]; then
    printf 'native package consumer lacks the selected Bootlin glibc sysroot\n' >&2
    exit 1
  fi
  loader=$(printf '%s\n' "$sysroot"/lib/ld-linux*.so.2)
  if [[ ! -x "$loader" ]]; then
    printf 'native package consumer lacks Bootlin loader under %s/lib\n' "$sysroot" >&2
    exit 1
  fi
  runtime_dirs="$sysroot/lib:$extra_dirs"
  printf '%s\n' "-Wl,--dynamic-linker,$loader -Wl,--disable-new-dtags -Wl,-rpath,$runtime_dirs"
}

load_target_tools() {
  local preset=$1
  local target_id=$2

  resolved_environment=$("$repo_root/scripts/discover_target_tools.sh" \
    --build-dir "$build_root/$preset" \
    --target-id "$target_id") || exit $?
  eval "$resolved_environment"
}

target_cache_value() {
  local preset=$1
  local name=$2
  local cache_file="$build_root/$preset/CMakeCache.txt"

  if [[ -f "$cache_file" ]]; then
    sed -n "s/^${name}:[^=]*=//p" "$cache_file" | tail -n 1
  fi
}

target_dependency_root() {
  local preset=$1

  target_cache_value "$preset" LONEJSON_CPKT_ROOT
}

target_pkg_config_path() {
  local package_root=$1
  local preset=$2
  local dependency_root

  dependency_root="$(target_dependency_root "$preset")"
  printf '%s' "$package_root/lib/pkgconfig"
  if [[ -n "$dependency_root" && -d "$dependency_root/lib/pkgconfig" ]]; then
    printf ':%s' "$dependency_root/lib/pkgconfig"
  fi
  if [[ -n "${PKG_CONFIG_PATH:-}" ]]; then
    printf ':%s' "$PKG_CONFIG_PATH"
  fi
  printf '\n'
}

target_cmake_prefix_path() {
  local package_root=$1
  local preset=$2
  local dependency_root

  dependency_root="$(target_dependency_root "$preset")"
  printf '%s' "$package_root"
  if [[ -n "$dependency_root" ]]; then
    printf ';%s' "$dependency_root"
  fi
  printf '\n'
}

run_with_target_path() {
  local target_id=$1
  shift

  if [[ "${target_id#*apple-darwin}" != "$target_id" && -n "${LINKER:-}" ]]; then
    local linker_dir
    linker_dir="$(dirname -- "$LINKER")"
    PATH="$linker_dir:$PATH" "$@"
  else
    "$@"
  fi
}

require_linux_origin_rpath() {
  local archive=$1
  local dynamic_metadata=$2
  local rpath_values old_ifs rpath_value

  rpath_values="$(printf '%s\n' "$dynamic_metadata" |
    sed -n 's/.*\(RUNPATH\|RPATH\).*: \[\(.*\)\].*/\2/p')"
  if [[ -z "$rpath_values" ]]; then
    return
  fi
  old_ifs=$IFS
  IFS=':'
  for rpath_value in $rpath_values; do
    if [[ "$rpath_value" != '$ORIGIN' &&
        "${rpath_value#'$ORIGIN'/}" == "$rpath_value" ]]; then
      IFS=$old_ifs
      printf 'non-origin rpath in %s: %s\n' "$archive" "$rpath_value" >&2
      exit 1
    fi
  done
  IFS=$old_ifs
}

archive_target_id() {
  local artifact=$1
  local target_id
  for target_id in \
      x86_64-linux-gnu \
      x86_64-linux-musl \
      aarch64-linux-gnu \
      aarch64-linux-musl \
      armhf-linux-gnu \
      armhf-linux-musl \
      arm64-apple-darwin; do
    case "$artifact" in
      liblonejson-*-"$target_id".tar.gz)
        printf '%s\n' "$target_id"
        return
        ;;
    esac
  done
}

extract_archive() {
  local archive=$1
  local tmp_dir=$2
  local package_root

  tar -xzf "$archive" -C "$tmp_dir"
  package_root="$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d | sort | head -n 1)"
  if [[ -z "$package_root" ]]; then
    printf 'failed to inspect archive: %s\n' "$archive" >&2
    exit 1
  fi
  printf '%s\n' "$package_root"
}

require_no_bootlin_toolchain_paths() {
  local archive=$1
  local package_root=$2
  local toolchain_path_pattern

  # Release packages must be relocatable.  A Bootlin collection path belongs
  # only to a local development executable, never to a packaged library,
  # static archive, or consumer metadata.  The collection-name match also
  # catches installations moved through CPKT_TOOLCHAIN_CACHE.
  toolchain_path_pattern='/(cpkt/)?toolchains(/roots)?/|/[^[:space:]:;]*(x86-64|aarch64|armv7-eabihf)--(glibc|musl)--stable-[^[:space:]:;]*'
  if grep -aRE "$toolchain_path_pattern" "$package_root" >/dev/null 2>&1; then
    printf 'pinned Bootlin toolchain path leaked in release archive: %s\n' \
      "$archive" >&2
    exit 1
  fi
}

require_no_bootlin_elf_loader_metadata() {
  local archive=$1
  local metadata=$2
  local bootlin_loader_path_pattern

  bootlin_loader_path_pattern='cpkt/toolchains/|/(x86-64|aarch64|armv7-eabihf)--(glibc|musl)--stable-'
  if printf '%s\n' "$metadata" |
      grep -E '(Requesting program interpreter|RUNPATH|RPATH)' |
      grep -E "$bootlin_loader_path_pattern" >/dev/null; then
    printf 'pinned Bootlin ELF interpreter or RPATH leaked in release archive: %s\n' \
      "$archive" >&2
    exit 1
  fi
}

require_archive_contract() {
  local archive=$1
  local target_id=$2
  local preset=$3
  local tmp_dir package_root shared_lib dynamic_metadata rpath_paths dependency_manifest
  local forbidden_metadata_pattern

  load_target_tools "$preset" "$target_id"

  tar -tzf "$archive" | while IFS= read -r entry; do
    entry_without_root="${entry#*/}"
    case "$entry_without_root" in
      "" | \
      "include/" | \
      "include/lonejson.h" | \
      "lib/" | \
      "lib/cmake/" | \
      "lib/cmake/lonejson/" | \
      "lib/cmake/lonejson/lonejsonConfig.cmake" | \
      "lib/cmake/lonejson/lonejsonConfigVersion.cmake" | \
      "lib/liblonejson.a" | \
      "lib/liblonejson.so" | \
      "lib/liblonejson.so."* | \
      "lib/liblonejson.dylib" | \
      "lib/liblonejson."*".dylib" | \
      "lib/pkgconfig/" | \
      "lib/pkgconfig/lonejson-curl.pc" | \
      "lib/pkgconfig/lonejson-jwt.pc" | \
      "lib/pkgconfig/lonejson-oidc.pc" | \
      "lib/pkgconfig/lonejson-openssl.pc" | \
      "lib/pkgconfig/lonejson.pc" | \
      "share/" | \
      "share/lonejson/" | \
      "share/lonejson/dependencies.json" | \
      "share/doc/" | \
      "share/doc/liblonejson/" | \
      "share/doc/liblonejson/LICENSE" | \
      "share/doc/liblonejson/README.md")
        ;;
      *)
        printf 'unexpected file in release archive %s: %s\n' "$archive" "$entry_without_root" >&2
        exit 1
        ;;
    esac
  done

  tmp_dir="$(mktemp -d "$workspace_root/build/verify_release_archives.XXXXXX")"
  owned_workspaces+=("$tmp_dir")
  package_root="$(extract_archive "$archive" "$tmp_dir")"

  require_no_bootlin_toolchain_paths "$archive" "$package_root"

  require_file "$package_root/lib/pkgconfig/lonejson.pc"
  require_file "$package_root/lib/cmake/lonejson/lonejsonConfig.cmake"
  require_file "$package_root/lib/cmake/lonejson/lonejsonConfigVersion.cmake"
  dependency_manifest="$package_root/share/lonejson/dependencies.json"
  require_file "$dependency_manifest"
  forbidden_metadata_pattern='(^|[^[:alnum:]_])(CURL::libcurl|OpenSSL::|openssl|OpenSSL|libcurl|libssl|libcrypto|-lcurl|-lssl|-lcrypto|crypto)([^[:alnum:]_]|$)'
  require_file "$package_root/lib/pkgconfig/lonejson-curl.pc"
  require_file "$package_root/lib/pkgconfig/lonejson-jwt.pc"
  require_file "$package_root/lib/pkgconfig/lonejson-oidc.pc"
  require_file "$package_root/lib/pkgconfig/lonejson-openssl.pc"
  if grep -RE 'cpkt|\.cache/|\.deps/|/home/|/build/' \
      "$package_root/lib/pkgconfig/lonejson.pc" \
      "$package_root/lib/cmake/lonejson" >/dev/null; then
    printf 'forbidden dependency or path leak in release metadata for %s\n' "$archive" >&2
    exit 1
  fi
  if grep -Eq "$forbidden_metadata_pattern" "$package_root/lib/pkgconfig/lonejson.pc"; then
    printf 'unexpected third-party pkg-config dependency in core lonejson SDK: %s\n' "$archive" >&2
    exit 1
  fi
  if awk '
      /add_library\(lonejson::lonejson SHARED IMPORTED\)/ { in_core = 1 }
      /add_library\(lonejson::lonejson_static STATIC IMPORTED\)/ { in_core = 1 }
      in_core { print }
      in_core && /^endif\(\)/ { in_core = 0 }
    ' "$package_root/lib/cmake/lonejson/lonejsonConfig.cmake" |
      grep -Eq "$forbidden_metadata_pattern"; then
    printf 'unexpected third-party CMake dependency in core lonejson SDK: %s\n' "$archive" >&2
    exit 1
  fi
  if grep -E '\.cache/|\.deps/|/home/|/build/|file://' "$dependency_manifest" >/dev/null; then
    printf 'forbidden path leak in dependency manifest for %s\n' "$archive" >&2
    exit 1
  fi
  require_file "$package_root/share/doc/liblonejson/LICENSE"
  if [[ ! -s "$package_root/share/doc/liblonejson/LICENSE" ]]; then
    printf 'missing project license text in %s\n' "$archive" >&2
    exit 1
  fi
  local archive_version
  archive_version=$(basename -- "$archive")
  archive_version=${archive_version#liblonejson-}
  archive_version=${archive_version%-$target_id.tar.gz}
  cmake -DLONEJSON_MANIFEST="$dependency_manifest" \
    -DLONEJSON_VERSION="$archive_version" \
    -DLONEJSON_TARGET_ID="$target_id" \
    -P "$repo_root/cmake/verify_dependency_manifest.cmake"

  if [[ "${target_id#*apple-darwin}" != "$target_id" ]]; then
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson*.dylib' | sort | head -n 1)"
    if [[ -z "$shared_lib" ]]; then
      printf 'missing packaged shared library in %s\n' "$archive" >&2
      exit 1
    fi
    if [[ -z "$OTOOL" ]]; then
      printf 'missing target otool for %s\n' "$target_id" >&2
      exit 1
    fi
    "$repo_root/scripts/check_darwin_macho_metadata.sh" \
      "$repo_root" \
      "$build_root/$preset" \
      "$target_id" \
      "$shared_lib" \
      "$archive"
    dynamic_metadata="$("$OTOOL" -l "$shared_lib" | awk '$1 == "name" || $1 == "path" { print }')"
    case "$dynamic_metadata" in
      *libcurl* | *libssl* | *libcrypto* | *OpenSSL* | *cpkt* | *".cache/"* | *".deps/"* | *"$repo_root"* | *"/home/"* | *"/build/"*)
        printf 'forbidden dependency or path leak in %s\n' "$archive" >&2
        exit 1
        ;;
    esac
    rpath_paths="$(printf '%s\n' "$dynamic_metadata" | sed -n 's/^[[:space:]]*path \([^ ]*\).*/\1/p')"
    if [[ -n "$rpath_paths" ]] && printf '%s\n' "$rpath_paths" | grep -Ev '^@loader_path(/|$)' >/dev/null; then
      printf 'non-loader-relative rpath in %s\n' "$archive" >&2
      exit 1
    fi
  else
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.so*' ! -type l | sort | head -n 1)"
    if [[ -z "$shared_lib" ]]; then
      printf 'missing packaged shared library in %s\n' "$archive" >&2
      exit 1
    fi
    if [[ -z "$READELF" ]]; then
      printf 'missing target readelf for %s\n' "$target_id" >&2
      exit 1
    fi
    dynamic_metadata="$("$READELF" -d "$shared_lib")"
    require_no_bootlin_elf_loader_metadata "$archive" "$dynamic_metadata"
    case "$dynamic_metadata" in
      *libcurl* | *libssl* | *libcrypto* | *OpenSSL* | *cpkt* | *".cache/"* | *".deps/"* | *"$repo_root"* | *"/home/"* | *"/build/"*)
        printf 'forbidden dependency or path leak in %s\n' "$archive" >&2
        exit 1
        ;;
    esac
    require_linux_origin_rpath "$archive" "$dynamic_metadata"
  fi

  system=Linux
  [[ "$target_id" != *apple-darwin ]] || system=Darwin
  bash "$repo_root/scripts/check_library_exports.sh" "$NM" "$system" "$shared_lib" \
    "$repo_root/cmake/lonejson.exports"
  bash "$repo_root/scripts/check_private_link.sh" "$CC" "$system" "$shared_lib" "$TARGET_CFLAGS"
  rm -rf "$tmp_dir"
}

require_archive_consumer_metadata() {
  local archive=$1
  local target_id=$2
  local preset=$3
  local tmp_dir package_root consumer_source pkg_config_flags raw_compile_flags
  local pkg_config_static_flags raw_link_flags runtime_link_flags cmake_source_dir cmake_build_dir
  local adapter_source adapter_cmake_source_dir adapter_cmake_build_dir
  local adapter_pkg_config_flags adapter_c89_flags cmake_prefix_path pkg_config_path
  local adapter_dependency_root adapter_runtime_link_flags
  local darwin_deployment_target cmake_system_name

  load_target_tools "$preset" "$target_id"

  tmp_dir="$(mktemp -d "$workspace_root/build/verify_release_archives.XXXXXX")"
  owned_workspaces+=("$tmp_dir")
  package_root="$(extract_archive "$archive" "$tmp_dir")"
  pkg_config_path="$(target_pkg_config_path "$package_root" "$preset")"
  cmake_prefix_path="$(target_cmake_prefix_path "$package_root" "$preset")"
  adapter_dependency_root="$(target_dependency_root "$preset")"

  consumer_source="$tmp_dir/consumer.c"
  cat >"$consumer_source" <<'EOF'
#include <lonejson.h>
#include <stddef.h>

int main(void) {
    lonejson *runtime;
    lonejson_error error;

    lonejson_error_init(&error);
    runtime = lonejson_new(NULL, &error);
    if (runtime == NULL) {
        return 1;
    }
    if (lonejson_validate_cstr(runtime, "{\"ok\":true}", &error) != LONEJSON_STATUS_OK) {
        lonejson_free(runtime);
        return 1;
    }
    lonejson_free(runtime);
    return 0;
}
EOF

  require_command pkg-config
  pkg_config_flags="$(PKG_CONFIG_PATH="$pkg_config_path" pkg-config --cflags --libs lonejson)"
  pkg_config_static_flags="$(PKG_CONFIG_PATH="$pkg_config_path" pkg-config --cflags --static --libs lonejson)"
  raw_compile_flags="$(target_raw_compile_flags "$target_id")"
  raw_link_flags="$(target_raw_link_flags "$target_id")"
  runtime_link_flags="$(target_development_runtime_flags "$target_id" "$preset" "$package_root/lib")"
  # shellcheck disable=SC2086
  run_with_target_path "$target_id" "$CC" "$consumer_source" $TARGET_CFLAGS $raw_compile_flags $pkg_config_flags $raw_link_flags $runtime_link_flags -o "$tmp_dir/pkg-config-consumer"
  # shellcheck disable=SC2086
  run_with_target_path "$target_id" "$CC" "$consumer_source" $TARGET_CFLAGS $raw_compile_flags $pkg_config_static_flags $raw_link_flags $runtime_link_flags -o "$tmp_dir/pkg-config-static-consumer"

  cmake_source_dir="$tmp_dir/cmake-consumer"
  cmake_build_dir="$tmp_dir/cmake-build"
  mkdir -p "$cmake_source_dir"
  cp "$consumer_source" "$cmake_source_dir/main.c"
  cat >"$cmake_source_dir/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(lonejson_archive_consumer C)
include("${LONEJSON_DEVELOPMENT_RUNTIME_MODULE}")
get_filename_component(policy_dir "${LONEJSON_DEVELOPMENT_RUNTIME_MODULE}" DIRECTORY)
include("${policy_dir}/LonejsonLinkWarnings.cmake")
find_package(lonejson CONFIG REQUIRED)
add_executable(lonejson_archive_consumer_shared main.c)
target_link_libraries(lonejson_archive_consumer_shared PRIVATE lonejson::lonejson)
add_executable(lonejson_archive_consumer_static main.c)
target_link_libraries(lonejson_archive_consumer_static PRIVATE lonejson::lonejson_static)
foreach(consumer lonejson_archive_consumer_shared lonejson_archive_consumer_static)
  target_compile_options(${consumer} PRIVATE -std=c89 -Wall -Wextra -Werror)
  lonejson_configure_link_warnings(${consumer})
endforeach()
lonejson_configure_development_runtime(lonejson_archive_consumer_shared)
lonejson_configure_development_runtime(lonejson_archive_consumer_static)
EOF

  if [[ "${target_id#*apple-darwin}" != "$target_id" ]]; then
    darwin_deployment_target="$(target_darwin_deployment_target)"
    cmake_args=(
      -S "$cmake_source_dir"
      -B "$cmake_build_dir"
      -G Ninja
      -D "CMAKE_PREFIX_PATH=$cmake_prefix_path"
      -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
      -D "LONEJSON_DEVELOPMENT_RUNTIME_MODULE=$repo_root/cmake/LonejsonDevelopmentRuntime.cmake"
      -D "LONEJSON_DEVELOPMENT_RUNTIME_EXTRA_DIRS=$package_root/lib"
      -D "CMAKE_TOOLCHAIN_FILE=$repo_root/cmake/toolchains/arm64-apple-darwin.cmake"
      -D "LONEJSON_MACOS_DEPLOYMENT_TARGET=$darwin_deployment_target"
      -D "CMAKE_OSX_DEPLOYMENT_TARGET=$darwin_deployment_target"
      -D "LONEJSON_CPKT_ROOT=$adapter_dependency_root"
    )
  else
    cmake_args=(
      -S "$cmake_source_dir"
      -B "$cmake_build_dir"
      -G Ninja
      -D "CMAKE_PREFIX_PATH=$cmake_prefix_path"
      -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
      -D "LONEJSON_DEVELOPMENT_RUNTIME_MODULE=$repo_root/cmake/LonejsonDevelopmentRuntime.cmake"
      -D "LONEJSON_DEVELOPMENT_RUNTIME_EXTRA_DIRS=$package_root/lib"
      -D "CMAKE_TOOLCHAIN_FILE=$(target_toolchain_file "$target_id")"
    )
  fi
  run_with_target_path "$target_id" cmake "${cmake_args[@]}"
  run_with_target_path "$target_id" cmake --build "$cmake_build_dir"
  if [[ "$target_id" == x86_64-linux-gnu &&
        -f "$build_root/$preset/CMakeCache.txt" ]]; then
    "$repo_root/tests/test_bootlin_runtime.sh" \
      "$build_root/$preset" "$cmake_build_dir/lonejson_archive_consumer_shared" --run
    "$repo_root/tests/test_bootlin_runtime.sh" \
      "$build_root/$preset" "$cmake_build_dir/lonejson_archive_consumer_static" --run
    "$tmp_dir/pkg-config-consumer"
    "$tmp_dir/pkg-config-static-consumer"
  fi

  if [[ -z "$adapter_dependency_root" ]]; then
    rm -rf "$tmp_dir"
    return
  fi

  adapter_source="$tmp_dir/adapter-consumer.c"
  cat >"$adapter_source" <<'EOF'
#include <lonejson.h>
#include <stddef.h>

#ifndef LONEJSON_WITH_CURL
#error "lonejson curl adapter target did not define LONEJSON_WITH_CURL"
#endif
#ifndef LONEJSON_WITH_JWT
#error "lonejson jwt/openssl adapter target did not define LONEJSON_WITH_JWT"
#endif
#ifndef LONEJSON_WITH_OIDC
#error "lonejson oidc adapter target did not define LONEJSON_WITH_OIDC"
#endif
#ifndef LONEJSON_WITH_OPENSSL
#error "lonejson openssl adapter target did not define LONEJSON_WITH_OPENSSL"
#endif

int main(void) {
    lonejson_error error;
    lonejson_auth_provider provider;
    lonejson_curl_parse curl_parse;
    lonejson_oidc_pkce pkce;

    lonejson_error_init(&error);
    lonejson_oidc_pkce_init(&pkce);
    (void)sizeof(curl_parse);
    if (lonejson_auth_provider_init_openssl(&provider, NULL, &error) != LONEJSON_STATUS_OK) {
        lonejson_oidc_pkce_cleanup(&pkce);
        return 1;
    }
    lonejson_oidc_pkce_cleanup(&pkce);
    return 0;
}
EOF

  adapter_pkg_config_flags="$(PKG_CONFIG_PATH="$pkg_config_path" pkg-config --cflags --libs lonejson-curl lonejson-oidc lonejson-openssl)"
  adapter_c89_flags="-std=c89 -Wall -Wextra -Werror -Werror=implicit-function-declaration"
  adapter_runtime_link_flags="$(target_development_runtime_flags "$target_id" "$preset" "$package_root/lib:$adapter_dependency_root/lib")"
  # shellcheck disable=SC2086
  run_with_target_path "$target_id" "$CC" "$adapter_source" $TARGET_CFLAGS $raw_compile_flags $adapter_c89_flags $adapter_pkg_config_flags $raw_link_flags $adapter_runtime_link_flags -o "$tmp_dir/pkg-config-adapter-consumer"

  adapter_cmake_source_dir="$tmp_dir/cmake-adapter-consumer"
  adapter_cmake_build_dir="$tmp_dir/cmake-adapter-build"
  mkdir -p "$adapter_cmake_source_dir"
  cp "$adapter_source" "$adapter_cmake_source_dir/main.c"
  cat >"$adapter_cmake_source_dir/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(lonejson_archive_adapter_consumer C)
include("${LONEJSON_DEVELOPMENT_RUNTIME_MODULE}")
get_filename_component(policy_dir "${LONEJSON_DEVELOPMENT_RUNTIME_MODULE}" DIRECTORY)
include("${policy_dir}/LonejsonLinkWarnings.cmake")
find_package(lonejson CONFIG REQUIRED COMPONENTS curl oidc openssl)
add_executable(lonejson_archive_adapter_consumer main.c)
target_compile_options(lonejson_archive_adapter_consumer PRIVATE
  -std=c89
  -Wall
  -Wextra
  -Werror
  -Werror=implicit-function-declaration)
target_link_libraries(lonejson_archive_adapter_consumer PRIVATE
  lonejson::lonejson
  lonejson::curl
  lonejson::oidc
  lonejson::openssl)
lonejson_configure_link_warnings(lonejson_archive_adapter_consumer)
lonejson_configure_development_runtime(lonejson_archive_adapter_consumer)
EOF

  if [[ "${target_id#*apple-darwin}" != "$target_id" ]]; then
    cmake_args=(
      -S "$adapter_cmake_source_dir"
      -B "$adapter_cmake_build_dir"
      -G Ninja
      -D "CMAKE_PREFIX_PATH=$cmake_prefix_path"
      -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
      -D "LONEJSON_DEVELOPMENT_RUNTIME_MODULE=$repo_root/cmake/LonejsonDevelopmentRuntime.cmake"
      -D "LONEJSON_DEVELOPMENT_RUNTIME_EXTRA_DIRS=$package_root/lib;$adapter_dependency_root/lib"
      -D "CMAKE_TOOLCHAIN_FILE=$repo_root/cmake/toolchains/arm64-apple-darwin.cmake"
      -D "LONEJSON_MACOS_DEPLOYMENT_TARGET=$darwin_deployment_target"
      -D "CMAKE_OSX_DEPLOYMENT_TARGET=$darwin_deployment_target"
      -D "LONEJSON_CPKT_ROOT=$adapter_dependency_root"
    )
  else
    cmake_args=(
      -S "$adapter_cmake_source_dir"
      -B "$adapter_cmake_build_dir"
      -G Ninja
      -D "CMAKE_PREFIX_PATH=$cmake_prefix_path"
      -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
      -D "LONEJSON_DEVELOPMENT_RUNTIME_MODULE=$repo_root/cmake/LonejsonDevelopmentRuntime.cmake"
      -D "LONEJSON_DEVELOPMENT_RUNTIME_EXTRA_DIRS=$package_root/lib;$adapter_dependency_root/lib"
      -D "CMAKE_TOOLCHAIN_FILE=$(target_toolchain_file "$target_id")"
    )
  fi
  run_with_target_path "$target_id" cmake "${cmake_args[@]}"
  run_with_target_path "$target_id" cmake --build "$adapter_cmake_build_dir"
  if [[ "$target_id" == x86_64-linux-gnu &&
        -f "$build_root/$preset/CMakeCache.txt" ]]; then
    "$repo_root/tests/test_bootlin_runtime.sh" \
      "$build_root/$preset" "$adapter_cmake_build_dir/lonejson_archive_adapter_consumer" --run
    "$tmp_dir/pkg-config-adapter-consumer"
  fi

  rm -rf "$tmp_dir"
}

require_curl_symbol() {
  local archive=$1
  local target_id=$2
  local preset=$3
  local tmp_dir package_root shared_lib static_lib

  tmp_dir="$(mktemp -d "$workspace_root/build/verify_release_archives.XXXXXX")"
  owned_workspaces+=("$tmp_dir")
  package_root="$(extract_archive "$archive" "$tmp_dir")"

  if [[ "${target_id#*apple-darwin}" != "$target_id" ]]; then
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson*.dylib' | sort | head -n 1)"
  else
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.so*' ! -type l | sort | head -n 1)"
  fi
  if [[ -z "$shared_lib" ]]; then
    printf 'missing packaged shared library in %s\n' "$archive" >&2
    exit 1
  fi
  static_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.a' | sort | head -n 1)"
  if [[ -z "$static_lib" ]]; then
    printf 'missing packaged static library in %s\n' "$archive" >&2
    exit 1
  fi

  "$repo_root/scripts/check_curl_abi_symbols.sh" \
    "$repo_root" \
    "$build_root/$preset" \
    "$target_id" \
    "$shared_lib" \
    "$static_lib" \
    "$archive"

  rm -rf "$tmp_dir"
}

require_jwt_symbol() {
  local archive=$1
  local target_id=$2
  local preset=$3
  local tmp_dir package_root shared_lib static_lib

  tmp_dir="$(mktemp -d "$workspace_root/build/verify_release_archives.XXXXXX")"
  owned_workspaces+=("$tmp_dir")
  package_root="$(extract_archive "$archive" "$tmp_dir")"

  if [[ "${target_id#*apple-darwin}" != "$target_id" ]]; then
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson*.dylib' | sort | head -n 1)"
  else
    shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.so*' ! -type l | sort | head -n 1)"
  fi
  if [[ -z "$shared_lib" ]]; then
    printf 'missing packaged shared library in %s\n' "$archive" >&2
    exit 1
  fi
  static_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.a' | sort | head -n 1)"
  if [[ -z "$static_lib" ]]; then
    printf 'missing packaged static library in %s\n' "$archive" >&2
    exit 1
  fi

  "$repo_root/scripts/check_jwt_abi_symbols.sh" \
    "$repo_root" \
    "$build_root/$preset" \
    "$target_id" \
    "$shared_lib" \
    "$static_lib" \
    "$archive"

  rm -rf "$tmp_dir"
}

require_command cmake
require_command pkg-config
require_command tar

verified=0
while read -r _hash artifact; do
  target_id="$(archive_target_id "$artifact")"
  [[ -n "$target_id" ]] || continue
  archive="$dist_dir/$artifact"
  if [[ ! -f "$archive" ]]; then
    printf 'checksum-listed archive missing under dist: %s\n' "$artifact" >&2
    exit 1
  fi
  preset="$(target_preset "$target_id")"
  require_archive_contract "$archive" "$target_id" "$preset"
  require_curl_symbol "$archive" "$target_id" "$preset"
  require_jwt_symbol "$archive" "$target_id" "$preset"
  require_archive_consumer_metadata "$archive" "$target_id" "$preset"
  verified=$((verified + 1))
done <"$checksums"

if [[ "$verified" -eq 0 ]]; then
  printf 'release archive verification failed: no binary SDK archives listed in %s\n' \
    "$checksums" >&2
  exit 1
fi

printf 'release archive verification passed: %s (%d binary SDK archive(s))\n' \
  "$checksums" "$verified"
