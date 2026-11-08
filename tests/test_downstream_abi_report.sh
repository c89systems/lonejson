#!/usr/bin/env bash
set -euo pipefail
repo_root=$1
mkdir -p "$repo_root/build"
workspace=$(mktemp -d "$repo_root/build/test-downstream-abi.XXXXXX")
trap 'rm -rf "$workspace"' EXIT
fixture="$workspace/repo"
mkdir -p "$fixture/include"

write_abi() {
  printf '#define LONEJSON_ABI_VERSION %s\n' "$1" >"$fixture/include/lonejson.h"
}
commit_abi() {
  git -C "$fixture" add include/lonejson.h
  git -C "$fixture" -c user.name=Fixture -c user.email=fixture@example.invalid \
    -c commit.gpgsign=false commit -qm 'test(fixture): set ABI'
}
report() {
  cmake "-DLONEJSON_ROOT=$fixture" -P "$repo_root/cmake/report_downstream_abi.cmake" \
    >"$workspace/report" 2>&1
}
require() { grep -F -- "$1" "$workspace/report" >/dev/null; }

# Nested source archives must not borrow this checkout's release tags.
write_abi 27
report
require 'release comparison unavailable'
require 'liblockdc, librotmos'
require 'libcai: update LoneJSON and liblockdc'
require 'libailo: update LoneJSON and librotmos; rebuild.'
require 'vectis: update LoneJSON, liblockdc, libcai, librotmos and libailo; rebuild last.'

git -c init.defaultBranch=trunk init -q "$fixture"
write_abi 26
commit_abi
report
require 'release comparison unavailable'
git -C "$fixture" tag v1.0.0
report
require 'ABI 26 is unchanged from v1.0.0'
! grep -F 'Downstream upgrade/rebuild order' "$workspace/report"

# An ABI delta is advisory and must preserve the caller's dirty worktree.
write_abi 27
git -C "$fixture" status --porcelain >"$workspace/before"
report
require 'ABI 26 (v1.0.0) -> 27 (working tree)'
require 'coordinated downstream upgrade required'
require 'Advisory only'
grep -E '^--   [1-3]\.' "$workspace/report" >"$workspace/order"
[[ $(wc -l <"$workspace/order") -eq 3 ]]
sed -n '1p' "$workspace/order" | grep -F 'liblockdc, librotmos' >/dev/null
sed -n '2p' "$workspace/order" | grep -F 'libcai:' >/dev/null
sed -n '3p' "$workspace/order" | grep -F 'vectis:' >/dev/null
git -C "$fixture" status --porcelain >"$workspace/after"
cmp "$workspace/before" "$workspace/after"

# Version order is numeric; prerelease tags do not replace stable releases.
git -C "$fixture" tag v1.9.0
commit_abi
git -C "$fixture" tag v1.10.0
git -C "$fixture" tag v99.0.0-rc1
write_abi 28
report
require 'ABI 27 (v1.10.0) -> 28 (working tree)'

write_abi invalid
if report; then
  printf 'downstream ABI report accepted an invalid working ABI\n' >&2
  exit 1
fi
require 'No numeric LONEJSON_ABI_VERSION'
write_abi 28
cmake -DCMAKE_DISABLE_FIND_PACKAGE_Git=TRUE "-DLONEJSON_ROOT=$fixture" \
  -P "$repo_root/cmake/report_downstream_abi.cmake" >"$workspace/report" 2>&1
require 'release comparison unavailable'
require 'vectis:'
