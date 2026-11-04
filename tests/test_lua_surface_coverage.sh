#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
exports=$(sed -n 's/.*{"\([A-Za-z0-9_]*\)",[[:space:]]*ljlua_.*/\1/p' \
  "$repo_root/src/lua/lonejson_lua_module.inc.h" | sort -u)
[[ -n "$exports" ]] || { printf 'Lua export inventory is empty\n' >&2; exit 1; }
status=0
while IFS= read -r name; do
  case "$name" in _test_*|monotonic_ns) continue ;; esac
  if ! grep -wF -- "$name" "$repo_root/tests/test_lua.lua" "$repo_root/tests/test_lua_fuzz.lua" >/dev/null; then
    printf 'Lua public C binding export missing test/fuzz coverage: %s\n' "$name" >&2
    status=1
  fi
done <<<"$exports"
exit "$status"
