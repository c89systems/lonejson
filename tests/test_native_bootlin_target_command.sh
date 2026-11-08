#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
target_id="$("$repo_root/scripts/detect_native_bootlin_target.sh")"

case "$target_id" in
  x86_64-linux-gnu|x86_64-linux-musl|aarch64-linux-gnu|aarch64-linux-musl|armhf-linux-gnu|armhf-linux-musl)
    ;;
  *)
    printf 'native Bootlin target command returned an unsupported target: %s\n' "$target_id" >&2
    exit 1
    ;;
esac

# CMake notices must not become part of Make's toolchain target argument.
mkdir -p "$repo_root/build"
workspace=$(mktemp -d "$repo_root/build/test-native-target-output.XXXXXX")
trap 'rm -rf "$workspace"' EXIT
real_cmake=$(command -v cmake)
cat >"$workspace/cmake" <<'SH'
#!/usr/bin/env bash
printf 'fixture CMake diagnostic\n' >&2
exec "$TEST_REAL_CMAKE" "$@"
SH
chmod +x "$workspace/cmake"
PATH="$workspace:$PATH" TEST_REAL_CMAKE="$real_cmake" \
  "$repo_root/scripts/detect_native_bootlin_target.sh" >"$workspace/out" 2>"$workspace/err"
printf '%s\n' "$target_id" >"$workspace/expected"
cmp "$workspace/expected" "$workspace/out"
grep -Fx 'fixture CMake diagnostic' "$workspace/err" >/dev/null
