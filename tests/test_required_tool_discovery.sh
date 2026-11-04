#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/required-discovery.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
fixture="$work/repo"
mkdir -p "$fixture/scripts" "$fixture/build"
cp "$repo_root/scripts/"{check_curl_abi_symbols,check_jwt_abi_symbols,check_darwin_macho_metadata}.sh "$fixture/scripts/"
touch "$work/shared" "$work/static"
cat >"$fixture/scripts/discover_target_tools.sh" <<'SH'
#!/usr/bin/env bash
printf 'NM=%q\n' "$INHERITED_TOOL"
printf 'required tool discovery failed\n' >&2
exit 23
SH
cat >"$work/inherited-tool" <<'SH'
#!/usr/bin/env bash
printf 'inherited tool used\n' >>"$DISCOVERY_EVENTS"
SH
chmod +x "$fixture/scripts/"*.sh "$work/inherited-tool"
export INHERITED_TOOL="$work/inherited-tool" DISCOVERY_EVENTS="$work/events"
export NM="$INHERITED_TOOL" OTOOL="$INHERITED_TOOL"
for check in check_curl_abi_symbols check_jwt_abi_symbols check_darwin_macho_metadata; do
  : >"$DISCOVERY_EVENTS"
  status=0
  "$fixture/scripts/$check.sh" "$fixture" "$fixture/build" arm64-apple-darwin \
    "$work/shared" "$work/static" >"$work/failure.log" 2>&1 || status=$?
  [[ $status == 23 ]] || { cat "$work/failure.log" >&2; printf 'discovery failure was hidden by %s\n' "$check" >&2; exit 1; }
  [[ ! -s "$DISCOVERY_EVENTS" ]]
  grep -F 'required tool discovery failed' "$work/failure.log" >/dev/null
done
# The CMake packaging adapter must also refuse partial discovery even when
# every tool variable is inherited from a previously successful target.
touch "$fixture/build/CMakeCache.txt"
cat >"$work/adapter.cmake" <<'CMAKE'
include("${ADAPTER}")
lonejson_discover_target_tools()
CMAKE
if CC="$INHERITED_TOOL" LINKER="$INHERITED_TOOL" AR="$INHERITED_TOOL" \
    STRIP="$INHERITED_TOOL" cmake -D "ADAPTER=$repo_root/cmake/lonejson_target_tools.cmake" \
    -D "LONEJSON_ROOT=$fixture" -D "LONEJSON_BINARY_DIR=$fixture/build" \
    -D LONEJSON_TARGET_ID=arm64-apple-darwin -P "$work/adapter.cmake" >"$work/adapter.log" 2>&1; then
  printf 'CMake packaging adapter accepted failed target discovery\n' >&2; exit 1
fi
grep -F 'Target-tool discovery failed' "$work/adapter.log" >/dev/null
printf 'Required discovery fails before inherited tools or partial exports are used\n'
