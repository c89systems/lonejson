#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
  printf 'usage: %s <repo-root> <stage-dir> [release-version]\n' "$0" >&2
  exit 1
fi

repo_root=$1
stage_dir=$2
workspace_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
"$workspace_root/scripts/require_build_workspace.sh" "$stage_dir"
release_version=${3:-}
manifest_path="$repo_root/RELEASE_MANIFEST"
tmp_manifest=""

cleanup() {
  if [[ -n "$tmp_manifest" && -f "$tmp_manifest" ]]; then
    rm -f "$tmp_manifest"
  fi
}

trap cleanup EXIT INT TERM

repo_root=$(CDPATH= cd -- "$repo_root" && pwd -P)
git_root=$(git -C "$repo_root" rev-parse --show-toplevel 2>/dev/null || true)
if [[ ! -f "$manifest_path" && "$git_root" != "$repo_root" ]]; then
  printf 'source staging requires a git worktree or RELEASE_MANIFEST: %s\n' "$repo_root" >&2
  exit 1
fi

rm -rf "$stage_dir"
mkdir -p "$stage_dir"

if [[ -f "$manifest_path" ]]; then
  cp "$manifest_path" "$stage_dir/RELEASE_MANIFEST"
  tar -C "$repo_root" -cf - -T "$manifest_path" | tar -xpf - -C "$stage_dir"
elif [[ "$git_root" == "$repo_root" ]]; then
  mkdir -p "$workspace_root/build"
  tmp_manifest="$(mktemp "$workspace_root/build/source-manifest.XXXXXX")"
  while IFS= read -r -d '' path; do
    [[ -e "$repo_root/$path" ]] || continue
    git -C "$repo_root" check-ignore --no-index -q "$path" && continue
    printf '%s\n' "$path" >>"$tmp_manifest"
  done < <(git -C "$repo_root" ls-files -z)
  cp "$tmp_manifest" "$stage_dir/RELEASE_MANIFEST"
  tar -C "$repo_root" -cf - -T "$tmp_manifest" | tar -xpf - -C "$stage_dir"
else
  printf 'source staging requires a git worktree or RELEASE_MANIFEST: %s\n' "$repo_root" >&2
  exit 1
fi

if [[ -n "$release_version" ]]; then
  printf '%s\n' "$release_version" >"$stage_dir/VERSION"
  if ! grep -qx 'VERSION' "$stage_dir/RELEASE_MANIFEST" 2>/dev/null; then
    printf '%s\n' 'VERSION' >>"$stage_dir/RELEASE_MANIFEST"
  fi
  if ! grep -qx 'RELEASE_MANIFEST' "$stage_dir/RELEASE_MANIFEST" 2>/dev/null; then
    printf '%s\n' 'RELEASE_MANIFEST' >>"$stage_dir/RELEASE_MANIFEST"
  fi
fi
