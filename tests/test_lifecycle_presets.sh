#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
presets="$repo_root/CMakePresets.json"

require_text() {
  local text=$1
  if ! grep -F -- "$text" "$presets" >/dev/null; then
    printf 'missing lifecycle preset contract: %s\n' "$text" >&2
    exit 1
  fi
}

reject_text() {
  local text=$1
  if grep -F -- "$text" "$presets" >/dev/null; then
    printf 'obsolete lifecycle preset contract remains: %s\n' "$text" >&2
    exit 1
  fi
}

preset_block() {
  local target_id=$1
  awk -v name="$target_id-release" '
    $0 ~ "\"name\": \"" name "\"" { active = 1 }
    active { print }
    active && /^    },$/ { exit }
  ' "$presets"
}

for target_id in \
  x86_64-linux-gnu \
  x86_64-linux-musl \
  aarch64-linux-gnu \
  aarch64-linux-musl \
  armhf-linux-gnu \
  armhf-linux-musl \
  arm64-apple-darwin; do
  require_text '"name": "'"$target_id"'-release"'
  require_text '"LONEJSON_TARGET_ID": "'"$target_id"'"'
  require_text '"LONEJSON_DIST_DIR": "${sourceDir}/dist"'
  require_text '"LONEJSON_CPKT_ROOT": "${sourceDir}/.cache/cpkt/'"$target_id"'/root"'
done

while IFS='|' read -r target_id target_arch target_os target_libc; do
  target_preset="$(preset_block "$target_id")"
  grep -F "\"LONEJSON_TARGET_ARCH\": \"$target_arch\"" <<<"$target_preset" >/dev/null
  grep -F "\"LONEJSON_TARGET_OS\": \"$target_os\"" <<<"$target_preset" >/dev/null
  grep -F "\"LONEJSON_TARGET_LIBC\": \"$target_libc\"" <<<"$target_preset" >/dev/null
done <<'EOF'
x86_64-linux-gnu|x86_64|linux|gnu
x86_64-linux-musl|x86_64|linux|musl
aarch64-linux-gnu|aarch64|linux|gnu
aarch64-linux-musl|aarch64|linux|musl
armhf-linux-gnu|armhf|linux|gnu
armhf-linux-musl|armhf|linux|musl
arm64-apple-darwin|arm64|darwin|
EOF

require_text '"name": "package-archive-x86_64-linux-gnu"'
require_text '"configurePreset": "x86_64-linux-gnu-release"'
require_text '"name": "package-archive-x86_64-linux-musl"'
require_text '"configurePreset": "x86_64-linux-musl-release"'
require_text '"toolchainFile": "${sourceDir}/cmake/toolchains/native.cmake"'
[[ -f "$repo_root/cmake/toolchains/native.cmake" ]]
grep -F 'lonejson_native_bootlin_target.cmake' \
  "$repo_root/cmake/toolchains/native.cmake" >/dev/null
grep -F 'ldd --version' \
  "$repo_root/cmake/toolchains/lonejson_native_bootlin_target.cmake" >/dev/null
grep -F 'lonejson_configure_bootlin_toolchain' \
  "$repo_root/cmake/toolchains/native.cmake" >/dev/null
fuzz_preset_block=$(awk '
  index($0, "\"name\": \"fuzz\"") { active = 1 }
  active { print }
  active && /^    },$/ { exit }
' "$presets")
grep -F '"LONEJSON_BUILD_FUZZERS": "ON"' <<<"$fuzz_preset_block" >/dev/null
grep -F '"LONEJSON_ENABLE_ASAN": "ON"' <<<"$fuzz_preset_block" >/dev/null
for host_preset in debug host; do
  host_preset_block=$(awk -v name="$host_preset" '
    index($0, "\"name\": \"" name "\"") { active = 1 }
    active { print }
    active && /^    },$/ { exit }
  ' "$presets")
  grep -F '"toolchainFile": "${sourceDir}/cmake/toolchains/native.cmake"' \
    <<<"$host_preset_block" >/dev/null
done

reject_text '"name": "linux-gnu-release"'
reject_text '"name": "linux-musl-release"'
reject_text '"configurePreset": "linux-gnu-release"'
reject_text '"configurePreset": "linux-musl-release"'
reject_text '${sourceDir}/.deps/cpkt'

# Verify real preset selection with a minimal inventory; preset filters can
# override command-line filters, so text checks alone do not establish scope.
mkdir -p "$repo_root/build"
fixture=$(mktemp -d "$repo_root/build/preset-selection.XXXXXX")
trap 'rm -rf -- "$fixture"' EXIT
cp "$presets" "$fixture/CMakePresets.json"
for preset in debug asan tsan x86_64-linux-gnu-release host-curl; do
  mkdir -p "$fixture/build/$preset"
  cat >"$fixture/build/$preset/CTestTestfile.cmake" <<'TESTS'
add_test(runtime cmake -E true)
set_tests_properties(runtime PROPERTIES LABELS local TIMEOUT 5)
add_test(policy cmake -E true)
set_tests_properties(policy PROPERTIES LABELS host-policy TIMEOUT 5)
add_test(service cmake -E true)
set_tests_properties(service PROPERTIES LABELS e2e TIMEOUT 5)
TESTS
done
for preset in asan tsan x86_64-linux-gnu-release; do
  (cd "$fixture" && ctest --preset "$preset" --show-only=json-v1) >"$fixture/selected"
  grep -F '"name" : "runtime"' "$fixture/selected" >/dev/null
  ! grep -F '"name" : "policy"' "$fixture/selected"
  ! grep -F '"name" : "service"' "$fixture/selected"
done
(cd "$fixture" && ctest --preset debug --show-only=json-v1) >"$fixture/selected"
grep -F '"name" : "runtime"' "$fixture/selected" >/dev/null
grep -F '"name" : "policy"' "$fixture/selected" >/dev/null
! grep -F '"name" : "service"' "$fixture/selected"
(cd "$fixture" && ctest --preset e2e --show-only=json-v1) >"$fixture/selected"
grep -F '"name" : "service"' "$fixture/selected" >/dev/null
! grep -F '"name" : "runtime"' "$fixture/selected"
! grep -F '"name" : "policy"' "$fixture/selected"
