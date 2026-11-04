#!/usr/bin/env bash
set -euo pipefail
repo_root=${1:?repository root required}
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/rewind-fixture.XXXXXX")
server_pid=
cleanup() {
  if [[ -n "$server_pid" ]]; then
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
  rm -rf -- "$work"
}
trap cleanup EXIT
python3 -B "$repo_root/tests/fixtures/rewind_server.py" "$work/port" >"$work/server.log" 2>&1 &
server_pid=$!
for ((attempt=0; attempt<100; attempt++)); do
  [[ ! -s "$work/port" ]] || break
  kill -0 "$server_pid" || { cat "$work/server.log" >&2; exit 1; }
  sleep 0.05
done
[[ -s "$work/port" ]] || { printf 'rewind fixture did not become ready\n' >&2; exit 1; }
url="http://127.0.0.1:$(cat "$work/port")"
request() {
  local expected=$1 method=$2 path=$3 body=${4:-} framing=${5:-known} status
  local args=()
  if [[ "$method" != GET ]]; then
    args+=(--data-binary "$body")
    if [[ "$framing" == chunked ]]; then
      args+=(-H 'Transfer-Encoding: chunked' -H 'Content-Length:')
    fi
  fi
  status=$(curl -sS --max-time 3 --http1.1 -X "$method" "${args[@]}" \
    -D "$work/headers" -o "$work/body" -w '%{http_code}' "$url$path")
  [[ "$status" == "$expected" ]] || {
    printf '%s %s: expected %s, got %s\n' "$method" "$path" "$expected" "$status" >&2
    cat "$work/body" >&2; exit 1;
  }
}
for framing in known chunked; do
  for hop in 0 1 2; do
    status=308; [[ "$hop" != 2 ]] || status=200
    request "$status" PATCH "/rewind/$framing/PATCH/308/3/$framing/$hop" '{"payload":"aaa"}' "$framing"
    tr -d '\r' <"$work/headers" | grep -ix "X-Lonejson-Rewind-Verified: $hop"
  done
  request 200 GET "/rewind-result/$framing"
  [[ "$(cat "$work/body")" == '3 3' ]]
done
request 400 PUT /rewind/corrupt/PUT/307/3/known/0 '{"payload":"aab"}'
! grep -i 'X-Lonejson-Rewind-Verified:' "$work/headers"
request 200 GET /rewind-result/corrupt
[[ "$(cat "$work/body")" == '1 0' ]]
request 400 POST /rewind/method/PUT/307/3/known/0 '{"payload":"aaa"}'
request 400 PUT /rewind/framing/PUT/307/3/chunked/0 '{"payload":"aaa"}'
request 400 PUT /rewind/order/PUT/307/3/known/1 '{"payload":"aaa"}'
request 404 GET /rewind-result/missing
request 307 PUT /rewind/result/PUT/307/3/known/0 '{"payload":"aaa"}'
request 200 GET /rewind-result/result
[[ "$(cat "$work/body")" == '1 1' ]]
request 404 GET /rewind-result/result
printf 'PASS rewind fixture replay, corruption, method, framing, order, consumed results\n'
