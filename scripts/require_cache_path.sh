#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
fail() { printf 'cache must belong to this checkout build/ or .cache/, or the declared global cpkt cache: %s\n' "${1:-}" >&2; exit 1; }
physical_path() {
  local path=$1 suffix= part
  [[ $path == /* && $path != *'/../'* && $path != */.. && $path != *'/./'* ]] || fail "$path"
  while [[ ! -e $path ]]; do
    [[ ! -L $path ]] || fail "$1"
    part=${path##*/}; suffix="/$part$suffix"; path=${path%/*}
    [[ -n $path ]] || path=/
  done
  [[ -d $path ]] || fail "$1"
  printf '%s%s\n' "$(CDPATH= cd -- "$path" && pwd -P)" "$suffix"
}
local_only=0
if [[ ${1:-} == --local ]]; then local_only=1; shift; fi
[[ $# == 1 ]] || fail 'usage: require_cache_path.sh [--local] ABSOLUTE_CACHE'
path=$(physical_path "$1")
case $path in "$root/build/"*|"$root/.cache/"*) exit 0 ;; esac
[[ $local_only == 0 ]] || fail "$1"
for base in "${HOME:-}/.cache/cpkt" "${XDG_CACHE_HOME:-${HOME:-}/.cache}/cpkt"; do
  global=$(physical_path "$base")
  case $path in
    "$global"|"$global/"*)
      # A cache selected through XDG or symlinks cannot borrow another checkout.
      ancestor=$global
      while [[ ! -d $ancestor ]]; do ancestor=${ancestor%/*}; done
      if git -C "$ancestor" rev-parse --show-toplevel >/dev/null 2>&1; then fail "$1"; fi
      exit 0 ;;
  esac
done
fail "$1"
