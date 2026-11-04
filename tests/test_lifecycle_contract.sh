#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
source "$repo_root/tests/json_helpers.sh"
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/lifecycle-contract.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
[[ $(printf '{"token":"a;b \\\"c","flag":true}' | json_field token) == 'a;b "c' ]]
printf '{"ok":true,"authorized":false,"email":"user@example.com"}' | json_assert ok=true authorized=false email=user@example.com
reject() {
  if "$@" >"$work/rejected" 2>&1; then printf 'invalid input accepted\n' >&2; exit 1; fi
}
reject json_field absent <<<'{}'
reject json_field value <<<'{"value":null}'
reject json_field value <<<'bad json'
reject json_assert ok=true <<<'{"ok":false}'
reject json_assert ok=true <<<'{"ok":"true"}'
[[ $(url_query_field 'http://localhost/?state=x&code=a%2Bb+c%26d#ignored' code) == 'a+b c&d' ]]
reject url_query_field 'http://localhost/?code=%XX' code
reject url_query_field 'http://localhost/?code=%00' code
reject url_query_field 'http://localhost/?state=x' code
mkdir -p "$work/empty" "$work/valid" "$work/repo/scripts"
printf 'add_test(pass "%s" -E true)\nset_tests_properties(pass PROPERTIES TIMEOUT 5 LABELS local)\n' "$(command -v cmake)" >"$work/valid/CTestTestfile.cmake"
reject cmake "-DBUILD_DIR=$work/empty" -P "$repo_root/tests/check_ctest_metadata.cmake"
cmake "-DBUILD_DIR=$work/valid" -P "$repo_root/tests/check_ctest_metadata.cmake"
printf 'add_test(pass "%s" -E true)\n' "$(command -v cmake)" >"$work/valid/CTestTestfile.cmake"
reject cmake "-DBUILD_DIR=$work/valid" -P "$repo_root/tests/check_ctest_metadata.cmake"
cp "$repo_root/scripts/test.sh" "$work/repo/scripts/"
cat >"$work/repo/CMakePresets.json" <<'JSON'
{"version":3,"configurePresets":[{"name":"empty","generator":"Ninja","binaryDir":"${sourceDir}/build"}],"testPresets":[{"name":"empty","configurePreset":"empty"}]}
JSON
mkdir -p "$work/repo/build"
reject bash "$work/repo/scripts/test.sh" empty
grep -F 'No tests were found' "$work/rejected" >/dev/null
printf 'PASS lifecycle assertions, metadata, and empty-suite rejection\n'
