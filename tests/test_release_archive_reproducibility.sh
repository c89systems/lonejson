#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/archive-reproducibility.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
fail() { printf '%s\n' "$*" >&2; exit 1; }

# Exercise source packaging itself, with identical bytes under different mtimes
# and umasks. A small explicit source manifest avoids an unrelated rebuild.
fixture="$work/source"
mkdir -p "$fixture/scripts" "$fixture/cmake"
cp "$repo_root/scripts/"{stage_release_sources,require_build_workspace}.sh "$fixture/scripts/"
if [[ -f "$repo_root/scripts/archive_release_tree.sh" ]]; then
  cp "$repo_root/scripts/archive_release_tree.sh" "$fixture/scripts/"
fi
cp "$repo_root/cmake/"{lonejson_dist_dir,lonejson_build_workspace}.cmake "$fixture/cmake/"
printf 'LICENSE\nrun.sh\nRELEASE_MANIFEST\n' >"$fixture/RELEASE_MANIFEST"
printf 'license\n' >"$fixture/LICENSE"
printf '#!/bin/sh\nexit 0\n' >"$fixture/run.sh"
chmod 755 "$fixture/run.sh"
for mask in 022 077; do
  touch -t "200${mask: -1}01010000" "$fixture/LICENSE" "$fixture/run.sh"
  (umask "$mask"; cmake -D "LONEJSON_ROOT=$fixture" -D LONEJSON_VERSION=1.2.3 \
    -P "$repo_root/cmake/package_source.cmake")
  cp "$fixture/dist/lonejson-1.2.3.tar.gz" "$work/source-$mask.tar.gz"
done
cmp "$work/source-022.tar.gz" "$work/source-077.tar.gz" || fail 'source archive metadata is not reproducible'
mkdir "$work/extracted"
tar -xzf "$work/source-077.tar.gz" -C "$work/extracted"
[[ -x "$work/extracted/lonejson-1.2.3/run.sh" ]] || fail 'source archive lost executable mode'

# The public Make file producer owns separate staging here; normal checkout
# packaging and native build state remain available to subsequent tests.
stage="$work/lua-stage"
lua_archive="$work/lonejson-lua-1.2.3.tar.gz"
for mask in 022 077; do
  rm -rf -- "$stage"
  rm -f "$lua_archive"
  (umask "$mask"; make -s -C "$repo_root" "$lua_archive" RELEASE_VERSION=1.2.3 \
    DIST_DIR="$work" RELEASE_PACK_DIR="$stage" \
    RELEASE_PACK_STAGE_DIR="$stage/lonejson-1.2.3")
  cp "$lua_archive" "$work/lua-$mask.tar.gz"
done
cmp "$work/lua-022.tar.gz" "$work/lua-077.tar.gz" || fail 'Lua source archive metadata is not reproducible'
make -s -n -C "$repo_root" "$lua_archive" RELEASE_VERSION=1.2.3 \
  DIST_DIR="$work" RELEASE_PACK_DIR="$stage" \
  RELEASE_PACK_STAGE_DIR="$stage/lonejson-1.2.3" >"$work/reuse"
if grep -E 'archive_release_tree|stage_lua_rock_sources' "$work/reuse"; then
  fail 'archive normalization invalidated the reusable source stage'
fi

spec="$work/lonejson-1.2.3-1.rockspec"
printf 'package = "lonejson"\n' >"$spec"
for mask in 022 077; do
  touch -t "200${mask: -1}01010000" "$spec" "$lua_archive"
  (umask "$mask"; "$repo_root/scripts/package_lua_src_rock.sh" \
    "$work/rock-$mask.src.rock" "$spec" "$lua_archive")
done
cmp "$work/rock-022.src.rock" "$work/rock-077.src.rock" || fail 'source rock metadata is not reproducible'
mkdir "$work/directory"
ln -s directory "$work/link"
for destination in "$work/directory" "$work/link"; do
  if "$repo_root/scripts/package_lua_src_rock.sh" "$destination" "$spec" "$lua_archive" >"$work/destination" 2>&1; then
    fail 'source rock writer accepted a directory or symlink destination'
  fi
  grep -F 'unsafe source rock destination' "$work/destination" >/dev/null
done

# The SDK and Darwin smoke bundle share the same tree archiver. Preserve links,
# empty directories, and executable modes while ordering entries consistently.
tree="$work/fixture-tree"
for mask in 022 077; do
  rm -rf -- "$tree"
  (umask "$mask"
    mkdir -p "$tree/empty"
    if [[ $mask == 022 ]]; then touch "$tree/z" "$tree/a"; else touch "$tree/a" "$tree/z"; fi
    printf '#!/bin/sh\nexit 0\n' >"$tree/run.sh"
    chmod 755 "$tree/run.sh"
    ln -s run.sh "$tree/alias"
    for format in tar.gz zip; do
      "$repo_root/scripts/archive_release_tree.sh" "$tree" "$work/tree-$mask.$format"
    done)
done
for format in tar.gz zip; do
  cmp "$work/tree-022.$format" "$work/tree-077.$format" || fail "$format tree metadata is not reproducible"
  mkdir "$work/unpack-$format"
  cmake -E chdir "$work/unpack-$format" cmake -E tar xf "$work/tree-077.$format"
  [[ -L "$work/unpack-$format/fixture-tree/alias" && -d "$work/unpack-$format/fixture-tree/empty" && \
     -x "$work/unpack-$format/fixture-tree/run.sh" ]] || fail "$format lost a public payload property"
done
tar --numeric-owner -tvzf "$work/tree-077.tar.gz" >"$work/metadata"
if grep -vE '^[^ ]+ +0/0 ' "$work/metadata"; then fail 'archive recorded workstation ownership'; fi
if "$repo_root/scripts/archive_release_tree.sh" "$repo_root/src" "$work/unsafe.tar.gz" >"$work/unsafe" 2>&1; then
  fail 'archiver accepted a source directory as mutable staging'
fi
grep -F 'refusing unsafe workspace' "$work/unsafe" >/dev/null
printf 'Source, Lua archive and source rock reproducibility passed\n'
