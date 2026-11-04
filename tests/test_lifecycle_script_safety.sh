#!/usr/bin/env bash
set -euo pipefail
repo_root=$1
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/lifecycle-script-safety.XXXXXX")
trap 'rm -rf "$work"' EXIT
root="$work/repo"
mkdir -p "$root/scripts" "$work/bin" "$root/build/retained" "$work/seeds"
for script in build package deps fuzz require_build_workspace ensure_large_fixtures smoke_darwin_release detect_cpkt_bundle detect_native_target detect_native_bootlin_target; do
  cp "$repo_root/scripts/$script.sh" "$root/scripts/"
done
mkdir -p "$root/.cache/cpkt/x86_64-linux-gnu/root" "$root/cmake/toolchains"
cp "$repo_root/cmake/toolchains/"{print_native_bootlin_target,lonejson_native_bootlin_target}.cmake "$root/cmake/toolchains/"
cat >"$work/bin/cmake" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
[[ "$PWD" == "$TEST_ROOT" ]]
printf '%s\n' "$*" >>"$TEST_LOG"
SH
cat >"$work/bin/make" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
[[ "$1" == -C && "$2" == "$TEST_ROOT" ]]
printf '%s\n' "$3" >>"$TEST_LOG"
[[ "$3" != "${TEST_FAIL_TARGET:-}" ]]
SH
chmod +x "$work/bin/"*
export TEST_ROOT="$root" TEST_LOG="$work/dispatch.log"
(cd "$work" && LONEJSON_CPKT_TARGET_ID=x86_64-linux-gnu PATH="$work/bin:$PATH" "$root/scripts/build.sh" debug)
[[ $(wc -l <"$TEST_LOG") == 2 ]]
grep -F -- '--build --preset debug' "$TEST_LOG" >/dev/null
: >"$TEST_LOG"
(cd "$work" && PATH="$work/bin:$PATH" "$root/scripts/package.sh")
printf '%s\n' release-matrix package-source package-checksums package-verify >"$work/expected"
cmp "$work/expected" "$TEST_LOG"
: >"$TEST_LOG"
if PATH="$work/bin:$PATH" TEST_FAIL_TARGET=package-source "$root/scripts/package.sh"; then
  echo 'packaging ignored a failed stage' >&2; exit 1
fi
[[ $(wc -l <"$TEST_LOG") == 2 ]]
# Invalid inputs must fail before touching retained results or running a target.
printf retained >"$root/build/retained/sentinel"
cat >"$work/target" <<'SH'
#!/bin/sh
printf called >>"$TEST_LOG"
echo lonejson-fuzz-no-core-v1
SH
chmod +x "$work/target"
reject() {
  : >"$TEST_LOG"
  if "$@" >"$work/rejected.log" 2>&1; then
    cat "$work/rejected.log" >&2; echo 'unsafe request accepted' >&2; exit 1
  fi
  [[ ! -s "$TEST_LOG" ]]
  [[ $(cat "$root/build/retained/sentinel") == retained ]]
}
reject "$root/scripts/deps.sh" x86_64-linux-gnu extra
reject "$root/scripts/package.sh" extra
reject "$root/scripts/fuzz.sh" /bin/true 1 ../../retained "$work/target" "$work/seeds"
reject "$root/scripts/fuzz.sh" /bin/true invalid valid "$work/target" "$work/seeds"
reject env LONEJSON_FUZZ_EXECUTIONS=invalid "$root/scripts/fuzz.sh" /bin/true 1 valid "$work/target" "$work/seeds"
mkdir -p "$work/escaped"
ln -s "$work/escaped" "$root/build/fuzz"
reject "$root/scripts/fuzz.sh" /bin/true 1 valid "$work/target" "$work/seeds"
reject "$root/scripts/ensure_large_fixtures.sh" "$work/target" /dev/null "$work/escaped/fixtures"
reject "$root/scripts/smoke_darwin_release.sh" ../../retained
reject "$root/scripts/smoke_darwin_release.sh" debug unexpected
# GNU Make must honor serialization even when a caller requests parallel jobs.
cat >"$work/serialization.mk" <<'MAKE'
.PHONY: lifecycle_serialization_probe lifecycle_probe_a lifecycle_probe_b
lifecycle_serialization_probe: lifecycle_probe_a lifecycle_probe_b
lifecycle_probe_a lifecycle_probe_b:
	@mkdir "$(TEST_LOCK)"
	@sleep 0.1
	@rmdir "$(TEST_LOCK)"
MAKE
make -s -C "$repo_root" -f "$repo_root/Makefile" -f "$work/serialization.mk" -j 4 \
  lifecycle_serialization_probe TEST_LOCK="$work/active"
echo 'Lifecycle script dispatch, failure propagation, workspace safety and serialization passed'
