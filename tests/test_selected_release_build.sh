#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/selected-release.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
fixture="$work/repo"
mkdir -p "$fixture/scripts" "$work/bin"
cp "$repo_root/Makefile" "$fixture/"
cp "$repo_root/scripts/deps.sh" "$fixture/scripts/"
printf '#!/usr/bin/env bash\nprintf "0.0.0\\n"\n' >"$fixture/scripts/release_version.sh"
printf '#!/usr/bin/env bash\nexit 1\n' >"$fixture/scripts/osxcross_available.sh"
cat >"$fixture/scripts/cpkt-toolchains.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
[[ "$1" == ensure && "$2" == *-linux-* ]]
printf 'ensure %s\n' "$2" >>"$RELEASE_EVENTS"
SH
cat >"$work/bin/cmake" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
target=
for arg in "$@"; do
  case $arg in LONEJSON_CPKT_TARGET_ID=*) target=${arg#*=} ;; esac
done
[[ -n $target && "$target" == *-linux-* ]]
printf 'fetch %s\n' "$target" >>"$RELEASE_EVENTS"
[[ $target != "${RELEASE_FAIL_TARGET:-}" ]]
mkdir -p "$PWD/.cache/cpkt/$target/root"
SH
cat >"$fixture/scripts/build.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
target=${1%-release}
[[ -d ".cache/cpkt/$target/root" ]]
printf 'build %s\n' "$target" >>"$RELEASE_EVENTS"
SH
chmod +x "$fixture/scripts/"*.sh "$work/bin/cmake"
export RELEASE_EVENTS="$work/events"
export PATH="$work/bin:$PATH"
make -s -C "$fixture" build-release LONEJSON_HOST_OS=Linux >"$work/output" 2>&1 || {
  cat "$work/output" >&2; exit 1;
}
for target in x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl armhf-linux-gnu armhf-linux-musl; do
  grep -Fx "ensure $target" "$RELEASE_EVENTS" >/dev/null
  grep -Fx "fetch $target" "$RELEASE_EVENTS" >/dev/null
  grep -Fx "build $target" "$RELEASE_EVENTS" >/dev/null
done
[[ $(grep -c '^build ' "$RELEASE_EVENTS") == 6 ]]
! grep -F apple-darwin "$RELEASE_EVENTS"
: >"$RELEASE_EVENTS"
make -s -C "$fixture" build-release LONEJSON_HOST_OS=Linux \
  RELEASE_BUILD_PRESETS=aarch64-linux-musl-release >"$work/selected" 2>&1
[[ $(grep -c '^fetch ' "$RELEASE_EVENTS") == 1 ]]
grep -Fx 'build aarch64-linux-musl' "$RELEASE_EVENTS" >/dev/null
! grep -E 'x86_64|armhf|apple-darwin' "$RELEASE_EVENTS"
: >"$RELEASE_EVENTS"
if RELEASE_FAIL_TARGET=aarch64-linux-gnu make -s -C "$fixture" build-release LONEJSON_HOST_OS=Linux >"$work/failed" 2>&1; then
  printf 'release build ignored a failed prerequisite\n' >&2; exit 1
fi
! grep '^build ' "$RELEASE_EVENTS"
printf 'Selected release preparation and failure propagation passed\n'
