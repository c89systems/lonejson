#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
mkdir -p "$repo_root/build"
tmp_dir=$(mktemp -d "$repo_root/build/podman-test.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT
mkdir -p "$tmp_dir/bin"
for checkout in first second; do
  mkdir -p "$tmp_dir/$checkout/scripts"
  cp "$repo_root/scripts/devenv.sh" "$repo_root/scripts/render_devenv.cmake" "$repo_root/scripts/require_build_workspace.sh" "$tmp_dir/$checkout/scripts/"
  cp "$repo_root/devenv.yaml.in" "$tmp_dir/$checkout/"
done
export PODMAN_TEST_LOG="$tmp_dir/events"
cat >"$tmp_dir/bin/podman" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
if [[ "$1" == info ]]; then
  printf '%s\n' "${PODMAN_TEST_ROOTLESS:-true}"
  exit
fi
if [[ "$1 $2" == 'pod exists' ]]; then
  exit "${PODMAN_TEST_EXISTS_STATUS:-1}"
fi
printf '%s\n' "$*" >>"$PODMAN_TEST_LOG"
if [[ "$1 $2" == 'kube play' ]]; then
  [[ "${PODMAN_TEST_PLAY_FAIL:-0}" != 1 ]] || exit 43
fi
if [[ "$1 $2" == 'kube down' && "${PODMAN_TEST_DOWN_FAIL:-0}" == 1 ]]; then
  exit 42
fi
EOF
chmod +x "$tmp_dir/bin/podman"
export PATH="$tmp_dir/bin:$PATH"
first="$tmp_dir/first"
second="$tmp_dir/second"
# Rendering works outside the checkout and never invokes a container engine.
(cd "$tmp_dir"; "$first/scripts/devenv.sh" render >/dev/null)
"$second/scripts/devenv.sh" render >/dev/null
first_manifest="$first/build/devenv/devenv.yaml"
second_manifest="$second/build/devenv/devenv.yaml"
[[ "$(stat -c %a "$first_manifest")" == 600 ]]
first_name=$(sed -n 's/^  name: "\(.*\)"$/\1/p' "$first_manifest")
second_name=$(sed -n 's/^  name: "\(.*\)"$/\1/p' "$second_manifest")
[[ "$first_name" != "$second_name" ]]
grep -F "$first/devenv" "$first_manifest" >/dev/null
[[ ! -e "$first/devenv.yaml" ]]
if "$first/scripts/devenv.sh" render >"$tmp_dir/re-render" 2>&1; then
  printf 'render overwrote the saved teardown manifest\n' >&2; exit 1
fi
"$first/scripts/devenv.sh" ps
ln -s "$first" "$tmp_dir/checkout-alias"
"$tmp_dir/checkout-alias/scripts/devenv.sh" ps
"$first/scripts/devenv.sh" logs --tail 10
grep -F "pod ps --filter name=^${first_name}$" "$PODMAN_TEST_LOG" >/dev/null
[[ "$(grep -Fcx "pod ps --filter name=^${first_name}$" "$PODMAN_TEST_LOG")" == 2 ]]
grep -F "pod logs --tail 10 $first_name" "$PODMAN_TEST_LOG" >/dev/null
if PODMAN_TEST_ROOTLESS=false "$first/scripts/devenv.sh" ps >"$tmp_dir/rootful" 2>&1; then
  printf 'rootful Podman accepted\n' >&2; exit 1
fi
grep -F 'rootless Podman is required' "$tmp_dir/rootful" >/dev/null
mkdir -p "$first/build/devenv/state/probe"
printf owned >"$first/build/devenv/state/probe/data"
if PODMAN_TEST_DOWN_FAIL=1 "$first/scripts/devenv.sh" reset; then
  printf 'reset ignored a failed pod teardown\n' >&2; exit 1
fi
[[ -f "$first/build/devenv/state/probe/data" ]]
LONEJSON_OIDC_E2E_PORT=24000 "$first/scripts/devenv.sh" reset
grep -F "kube down $first_manifest" "$PODMAN_TEST_LOG" >/dev/null
[[ ! -e "$first/build/devenv" ]]
"$first/scripts/devenv.sh" reset
if PODMAN_TEST_EXISTS_STATUS=0 "$first/scripts/devenv.sh" reset >"$tmp_dir/orphan" 2>&1; then
  printf 'reset accepted a live pod without its teardown manifest\n' >&2; exit 1
fi
if PODMAN_TEST_EXISTS_STATUS=125 "$first/scripts/devenv.sh" reset >"$tmp_dir/engine" 2>&1; then
  printf 'reset ignored a container-engine failure\n' >&2; exit 1
fi
# Reject malformed/colliding/privileged ports before creating a manifest.
for port in 0 80 65536 abc '8443;touch sentinel'; do
  if LONEJSON_OIDC_E2E_PORT="$port" "$first/scripts/devenv.sh" render >"$tmp_dir/port" 2>&1; then
    printf 'invalid port accepted: %s\n' "$port" >&2; exit 1
  fi
  [[ ! -e "$first_manifest" ]]
done
if LONEJSON_OIDC_E2E_PORT=8443 "$first/scripts/devenv.sh" render >"$tmp_dir/collision" 2>&1; then
  printf 'duplicate host ports accepted\n' >&2; exit 1
fi
LONEJSON_OIDC_E2E_PORT=24000 "$first/scripts/devenv.sh" render >/dev/null
grep -F 'hostPort: 24000' "$first_manifest" >/dev/null
# Startup probes use bounded HTTP requests and propagate failures.
cat >"$first/scripts/ensure_test_certs.sh" <<'SH'
#!/usr/bin/env bash
exit 0
SH
cat >"$first/scripts/ensure_large_fixtures.sh" <<'SH'
#!/usr/bin/env bash
mkdir -p "$3"
SH
cat >"$tmp_dir/bin/curl" <<'SH'
#!/usr/bin/env bash
exit 0
SH
chmod +x "$first/scripts/ensure_test_certs.sh" "$first/scripts/ensure_large_fixtures.sh" "$tmp_dir/bin/curl"
trap 'rm -rf "$tmp_dir"' EXIT
LUA=/bin/true "$first/scripts/devenv.sh" up >"$tmp_dir/startup"
timeout 5 "$first/scripts/devenv.sh" down
grep -F 'kube play --network pasta' "$PODMAN_TEST_LOG" >/dev/null
for keep in 0 1; do
  "$first/scripts/devenv.sh" reset
  : >"$PODMAN_TEST_LOG"
  if LUA=/bin/true PODMAN_TEST_PLAY_FAIL=1 LONEJSON_E2E_KEEP_DEVSERVICES="$keep" \
      "$first/scripts/devenv.sh" up >"$tmp_dir/play-failure" 2>&1; then
    printf 'startup swallowed a failed kube play\n' >&2; exit 1
  fi
  grep -F 'PKT_DIAGNOSTIC_BEGIN' "$tmp_dir/play-failure" >/dev/null
  if [[ "$keep" == 0 ]]; then
    grep -F 'kube down' "$PODMAN_TEST_LOG" >/dev/null
  else
    ! grep -F 'kube down' "$PODMAN_TEST_LOG"
  fi
done
# Validate path escaping without third-party YAML dependencies.
odd="$tmp_dir/checkout with \"quotes\""
mkdir -p "$odd/scripts"
cp "$first/scripts/render_devenv.cmake" "$first/scripts/require_build_workspace.sh" "$odd/scripts/"
cp "$repo_root/devenv.yaml.in" "$odd/"
cmake -P "$odd/scripts/render_devenv.cmake" >/dev/null
escaped=${odd//\"/\\\"}
grep -F "hostPath: {path: \"$escaped/devenv\"" "$odd/build/devenv/devenv.yaml" >/dev/null
[[ $(grep -c 'hostPath: {path:' "$odd/build/devenv/devenv.yaml") == 5 ]]
printf '%s\n' 'Podman renderer, isolation, rootless policy, ports, and reset tests passed.'
