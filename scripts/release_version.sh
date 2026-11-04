#!/usr/bin/env bash
set -euo pipefail

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
root_dir="$(CDPATH= cd -- "$script_dir/.." && pwd)"
version_file="$root_dir/VERSION"

git_top="$(git -C "$root_dir" rev-parse --show-toplevel 2>/dev/null || true)"
if [ -n "$git_top" ]; then
    git_top="$(CDPATH= cd -- "$git_top" && pwd)"
fi

if [ "$git_top" = "$root_dir" ]; then
    exact_tags="$(git -C "$root_dir" tag --points-at HEAD --sort=-version:refname)"
    release_version=
    while IFS= read -r tag; do
        if [[ "$tag" =~ ^v[0-9]+[.][0-9]+[.][0-9]+$ ]]; then
            if git -C "$root_dir" symbolic-ref --quiet "refs/tags/$tag" >/dev/null 2>&1; then
                continue
            fi
            tag_type="$(git -C "$root_dir" cat-file -t "refs/tags/$tag")"
            if [ "$tag_type" = commit ]; then
                if [[ -n "$release_version" ]]; then
                    printf 'release_version.sh: ambiguous exact lightweight release tags on HEAD\n' >&2
                    exit 1
                fi
                release_version=${tag#v}
            fi
        fi
    done <<<"$exact_tags"
    if [[ -n "$release_version" ]]; then
        printf '%s\n' "$release_version"
        exit 0
    fi
    if [ -n "${LONEJSON_VERSION_OVERRIDE:-}" ]; then
        case "$LONEJSON_VERSION_OVERRIDE" in
          [0-9]*.[0-9]*.[0-9]*)
            if [[ "$LONEJSON_VERSION_OVERRIDE" =~ ^[0-9]+[.][0-9]+[.][0-9]+$ ]]; then
                printf '%s\n' "$LONEJSON_VERSION_OVERRIDE"
                exit 0
            fi
            ;;
        esac
        printf 'release_version.sh: invalid LONEJSON_VERSION_OVERRIDE value: %s\n' "$LONEJSON_VERSION_OVERRIDE" >&2
        exit 1
    fi
    printf '%s\n' "0.0.0"
    exit 0
fi

if [ ! -f "$version_file" ]; then
    printf 'release_version.sh: non-git source tree is missing VERSION\n' >&2
    exit 1
fi

version="$(tr -d '\r\n' < "$version_file")"
if [[ "$version" =~ ^[0-9]+[.][0-9]+[.][0-9]+$ ]]; then
    printf '%s\n' "$version"
else
    printf 'release_version.sh: invalid VERSION value: %s\n' "$version" >&2
    exit 1
fi
