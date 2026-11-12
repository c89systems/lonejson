#!/usr/bin/env bash
set -euo pipefail
root=$(cd "${1:?source required}" && pwd -P)
src_tree=${2:?src tree required}
include_tree=${3:?include tree required}
lua_tree=${4:?lua tree required}
[[ $(git -C "$root" rev-parse --show-toplevel 2>/dev/null || true) == "$root" ]] || {
  printf 'benchmark reference reconstruction requires an owning Git checkout\n' >&2; exit 1;
}
for tree in "$src_tree" "$include_tree" "$lua_tree"; do
  [[ "$tree" =~ ^([0-9a-f]{40}|[0-9a-f]{64})$ && $(git -C "$root" cat-file -t "$tree" 2>/dev/null || true) == tree ]] || {
    printf 'frozen implementation Git tree is unavailable: %s; fetch its source objects\n' "$tree" >&2; exit 1;
  }
done
parent="$root/build/benchmark-reference"
reference="$parent/$src_tree-$include_tree-$lua_tree/source"
mkdir -p "$parent"
if [[ ! -d "$reference" ]]; then
  staging=$(mktemp -d "$parent/prepare.XXXXXX")
  trap 'rm -rf -- "$staging"' EXIT
  mkdir -p "$staging/source"
  git -C "$root" archive HEAD | tar -xf - -C "$staging/source"
  rm -rf -- "$staging/source/src" "$staging/source/include" "$staging/source/lua"
  git -C "$root" archive --prefix=src/ "$src_tree" | tar -xf - -C "$staging/source"
  git -C "$root" archive --prefix=include/ "$include_tree" | tar -xf - -C "$staging/source"
  git -C "$root" archive --prefix=lua/ "$lua_tree" | tar -xf - -C "$staging/source"
  printf '0.0.0\n' > "$staging/source/VERSION"
  mkdir -p "${reference%/source}"
  mv "$staging/source" "$reference"
fi
# Keep the implementation immutable and use the current harness/build inputs.
for directory in bench cmake scripts tests; do
  mkdir -p "$reference/$directory"
  cp -Rp "$root/$directory/." "$reference/$directory/"
done
cp -p "$root/CMakeLists.txt" "$root/CMakePresets.json" "$root/Makefile" "$reference/"
printf '%s\n' "$reference"
