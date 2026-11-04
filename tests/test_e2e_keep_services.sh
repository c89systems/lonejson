#!/usr/bin/env bash
set -euo pipefail
repo_root=$1
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/e2e-cleanup.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/repo/scripts" "$work/bin"
cp "$repo_root/scripts/test-e2e.sh" "$work/repo/scripts/"
cat >"$work/repo/scripts/devenv.sh" <<'SH'
#!/usr/bin/env bash
printf '%s\n' "$*" >>"$E2E_EVENTS"
[[ "$1" != up || "${E2E_UP_FAIL:-0}" != 1 ]]
SH
cat >"$work/bin/make" <<'SH'
#!/usr/bin/env bash
printf 'make:%s\n' "$*" >>"$E2E_EVENTS"
[[ "${E2E_BUILD_FAIL:-0}" != 1 ]]
SH
cat >"$work/bin/ctest" <<'SH'
#!/usr/bin/env bash
printf 'ctest:%s\n' "$*" >>"$E2E_EVENTS"
[[ "${E2E_TEST_FAIL:-0}" != 1 ]]
SH
chmod +x "$work/repo/scripts/devenv.sh" "$work/bin/"*
export PATH="$work/bin:$PATH" E2E_EVENTS="$work/events"
for keep in 0 1; do
  : >"$E2E_EVENTS"
  LONEJSON_E2E_KEEP_DEVSERVICES="$keep" "$work/repo/scripts/test-e2e.sh"
  [[ $(grep -cx up "$E2E_EVENTS") == 1 ]]
  grep -Fx 'make:--no-print-directory e2e-build' "$E2E_EVENTS"
  grep -Fx 'ctest:--preset e2e' "$E2E_EVENTS"
  if [[ "$keep" == 0 ]]; then [[ $(tail -n 1 "$E2E_EVENTS") == reset ]];
  else ! grep -Ex 'down|reset' "$E2E_EVENTS"; fi
  for failure in E2E_UP_FAIL E2E_BUILD_FAIL E2E_TEST_FAIL; do
    : >"$E2E_EVENTS"
    if env "$failure=1" LONEJSON_E2E_KEEP_DEVSERVICES="$keep" \
        "$work/repo/scripts/test-e2e.sh" >"$work/failure" 2>&1; then
      printf 'e2e swallowed a startup, build, or CTest failure\n' >&2; exit 1
    fi
    grep -Fx ps "$E2E_EVENTS"
    grep -Fx 'logs --tail 120' "$E2E_EVENTS"
    if [[ "$keep" == 0 && "$failure" != E2E_UP_FAIL ]]; then
      [[ $(tail -n 1 "$E2E_EVENTS") == down ]]
    else ! grep -Ex 'down|reset' "$E2E_EVENTS"; fi
    if [[ "$failure" != E2E_TEST_FAIL ]]; then ! grep -F 'ctest:' "$E2E_EVENTS"; fi
  done
done
