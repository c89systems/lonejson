#!/usr/bin/env bash
set -euo pipefail
[[ $# == 2 ]] || { printf 'usage: %s STAGED_TREE OUTPUT.tar.gz|OUTPUT.zip\n' "$0" >&2; exit 2; }
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
stage=$1
output=$2
case "$output" in *.tar.gz|*.zip) ;; *) printf 'unsupported release archive: %s\n' "$output" >&2; exit 2 ;; esac
"$repo_root/scripts/require_build_workspace.sh" "$stage"
"$repo_root/scripts/require_build_workspace.sh" "$(dirname -- "$output")"
[[ ! -L "$output" && ! -d "$output" ]] || { printf 'unsafe archive destination: %s\n' "$output" >&2; exit 1; }
stage=$(CDPATH= cd -- "$stage" && pwd -P)
name=$(basename -- "$stage")
work=$(mktemp -d "$repo_root/build/release-archive.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
export LC_ALL=C TZ=UTC COPYFILE_DISABLE=1
parent=$work
cp -pRP "$stage" "$work/$name"
(cd "$parent"; find "$name" -print | sort) >"$work/entries"
# Normalize a disposable copy so Make's staging timestamps stay valid.
# Retain executable intent and symbolic links;
# 1980 is also representable in ZIP's DOS timestamps.
executable_paths=()
regular_paths=()
while IFS= read -r entry; do
  path="$parent/$entry"
  if [[ ! -L "$path" ]]; then
    if [[ -d "$path" || -x "$path" ]]; then
      executable_paths+=("$path")
    else
      regular_paths+=("$path")
    fi
  fi
done <"$work/entries"
if [[ ${#executable_paths[@]} -ne 0 ]]; then
  printf '%s\0' "${executable_paths[@]}" | xargs -0 chmod 755
fi
if [[ ${#regular_paths[@]} -ne 0 ]]; then
  printf '%s\0' "${regular_paths[@]}" | xargs -0 chmod 644
fi
while IFS= read -r entry; do
  printf '%s\0' "$parent/$entry"
done <"$work/entries" | xargs -0 touch -h -t 198001010000.00
case "$output" in
  *.tar.gz)
    case "$(tar --version)" in
      *GNU*) ownership=(--format=gnu --owner=0 --group=0 --numeric-owner) ;;
      *bsdtar*) ownership=(--format=gnutar --uid=0 --gid=0 --uname=root --gname=root
        --no-acls --no-xattrs --no-fflags --no-mac-metadata) ;;
      *) printf 'GNU tar or bsdtar is required for release archives\n' >&2; exit 1 ;;
    esac
    (cd "$parent"; tar -cf "$work/archive.tar" "${ownership[@]}" --no-recursion -T "$work/entries")
    gzip -n -9 -c "$work/archive.tar" >"$work/output"
    ;;
  *.zip)
    (cd "$parent"; zip -X -y -q "$work/archive.zip" -@ <"$work/entries")
    mv "$work/archive.zip" "$work/output"
    ;;
esac
mv -f "$work/output" "$output"
