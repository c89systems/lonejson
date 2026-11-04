#!/usr/bin/env bash
# Small native JSON assertions for the local HTTP integration checks.
json_field() {
  local data
  data=$(cat)
  cmake "-DJSON=$data" "-DFIELDS=$1" -P "$repo_root/tests/json_fields.cmake"
}
json_assert() {
  local data pair fields= args=()
  data=$(cat)
  for pair in "$@"; do
    fields+="${fields:+;}${pair%%=*}"
    args+=("-DEXPECT_${pair%%=*}=${pair#*=}")
  done
  cmake "-DJSON=$data" "-DFIELDS=$fields" "${args[@]}" -P "$repo_root/tests/json_fields.cmake"
}
url_query_field() {
  local query=${1#*\?} field=$2 pair value byte result=
  local parts=()
  IFS='&' read -r -a parts <<<"${query%%#*}"
  for pair in "${parts[@]}"; do
    [[ "${pair%%=*}" == "$field" ]] || continue
    value=${pair#*=}
    value=${value//+/ }
    while [[ -n "$value" ]]; do
      if [[ "$value" == %* ]]; then
        byte=${value:1:2}
        [[ "$byte" =~ ^[[:xdigit:]]{2}$ && "$byte" != 00 ]] || return 1
        printf -v byte '%b' "\\x$byte"
        result+=$byte
        value=${value:3}
      else
        result+=${value:0:1}
        value=${value:1}
      fi
    done
    printf '%s\n' "$result"
    return 0
  done
  printf 'missing URL query field: %s\n' "$field" >&2
  return 1
}
