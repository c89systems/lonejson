#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
generated="$repo_root/build/devenv"
pod_name=$(sed -n 's/^  name: "\(.*\)"$/\1/p' "$generated/devenv.yaml")
[[ -n "$pod_name" ]]
for service in oauth2 oauth2-tls api-fixture sink nginx; do
  # Exercise each image's actual user namespace and effective writer identity.
  podman exec "$pod_name-$service" sh -c \
    'mkdir -p /state/ownership /tmp/ownership; printf state > /state/ownership/probe; printf temp > /tmp/ownership/probe'
  for area in state tmp; do
    [[ "$(stat -c %u "$generated/$area/$service/ownership/probe")" == "$(id -u)" ]]
  done
done
printf '%s\n' 'All five images wrote state and temporary files as the host user.'

# Exercise the real YAML parser with paths that JSON's surrogate escaping
# cannot represent in YAML. The unique fixture pod is never started.
fixture=$(mktemp -d "$generated/tmp/yaml-😀.XXXXXX")
trap 'rm -rf -- "$fixture"' EXIT
mkdir -p "$fixture/scripts"
cp "$repo_root/scripts/render_devenv.cmake" "$repo_root/scripts/require_build_workspace.sh" "$fixture/scripts/"
cp "$repo_root/devenv.yaml.in" "$fixture/"
cmake -P "$fixture/scripts/render_devenv.cmake" >/dev/null
podman kube down "$fixture/build/devenv/devenv.yaml"
printf '%s\n' 'Podman parsed the non-BMP checkout manifest.'
