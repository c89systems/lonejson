#!/usr/bin/env bash
set -euo pipefail
script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
lifecycle_scripts_dir=${CPKT_LIFECYCLE_SCRIPTS_DIR:-$script_dir/../tools/lifecycle/scripts}
source "$lifecycle_scripts_dir/require-host-bash.sh"
# The upstream collection owns Linux and osxcross. Native Apple discovery is
# this project's small extension; it does not provision workstation tools.
if [[ "$("${UNAME:-uname}" -s)" == Darwin && ${2:-} == arm64-apple-darwin ]]; then
  die() { printf 'lonejson Apple tools: %s\n' "$*" >&2; exit 1; }
  source "$script_dir/cpkt-native-apple.sh"
  case ${1:-} in
    discover|ensure) print_darwin_target ;;
    env)
      description=$(print_darwin_target)
      while IFS='=' read -r key value; do
        case $key in
          source|root|sysroot|cc|cxx|ld|ar|ranlib|strip|nm|otool|install_name_tool)
            printf 'export CPKT_TOOLCHAIN_%s=%q\n' "${key^^}" "$value" ;;
        esac
        case $key in
          cc|cxx|ld|ar|ranlib|strip|nm) printf 'export %s=%q\n' "${key^^}" "$value" ;;
          sysroot) printf 'export CPKT_SYSROOT=%q\n' "$value" ;;
        esac
      done <<<"$description"
      printf 'export CPKT_TARGET=arm64-apple-darwin\n'
      ;;
    *) die 'usage: cpkt-toolchains.sh <discover|ensure|env> arm64-apple-darwin' ;;
  esac
else
  case ${1:-} in
    discover|ensure|env)
      cache=${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-${HOME:?HOME or XDG_CACHE_HOME or CPKT_TOOLCHAIN_CACHE required}/.cache}/cpkt/toolchains}
      "$script_dir/require_cache_path.sh" "$cache"
      ;;
  esac
  exec "$lifecycle_scripts_dir/cpkt-toolchains.sh" "$@"
fi
