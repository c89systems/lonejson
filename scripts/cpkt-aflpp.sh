#!/usr/bin/env bash
set -euo pipefail
script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
cache=${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-${HOME:?HOME or XDG_CACHE_HOME or CPKT_TOOLCHAIN_CACHE required}/.cache}/cpkt/toolchains}
"$script_dir/require_cache_path.sh" "$cache"
exec "${CPKT_LIFECYCLE_SCRIPTS_DIR:-$script_dir/../tools/lifecycle/scripts}/cpkt-aflpp.sh" "$@"
