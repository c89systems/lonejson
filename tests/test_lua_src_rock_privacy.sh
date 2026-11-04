#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Rationale: LuaRocks release artifacts are public packages and must reject
# local source URLs or repository paths before release.

repo_root=$1
tmp_dir=$(mktemp -d "$workspace_root/build/test_lua_src_rock_privacy.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

rockspec="$tmp_dir/lonejson-0.0.0-1.rockspec"
source_dir="$tmp_dir/source/lonejson-0.0.0"
source_tarball="$tmp_dir/lonejson-0.0.0.tar.gz"
bad_rock="$tmp_dir/lonejson-0.0.0-1.src.rock"
failure_log="$tmp_dir/failure.log"

mkdir -p "$source_dir"
printf '%s\n' '0.0.0' >"$source_dir/VERSION"
tar -czf "$source_tarball" -C "$tmp_dir/source" lonejson-0.0.0

cat >"$rockspec" <<EOF
package = "lonejson"
version = "0.0.0-1"
source = {
  url = "file://${repo_root}/dist/.pack/lonejson-0.0.0.tar.gz",
}
description = {
  summary = "privacy regression fixture",
  license = "MIT",
}
dependencies = {
  "lua >= 5.5, < 5.6",
}
build = {
  type = "none",
}
EOF

zip -q -j "$bad_rock" "$rockspec" "$source_tarball"

if "$repo_root/scripts/smoke_lua_src_rock.sh" "$bad_rock" >"$failure_log" 2>&1; then
  printf 'expected smoke_lua_src_rock.sh to reject local file URL\n' >&2
  exit 1
fi

grep -Eq 'absolute local file:// URL|repository path' "$failure_log"

cp "$rockspec" "$tmp_dir/original.rockspec"
for source_url in 'file://lonejson-lua-0.0.0.tar.gz' ''; do
  sed "s|file://${repo_root}/dist/.pack/lonejson-0.0.0.tar.gz|$source_url|" \
    "$tmp_dir/original.rockspec" >"$rockspec"
  rm -f "$bad_rock"
  zip -q -j "$bad_rock" "$rockspec" "$source_tarball"
  if "$repo_root/scripts/smoke_lua_src_rock.sh" "$bad_rock" >"$failure_log" 2>&1; then
    printf 'source rock accepted a missing or local source URL\n' >&2; exit 1
  fi
  grep -F 'public HTTPS source URL' "$failure_log" >/dev/null
done
