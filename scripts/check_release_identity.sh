#!/usr/bin/env bash
set -euo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
fail() { printf 'release precondition: %s\n' "$*" >&2; exit 1; }
[[ $# == 0 ]] || fail 'this check accepts no arguments'
git_root=$(git -C "$repo_root" rev-parse --show-toplevel)
[[ "$(CDPATH= cd -- "$git_root" && pwd -P)" == "$repo_root" ]] || fail 'release requires the owning Git checkout'
[[ -z "${LONEJSON_VERSION_OVERRIDE:-}" ]] || fail 'remove LONEJSON_VERSION_OVERRIDE before final release'
[[ -z "$(git -C "$repo_root" status --porcelain --untracked-files=normal)" ]] || fail 'commit or remove pending work before final release'
default_ref=$(git -C "$repo_root" symbolic-ref --quiet refs/remotes/origin/HEAD) || fail 'set origin/HEAD to the authoritative default branch before final release'
default_branch=${default_ref#refs/remotes/origin/}
branch=$(git -C "$repo_root" symbolic-ref --quiet --short HEAD) || fail 'final release requires an attached default branch'
[[ "$branch" == "$default_branch" ]] || fail "final release requires the default branch $default_branch; got $branch"
git -C "$repo_root" verify-commit HEAD >/dev/null 2>&1 || fail 'HEAD must have a verified commit signature before final release'
tags=$(git -C "$repo_root" tag --points-at HEAD)
release_tags=()
while IFS= read -r tag; do
  [[ ! "$tag" =~ ^v[0-9]+[.][0-9]+[.][0-9]+$ ]] || release_tags+=("$tag")
done <<<"$tags"
[[ ${#release_tags[@]} == 1 ]] || fail 'HEAD must have exactly one lightweight vX.Y.Z release tag'
tag=${release_tags[0]}
if git -C "$repo_root" symbolic-ref --quiet "refs/tags/$tag" >/dev/null 2>&1; then
  fail "$tag must not be symbolic"
fi
[[ "$(git -C "$repo_root" cat-file -t "refs/tags/$tag")" == commit ]] || fail "$tag must be lightweight"
[[ "$("$repo_root/scripts/release_version.sh")" == "${tag#v}" ]] || fail 'release version differs from the selected tag'
printf 'Release identity verified: %s %s\n' "$tag" "$(git -C "$repo_root" rev-parse HEAD)"
