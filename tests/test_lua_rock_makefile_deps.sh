#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Lua preparation must install the selected configured library on each call;
# CMake owns incremental library builds rather than a shared Make stamp.

repo_root=$1
luarocks_exec=${2:-luarocks}
tmp_dir=$(mktemp -d "$workspace_root/build/test_lua_rock_makefile_deps.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

lua_rock_tree="$tmp_dir/luarocks"
lua_rockspec="$lua_rock_tree/lonejson-0.0.0-1.rockspec"
lua_stamp="$lua_rock_tree/.installed.stamp"

make_lua_rock() {
  make --no-print-directory -C "$repo_root" "$@" \
    LUA_ROCK_TREE="$lua_rock_tree" \
    LUA_ROCKSPEC="$lua_rockspec" \
    LUAROCKS="$luarocks_exec"
}

dry_run=$(make_lua_rock -B -n lua-rock)
configure_line=$(printf '%s\n' "$dry_run" |
  awk '/cmake --preset debug/ { print NR; exit }')
build_line=$(printf '%s\n' "$dry_run" |
  awk '/cmake --build --preset debug --target lonejson_shared/ { print NR; exit }')

if [ -z "$configure_line" ] || [ -z "$build_line" ]; then
  printf '%s\n' "$dry_run" >&2
  printf 'lua-rock dry-run did not configure and build debug liblonejson\n' >&2
  exit 1
fi
if [ "$configure_line" -ge "$build_line" ]; then
  printf '%s\n' "$dry_run" >&2
  printf 'lua-rock builds debug liblonejson before configuring debug\n' >&2
  exit 1
fi
printf '%s\n' "$dry_run" | grep -F -- '-std=gnu89' >/dev/null

mkdir -p "$lua_rock_tree"
touch "$lua_rockspec" "$lua_stamp"
# The environment surface must not override the host loader's runtime.
env_commands=$(make_lua_rock -n lua-env)
if printf '%s\n' "$env_commands" | grep -E 'export (LD_LIBRARY_PATH|DYLD_LIBRARY_PATH)=' >/dev/null; then
  printf 'lua-env must not export loader overrides\n' >&2; exit 1
fi
printf '%s\n' "$env_commands" | grep -F 'export LONEJSON_LIBDIR=' >/dev/null
printf '%s\n' "$dry_run" | grep -F 'export TMPDIR=' >/dev/null

# CTest supplies its already-configured build. Preparing Lua must not reset
# the active configuration to the debug preset while its tests are running.
prepared_build="$tmp_dir/prepared-build"
prepared_plan=$(make_lua_rock -B -n lua-rock LONEJSON_LUA_BUILD_DIR="$prepared_build")
if grep -F 'cmake --preset' <<<"$prepared_plan"; then
  printf 'Lua preparation reconfigures an active CTest build\n' >&2; exit 1
fi
grep -F "cmake --build \"$prepared_build\" --target lonejson_shared lonejson_static" <<<"$prepared_plan" >/dev/null
grep -F "cmake --install \"$prepared_build\"" <<<"$prepared_plan" >/dev/null

# A previous install must not suppress selection of a different configured
# library, even when that old marker is newer than every source input.
touch "$lua_rockspec" "$lua_stamp"
for configuration in debug host asan debug; do
  selected="$tmp_dir/$configuration"
  warm_plan=$(make_lua_rock -n lua-rock LONEJSON_LUA_BUILD_DIR="$selected")
  if ! grep -F "cmake --install \"$selected\"" <<<"$warm_plan" >/dev/null; then
    printf 'warm Lua preparation skipped selected configuration %s\n' "$configuration" >&2; exit 1
  fi
done

# Execute the public Make target against synthetic configured libraries. The
# installed bytes must follow debug -> host -> sanitizer -> debug, even with
# an existing rock and no source edits.
fixture="$tmp_dir/repo"
mkdir -p "$fixture/scripts" "$fixture/build/luarocks" "$tmp_dir/bin"
cp "$repo_root/Makefile" "$fixture/"
printf '#!/usr/bin/env bash\nprintf "0.0.0\\n"\n' >"$fixture/scripts/release_version.sh"
printf '#!/usr/bin/env bash\nprintf "x86_64-linux-gnu\\n"\n' >"$fixture/scripts/detect_native_target.sh"
printf '#!/usr/bin/env bash\nexit 0\n' >"$fixture/scripts/cpkt-toolchains.sh"
printf '#!/usr/bin/env bash\nexit 1\n' >"$fixture/scripts/osxcross_available.sh"
cat >"$tmp_dir/bin/cmake" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
printf '%s %s\n' "$1" "$2" >>"$LUA_PREP_EVENTS"
case $1 in
  --build)
    [[ ${FAIL_LUA_BUILD:-0} == 0 ]]
    cp "$2/profile" "$2/liblonejson.dylib" ;;
  --install)
    [[ $3 == --prefix ]]
    mkdir -p "$4/lib"
    cp "$2/liblonejson.dylib" "$4/lib/" ;;
  *) exit 1 ;;
esac
SH
cat >"$tmp_dir/bin/luarocks" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
[[ $1 == make ]]
actual=$(cat "$LONEJSON_LIBDIR/liblonejson.dylib")
[[ $actual == "$EXPECTED_LUA_PROFILE" ]]
printf 'rock %s\n' "$actual" >>"$LUA_PREP_EVENTS"
SH
chmod +x "$fixture/scripts/"*.sh "$tmp_dir/bin/"*
# Reconstruct the old warm state; it must no longer suppress preparation.
touch "$fixture/build/luarocks/spec.rockspec" "$fixture/build/luarocks/.installed.stamp"
export LUA_PREP_EVENTS="$tmp_dir/preparation-events"
prepare() {
  PATH="$tmp_dir/bin:$PATH" make -s -C "$fixture" lua-rock \
    LONEJSON_LUA_BUILD_DIR="$1" LUA_ROCK_SOURCES= \
    LUA_ROCKSPEC="$fixture/build/luarocks/spec.rockspec" \
    LUAROCKS="$tmp_dir/bin/luarocks"
}
for configuration in debug host sanitizer debug; do
  selected="$fixture/build/$configuration"
  mkdir -p "$selected"
  printf '%s\n' "$configuration" >"$selected/profile"
  EXPECTED_LUA_PROFILE="$configuration" prepare "$selected" >"$tmp_dir/prepare.log" 2>&1 || {
    cat "$tmp_dir/prepare.log" >&2; exit 1;
  }
  [[ $(cat "$fixture/build/lua-sdk/lib/liblonejson.dylib") == "$configuration" ]]
done
# A new binary in the same build must also replace the installed library.
printf 'host-updated\n' >"$fixture/build/host/profile"
EXPECTED_LUA_PROFILE=host-updated prepare "$fixture/build/host" >"$tmp_dir/prepare.log" 2>&1
[[ $(cat "$fixture/build/lua-sdk/lib/liblonejson.dylib") == host-updated ]]
: >"$LUA_PREP_EVENTS"
if FAIL_LUA_BUILD=1 EXPECTED_LUA_PROFILE=debug prepare "$fixture/build/debug" >"$tmp_dir/failed.log" 2>&1; then
  printf 'Lua preparation ignored a failed selected build\n' >&2; exit 1
fi
[[ $(wc -l <"$LUA_PREP_EVENTS") == 1 ]]
printf 'Warm Lua preparation installs the selected library and propagates failures\n'
