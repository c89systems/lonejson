#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir=${LONEJSON_E2E_BUILD_DIR:-$repo_root/build/host-curl}
nginx_https_port=${LONEJSON_NGINX_HTTPS_E2E_PORT:-8443}
ca_file=${LONEJSON_CURL_E2E_CAINFO:-$repo_root/build/devenv/credentials/server.crt}

[[ -x "$build_dir/lonejson_curl_rewind_e2e" ]] || {
  printf 'missing curl e2e executable; run make e2e-build\n' >&2; exit 1;
}

LONEJSON_CURL_E2E_CAINFO="$ca_file" \
LONEJSON_CURL_E2E_GET_URL="https://localhost:${nginx_https_port}/variants/ingest-target.json" \
  "${repo_root}/examples/bin/curl_get"
LONEJSON_CURL_E2E_CAINFO="$ca_file" \
LONEJSON_CURL_E2E_PUT_URL="https://localhost:${nginx_https_port}/ingest" \
  "${repo_root}/examples/bin/curl_put"

# Direct example execution must discover the generated CA without the wrapper
# supplying it, including the documented example and build working directories.
for directory in "$repo_root" "$repo_root/examples" "$repo_root/examples/bin"; do
  (
    cd "$directory"
    env -u LONEJSON_CURL_E2E_CAINFO \
      LONEJSON_CURL_E2E_GET_URL="https://localhost:${nginx_https_port}/variants/ingest-target.json" \
      "$repo_root/examples/bin/curl_get"
    env -u LONEJSON_CURL_E2E_CAINFO \
      LONEJSON_CURL_E2E_PUT_URL="https://localhost:${nginx_https_port}/ingest" \
      "$repo_root/examples/bin/curl_put"
  )
done

# Exercise the prepared real-curl replay client.
"$build_dir/lonejson_curl_rewind_e2e" \
  "https://localhost:${nginx_https_port}" "$ca_file"
