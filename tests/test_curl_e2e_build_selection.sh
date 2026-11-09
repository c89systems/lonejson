#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
work=$(mktemp -d "$repo_root/build/curl-e2e-selection.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/scripts" "$work/examples/bin" "$work/selected-debug" "$work/build/host-curl"
cp "$repo_root/scripts/test_curl_e2e.sh" "$work/scripts/"
# A staged Release client must never replace the selected Debug child.
for client in curl_get curl_put; do
  printf '#!/usr/bin/env bash\nexit 87\n' > "$work/examples/bin/$client"
  chmod +x "$work/examples/bin/$client"
done
export LONEJSON_E2E_SELECTION_EVENTS="$work/events"
for directory in "$work/selected-debug" "$work/build/host-curl"; do
  for program in example_curl_get example_curl_put lonejson_curl_rewind_e2e; do
    cat > "$directory/$program" <<'APP'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$0" >> "$LONEJSON_E2E_SELECTION_EVENTS"
APP
    chmod +x "$directory/$program"
  done
  : > "$work/events"
  LONEJSON_E2E_BUILD_DIR="$directory" LONEJSON_CURL_E2E_CAINFO="$work/server.crt" \
    bash "$work/scripts/test_curl_e2e.sh" > "$work/run.log" 2>&1 || {
    cat "$work/run.log" >&2
    printf 'curl e2e failed to use the selected build: %s\n' "$directory" >&2
    exit 1
  }
  [[ $(grep -c '/example_curl_get$' "$work/events") == 4 ]]
  [[ $(grep -c '/example_curl_put$' "$work/events") == 4 ]]
  [[ $(grep -c '/lonejson_curl_rewind_e2e$' "$work/events") == 1 ]]
  while IFS= read -r child; do
    [[ "$child" == "$directory/"* ]] || {
      printf 'child came from another build: %s\n' "$child" >&2; exit 1;
    }
  done < "$work/events"
done
# Fail before any child executes when the selected build is incomplete.
rm "$work/selected-debug/example_curl_get"
: > "$work/events"
if LONEJSON_E2E_BUILD_DIR="$work/selected-debug" \
    bash "$work/scripts/test_curl_e2e.sh" > "$work/missing.log" 2>&1; then
  printf 'missing selected client unexpectedly succeeded\n' >&2; exit 1
fi
grep -q 'example_curl_get' "$work/missing.log"
[[ ! -s "$work/events" ]]
