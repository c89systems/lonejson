#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bundle_root="$("${repo_root}/scripts/detect_cpkt_bundle.sh")"
build_dir="${repo_root}/build/host-curl"
output_dir="${repo_root}/examples/bin"

# These are development executables.  Build them through the same CMake target
# helper as tests, fuzzers, and ordinary examples so native Linux execution
# selects the pinned Bootlin loader and private runtime paths directly.
configure_args=()
if [[ ${LONEJSON_RELEASE_TARGETS+x} ]]; then
  configure_args+=("-DLONEJSON_RELEASE_TARGETS=$LONEJSON_RELEASE_TARGETS")
fi
cmake --preset host-curl -S "${repo_root}" \
  -D "LONEJSON_CPKT_ROOT=${bundle_root}" "${configure_args[@]}"
cmake --build "${build_dir}" --target example_curl_get example_curl_put
mkdir -p "${output_dir}"
cp "${build_dir}/example_curl_get" "${output_dir}/curl_get"
cp "${build_dir}/example_curl_put" "${output_dir}/curl_put"
