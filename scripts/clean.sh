#!/usr/bin/env bash

set -eu

mode="all"
root_dir=""
dist_dir=""

usage() {
    printf 'usage: %s [--dist-only] [--root DIR] [--dist-dir DIR]\n' "$0" >&2
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --dist-only)
            mode="dist"
            shift
            ;;
        --root)
            if [ "$#" -lt 2 ]; then
                usage
                exit 1
            fi
            root_dir="$2"
            shift 2
            ;;
        --dist-dir)
            if [ "$#" -lt 2 ]; then
                usage
                exit 1
            fi
            dist_dir="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done

if [ -z "$root_dir" ]; then
    root_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
else
    root_dir="$(CDPATH= cd -- "$root_dir" && pwd)"
fi

script_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
home_dir="${HOME:-}"

if [ "$root_dir" = "/" ] || [ "$root_dir" = "$home_dir" ] ||
   [ "$root_dir" != "$script_root" ]; then
    printf 'clean.sh: refusing to clean unsafe root: %s\n' "$root_dir" >&2
    exit 1
fi

if [ -z "$dist_dir" ]; then
    dist_dir="$root_dir/dist"
else
    case "$dist_dir" in
        /*)
            ;;
        *)
            printf 'clean.sh: dist directory must be absolute: %s\n' "$dist_dir" >&2
            exit 1
            ;;
    esac
fi

if [ "$dist_dir" = "/" ] || [ "$dist_dir" = "$home_dir" ] ||
   [ "$dist_dir" = "$root_dir" ]; then
  printf 'clean.sh: refusing to clean unsafe dist directory: %s\n' "$dist_dir" >&2
  exit 1
fi

if [ "$dist_dir" != "$root_dir/dist" ]; then
    case "$dist_dir" in
        "$root_dir"/*)
            printf 'clean.sh: refusing to clean project directory as custom dist: %s\n' "$dist_dir" >&2
            exit 1
            ;;
    esac
    if [ -e "$dist_dir" ] && [ ! -f "$dist_dir/.lonejson-dist" ]; then
        printf 'clean.sh: refusing to clean unmarked custom dist directory: %s\n' "$dist_dir" >&2
        exit 1
    fi
fi

remove_path() {
    target_path="$1"
    case "$target_path" in
        "$root_dir"/build|"$root_dir"/dist|"$root_dir"/.cache|"$root_dir"/.luarocks-build|"$root_dir"/examples/bin|"$root_dir"/lonejson|"$dist_dir")
            ;;
        *)
            printf 'clean.sh: refusing to remove unexpected path: %s\n' "$target_path" >&2
            exit 1
            ;;
    esac
    if [ -e "$target_path" ] || [ -L "$target_path" ]; then
        rm -rf -- "$target_path"
    fi
}

if [ "$mode" = "all" ]; then
    if [ -f "$root_dir/build/devenv/devenv.yaml" ]; then
        "$root_dir/scripts/devenv.sh" down
    fi
    remove_path "$root_dir/build"
    remove_path "$root_dir/dist"
    remove_path "$root_dir/.cache"
    remove_path "$root_dir/.luarocks-build"
    remove_path "$root_dir/examples/bin"
    remove_path "$root_dir/lonejson"
fi

remove_path "$dist_dir"
