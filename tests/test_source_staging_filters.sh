#!/usr/bin/env bash
set -euo pipefail
root=${1:?repository root required}
work=$(mktemp -d "$root/build/source-staging-filters.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
fixture="$work/source"
stage="$work/stage"
mkdir -p "$fixture/ignored"
git -C "$fixture" init -q
git -C "$fixture" config core.excludesFile /dev/null
printf '/ignored/*\n!/ignored/keep\n/private-secret\n' >"$fixture/.gitignore"
for path in visible 'space file' ignored/hidden ignored/keep private-secret missing; do
  printf '%s\n' "$path" >"$fixture/$path"
done
ln -s visible "$fixture/link"
ln -s absent "$fixture/dangling"
git -C "$fixture" add -f -- .gitignore visible 'space file' ignored private-secret missing link dangling
rm "$fixture/missing"
bash "$root/scripts/stage_release_sources.sh" "$fixture" "$stage" 1.2.3
for path in visible 'space file' ignored/keep link; do
  cmp "$fixture/$path" "$stage/$path"
  grep -Fx "$path" "$stage/RELEASE_MANIFEST" >/dev/null
done
[[ -L "$stage/link" ]]
for path in private-secret ignored/hidden missing dangling; do
  [[ ! -e "$stage/$path" && ! -L "$stage/$path" ]]
  if grep -Fx "$path" "$stage/RELEASE_MANIFEST"; then
    printf 'private or missing source entered the manifest: %s\n' "$path" >&2
    exit 1
  fi
done
grep -Fx 1.2.3 "$stage/VERSION" >/dev/null
