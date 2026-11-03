#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  printf 'usage: %s <output-src-rock> <rockspec> <source-tarball>\n' "$0" >&2
  exit 1
fi

output_src_rock=$1
rockspec=$2
source_tarball=$3
if [[ -L "$output_src_rock" || -d "$output_src_rock" ]]; then
  printf 'unsafe source rock destination: %s\n' "$output_src_rock" >&2
  exit 1
fi

if [[ ! -f "$rockspec" ]]; then
  printf 'package_lua_src_rock.sh: rockspec not found: %s\n' "$rockspec" >&2
  exit 1
fi
if [[ ! -f "$source_tarball" ]]; then
  printf 'package_lua_src_rock.sh: source tarball not found: %s\n' "$source_tarball" >&2
  exit 1
fi
if ! command -v zip >/dev/null 2>&1; then
  printf 'package_lua_src_rock.sh: zip not found in PATH\n' >&2
  exit 1
fi

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/source-rock.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
spec_name=$(basename -- "$rockspec")
source_name=$(basename -- "$source_tarball")
cp "$rockspec" "$work/$spec_name"
cp "$source_tarball" "$work/$source_name"
chmod 644 "$work/$spec_name" "$work/$source_name"
TZ=UTC touch -t 198001010000.00 "$work/$spec_name" "$work/$source_name"
(cd "$work"; TZ=UTC zip -X -q archive.src.rock "$spec_name" "$source_name")
mv -f "$work/archive.src.rock" "$output_src_rock"
