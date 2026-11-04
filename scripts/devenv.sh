#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
generated="$repo_root/build/devenv"
manifest="$generated/devenv.yaml"
pod_name="lonejson-e2e-$(printf %s "$repo_root" | sha256sum | cut -c1-12)"
action=${1:-}
[[ $# -gt 0 ]] && shift
case "$action" in
  up|down|reset|render|ps|logs) ;;
  *) printf 'usage: %s {up|down|reset|render|ps|logs} [log-options]\n' "$0" >&2; exit 2 ;;
esac
if [[ "$action" != logs && $# != 0 ]]; then
  printf '%s does not accept arguments\n' "$action" >&2
  exit 2
fi

"$repo_root/scripts/require_build_workspace.sh" "$generated"
if [[ "$action" == render ]]; then
  # Never change the teardown manifest beneath a live pod.
  if [[ -e "$manifest" ]]; then
    printf 'manifest already exists; run make dev-reset before rendering again\n' >&2
    exit 1
  fi
  exec cmake -P "$repo_root/scripts/render_devenv.cmake"
fi
command -v podman >/dev/null || { printf 'rootless Podman is required\n' >&2; exit 1; }
[[ "$(podman info --format '{{.Host.Security.Rootless}}')" == true ]] || {
  printf 'rootless Podman is required; run as the ordinary host user\n' >&2; exit 1;
}

down() {
  if [[ -f "$manifest" ]]; then
    podman kube down "$manifest"
  else
    if podman pod exists "$pod_name"; then
      printf 'pod %s exists but its teardown manifest is missing: %s\n' "$pod_name" "$manifest" >&2
      return 1
    else
      # Only exit 1 means absent; engine failures must block deletion.
      local status=$?
      [[ "$status" == 1 ]] || return "$status"
    fi
  fi
}
diagnose() {
  printf 'Podman e2e failure: pod=%s manifest=%s\n' "$pod_name" "$manifest" >&2
  podman pod ps --filter "name=^${pod_name}$" >&2 || true
  podman pod logs --tail 120 "$pod_name" >&2 || true
  printf '%s\n' PKT_DIAGNOSTIC_BEGIN surface=dev-up phase=service-readiness \
    status=failed class=e2e-service reason=local-podman-service-not-ready \
    'next=run make dev-logs; check endpoints and LONEJSON_*_E2E_PORT overrides' \
    PKT_DIAGNOSTIC_END >&2
}
wait_url() {
  local url=$1 deadline=$((SECONDS + 90))
  while (( SECONDS < deadline )); do
    if curl -fsS --max-time 3 --cacert "$generated/credentials/server.crt" "$url" >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
  done
  printf 'timed out waiting for %s\n' "$url" >&2
  return 1
}

case "$action" in
  down) down ;;
  reset)
    down
    rm -rf -- "$generated"
    ;;
  ps) podman pod ps --filter "name=^${pod_name}$" ;;
  logs) podman pod logs "$@" "$pod_name" ;;
  up)
    command -v curl >/dev/null || { printf 'dev-up requires curl\n' >&2; exit 1; }
    # Validate all inputs before touching an existing pod or teardown manifest.
    cmake -D CHECK_ONLY=ON -P "$repo_root/scripts/render_devenv.cmake"
    if [[ -e "$manifest" ]]; then
      down
    fi
    cmake -P "$repo_root/scripts/render_devenv.cmake" >/dev/null
    trap 'status=$?; if (( status != 0 )); then diagnose; if [[ "${LONEJSON_E2E_KEEP_DEVSERVICES:-0}" != 1 ]]; then down || true; fi; fi; exit "$status"' EXIT
    umask 077
    for service in oauth2 oauth2-tls api-fixture sink nginx; do
      mkdir -p "$generated/state/$service" "$generated/tmp/$service"
    done
    mkdir -p "$generated/logs" "$generated/credentials"
    "$repo_root/scripts/ensure_test_certs.sh"
    lua_bin="${LUA:-$("$repo_root/scripts/resolve_lua55.sh")}"
    "$repo_root/scripts/ensure_large_fixtures.sh" "$lua_bin" \
      "$repo_root/scripts/generate_large_fixtures.lua" "$generated/state/nginx/generated/variants"
    # A private rootless network keeps teardown independent of other checkouts'
    # shared bridge network namespace and its user mapping.
    podman kube play --network pasta "$manifest"
    nginx_port=${LONEJSON_NGINX_HTTPS_E2E_PORT:-8443}
    oidc_port=${LONEJSON_OIDC_E2E_PORT:-18443}
    api_port=${LONEJSON_API_FIXTURE_E2E_PORT:-18080}
    wait_url "https://localhost:$nginx_port/variants/ingest-target.json"
    wait_url "https://localhost:$oidc_port/.well-known/openid-configuration/default"
    wait_url "http://127.0.0.1:$api_port/health"
    # Probe the upload sink through nginx rather than assuming container order.
    curl -fsS --max-time 3 --cacert "$generated/credentials/server.crt" \
      -X POST -H 'Content-Type: application/json' --data '{}' "https://localhost:$nginx_port/ingest" >/dev/null
    printf 'lonejson e2e ready: pod=%s\n  manifest=%s\n  state=%s\n  CA=%s\n' \
      "$pod_name" "$manifest" "$generated" "$generated/credentials/server.crt"
    printf '  HTTPS=https://localhost:%s/ OIDC=https://localhost:%s/default API=http://127.0.0.1:%s/health\n' \
      "$nginx_port" "$oidc_port" "$api_port"
    sed -n 's/^      image: /  image=/p' "$manifest" | sort -u
    ;;
esac
