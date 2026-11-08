# Curl upload rewind

## Contract

Mapped JSON uploads support replay without measuring, buffering, or spooling
the document. Applications install `lonejson_curl_seek_callback` as
`CURLOPT_SEEKFUNCTION` and pass the same upload adapter through `CURLOPT_SEEKDATA`
and `CURLOPT_READDATA`. HTTP method, redirect policy, and framing remain owned by
the application. The reported upload size remains unknown (`-1`).

`lonejson_curl_upload_is_rewindable` (receiver: `is_rewindable`) checks all emitted
mapped fields, including nested objects and object arrays, without reading their
contents. Omitted fields do not prevent replay. One-shot JSON readers prevent
replay. File capability probes may seek and restore the current position; a
positive result does not guarantee future file availability or successful I/O.

`lonejson_curl_upload_rewind` (receiver: `rewind`) restarts serialization using
the original map, source, allocator, and write options. It works before reading,
after partial output, and after EOF, and can be repeated. Maps, values, source
contents, and allocator context must remain alive and stable until cleanup.
The runtime handle need not remain alive after successful initialization.

Use the receiver `seek_callback` for direct calls on an initialized adapter;
install the free callback in libcurl. The seek callback accepts only
`(0, SEEK_SET)`. Unsupported offsets/origins and
non-rewindable documents return `CURL_SEEKFUNC_CANTSEEK` without altering the
generator or consuming input. Invalid/inactive adapters and restart failures
return `CURL_SEEKFUNC_FAIL`. The direct rewind operation returns
`LONEJSON_STATUS_INVALID_ARGUMENT` for inactive or non-rewindable adapters.
Restart failures preserve diagnostics in `generator.error`; allocation failure
leaves the old serialization cursor intact. Sources are opened lazily, so later
source failures abort the read callback and preserve its diagnostic.

## Verification

Unit tests cover initial, partial, completed, and repeated replay; byte identity;
unknown size; unsupported seeks; inactive adapters; one-shot and mixed/nested
documents; source errors; allocator failure and cleanup; and bounded memory for
large source-backed uploads across replay. Capability checks must not consume
one-shot readers. Integration tests use real libcurl and a loopback HTTP server
that consumes each request before returning 307 or 308. POST, PUT, and PATCH
must preserve the body and method across replay; one-shot uploads must fail
with `CURLE_SEND_FAIL_REWIND`. Run the standard debug checks, curl-enabled checks,
formatting validation, and native memory checks where available.

## HTTPS end-to-end gate

`make test-curl-e2e` also runs a 72-case matrix through real libcurl, nginx TLS,
and the sink fixture. It is mandatory within `make test-e2e`. Certificate and
hostname verification remain enabled; the endpoint uses
`LONEJSON_NGINX_HTTPS_E2E_PORT` and the CA uses `LONEJSON_CURL_E2E_CAINFO`.

The matrix crosses POST/PUT/PATCH, 307/308, fixed values/file sources/one-shot
JSON readers, 3-byte/256-KiB fields, and known-length/chunked framing. The
fixture consumes and checks exact bytes, method, framing, and replay order
before issuing each of two consecutive redirects. The nginx rewind routes
forward request bodies with request buffering disabled and report the original
client framing through an overwritten fixture header, since nginx may reframe
small requests. Verification receipts
and a separate request-count query prove all three replayable requests arrived
intact, or that exactly one one-shot request arrived before curl reported
`CURLE_SEND_FAIL_REWIND`. Upload size is supplied explicitly by the test for
known-length cases; it is independent of source replayability.

Fixture regression tests reject corrupt bodies, wrong methods, incorrect
framing, and out-of-order replays. The fixture intentionally materializes a
bounded expected body for byte comparison; the upload adapter still streams.
