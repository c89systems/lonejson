#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/native-test-prerequisites.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
fixture="$work/repo"
mkdir -p "$fixture/scripts" "$fixture/helpers" "$work/bin" "$fixture/.cache/toolchains"
cp "$repo_root/Makefile" "$fixture/"
cp "$repo_root/scripts/"{test,cpkt-aflpp,require_cache_path}.sh "$fixture/scripts/"
printf '#!/usr/bin/env bash\nprintf "0.0.0\\n"\n' >"$fixture/scripts/release_version.sh"
printf '#!/usr/bin/env bash\nexit 1\n' >"$fixture/scripts/osxcross_available.sh"
printf '#!/usr/bin/env bash\nprintf "x86_64-linux-gnu\\n"\n' >"$fixture/scripts/detect_native_target.sh"
printf '#!/usr/bin/env bash\nprintf "%%s\\n" "$PWD/.cache/cpkt"\n' >"$fixture/scripts/detect_cpkt_bundle.sh"
for script in deps build; do
  cat >"$fixture/scripts/$script.sh" <<'SH'
#!/usr/bin/env bash
printf '%s %s\n' "${0##*/}" "$*" >>"$AFL_PREP_EVENTS"
SH
done
cat >"$fixture/helpers/cpkt-aflpp.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
[[ $# == 1 && $1 == ensure ]]
printf 'afl ensure\n' >>"$AFL_PREP_EVENTS"
[[ ${AFL_PREP_FAIL:-0} == 0 ]] || exit 37
touch "$CPKT_TOOLCHAIN_CACHE/afl-ready"
SH
cat >"$work/bin/uname" <<'SH'
#!/usr/bin/env bash
case "$1" in
  -s) printf '%s\n' "$AFL_PREP_OS" ;;
  -m) printf '%s\n' "$AFL_PREP_ARCH" ;;
  *) exit 1 ;;
esac
SH
cat >"$work/bin/cmake" <<'SH'
#!/usr/bin/env bash
printf 'cmake %s\n' "$*" >>"$AFL_PREP_EVENTS"
SH
cat >"$work/bin/ctest" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
printf 'ctest %s\n' "$*" >>"$AFL_PREP_EVENTS"
if [[ "$AFL_PREP_OS:$AFL_PREP_ARCH" == Linux:x86_64 && ! -f "$CPKT_TOOLCHAIN_CACHE/afl-ready" ]]; then
  printf 'AFL++ policy tests reached an unprepared cache\n' >&2; exit 42
fi
SH
chmod +x "$fixture/scripts/"*.sh "$fixture/helpers/"*.sh "$work/bin/"*
export PATH="$work/bin:$PATH" AFL_PREP_EVENTS="$work/events"
export CPKT_LIFECYCLE_SCRIPTS_DIR="$fixture/helpers"
export CPKT_TOOLCHAIN_CACHE="$fixture/.cache/toolchains"
touch "$CPKT_TOOLCHAIN_CACHE/bootlin-ready"
run_make() {
  make --no-print-directory -s -C "$fixture" "$@" \
    "LONEJSON_HOST_OS=$AFL_PREP_OS" LUA=lua
}
for platform in Linux:x86_64 Darwin:arm64 Linux:aarch64; do
  export AFL_PREP_OS=${platform%:*} AFL_PREP_ARCH=${platform#*:}
  for target in test test-debug test-host test-host-curl; do
    rm -f "$CPKT_TOOLCHAIN_CACHE/afl-ready"
    : >"$AFL_PREP_EVENTS"
    if ! run_make "$target" >"$work/output" 2>&1; then
      cat "$work/output" >&2; exit 1
    fi
    grep '^ctest ' "$AFL_PREP_EVENTS" >/dev/null
    if [[ "$platform" == Linux:x86_64 ]]; then
      [[ $(grep -cx 'afl ensure' "$AFL_PREP_EVENTS") == 1 ]]
      [[ -f "$CPKT_TOOLCHAIN_CACHE/afl-ready" ]]
    else
      ! grep -Fx 'afl ensure' "$AFL_PREP_EVENTS"
      [[ ! -e "$CPKT_TOOLCHAIN_CACHE/afl-ready" ]]
    fi
    [[ -f "$CPKT_TOOLCHAIN_CACHE/bootlin-ready" ]]
  done
done
export AFL_PREP_OS=Linux AFL_PREP_ARCH=x86_64
for target in test test-host test-host-curl; do
  rm -f "$CPKT_TOOLCHAIN_CACHE/afl-ready"
  : >"$AFL_PREP_EVENTS"
  if AFL_PREP_FAIL=1 run_make "$target" >"$work/failed" 2>&1; then
    printf 'native test gate ignored failed AFL preparation: %s\n' "$target" >&2; exit 1
  fi
  grep -Fx 'afl ensure' "$AFL_PREP_EVENTS" >/dev/null
  ! grep '^ctest ' "$AFL_PREP_EVENTS"
done
# Make shares the preparation prerequisite when several suites are requested.
rm -f "$CPKT_TOOLCHAIN_CACHE/afl-ready"
: >"$AFL_PREP_EVENTS"
run_make test test-host test-host-curl >"$work/combined" 2>&1 || {
  cat "$work/combined" >&2; exit 1;
}
[[ $(grep -cx 'afl ensure' "$AFL_PREP_EVENTS") == 1 ]]
[[ $(grep -c '^ctest ' "$AFL_PREP_EVENTS") == 4 ]]
printf 'Native test AFL preparation, platform scope and failure propagation passed\n'
