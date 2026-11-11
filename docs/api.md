# C API and ownership

See the [public header](https://github.com/c89systems/lonejson/blob/trunk/include/lonejson.h) for declarations and the [receiver audit](receiver-api.md) for method coverage. Snippets use the schema from the [quick start](../README.md#quick-start).

The API is arranged in a few clear families. Mapping is done with
`LONEJSON_FIELD_*` and `LONEJSON_MAP_DEFINE(...)`. One-shot parsing is handled
by `lonejson_parse_cstr`, `lonejson_parse_buffer`, `lonejson_parse_reader`,
`lonejson_parse_filep`, and `lonejson_parse_path`, with `lonejson_validate_*`
variants available when syntax validation is all you need. Streaming uses
`lonejson_stream_open_*`, `lonejson_stream_next`, `lonejson_stream_error`, and
`lonejson_stream_close`. Selected-array cursors use
`lonejson_array_stream_open_*`, `lonejson_array_stream_next`,
`lonejson_array_stream_next_value`, `lonejson_array_stream_error`, and
`lonejson_array_stream_close`. Push-fed selected-array parsing uses
`lonejson_array_stream_open_push`, `lonejson_array_stream_push*`, and
`lonejson_array_stream_finish*`. Streaming selected-array rewrite uses
`lonejson_array_rewrite_*`. Protocol helpers are available through
`lonejson_sse_*` and `lonejson_multipart_*`. Serialization is exposed through
`lonejson_serialize_*`, `lonejson_serialize_jsonl_*`, and the pull-style
`lonejson_generator_init/read/cleanup` trio. Ownership and reuse of mapped
values is handled by `lonejson_cleanup` and `lonejson_reset`.

Large-value handling is expressed explicitly. `lonejson_spooled` is for inbound
values that may spill to disk, with thresholds chosen from the runtime's named
spool classes. `lonejson_source` and the corresponding
`lonejson_source_set_*` helpers are for outbound values that should be
serialized directly from an existing path, `FILE *`, or file descriptor.
`lonejson_json_value` covers the related case where a typed envelope needs to
embed one already-formed JSON value directly, and `lonejson_visit_value_*`
covers the lower-level case where one arbitrary JSON value should be visited
without a schema. Parse, write, and arbitrary-value limits are configured on
the instantiated `lonejson` runtime through `lonejson_config`.

Core mapped operations use an instantiated runtime. You can call the
free functions such as `lonejson_parse_cstr(lj, ...)` or the equivalent method
pointer route `lj->parse_cstr(lj, ...)`; they are intentionally the same API
surface. Treat `lonejson *` as an owner-only handle: do not copy `struct
lonejson` by value, and only free the original pointer returned by
`lonejson_new()`.

Receiver operations are also available on their owning shells. For example,
`items->set_record_reuse(items, 1, &error)` configures a selected-array stream,
and `items->error_fn(items)` returns its last error (`error` is already a data
member). Runtime methods include mapped-record assignment and array append,
SSE JSON delivery, generator measurement and writer-generator construction,
and the enabled OAuth2/OIDC response parsers. Initialization uses free functions
or the owning runtime, without requiring methods on uninitialized storage.
Receiver layouts are part of the shared-library ABI; use matching headers
and library builds. See [receiver API coverage](receiver-api.md).

SSE and multipart parser handles also expose receiver methods: `push`, `finish`
and `close`. SSE additionally exposes `push_json`, `finish_json`,
`push_json_value` and `finish_json_value`; these take the receiver first and
runtime second, for example `sse->finish_json(sse, lj, map, dst, options,
callback, user, &error)`. Create handles with `lonejson_sse_open()` or
`lonejson_multipart_open()` and close them through their method or free function.
Parser state stays private and delivery remains incremental.

Serializer-owned output APIs such as `lonejson_serialize_alloc()` and
`lonejson_serialize_owned()` are materializing APIs and are capped by the
runtime's `write_max_output_bytes`, which defaults to
`LONEJSON_WRITE_MAX_OUTPUT_BYTES` (8 MiB). Fixed caller buffers are bounded by
their explicit capacity, and sink/file/generator serializers remain streaming
or caller-bounded rather than using this materialized-output cap.

Concurrency follows the usual runtime/handle split. Different threads may call
into the same `lonejson *` concurrently, or use different runtimes
concurrently, as long as they operate on distinct mutable handles or
destination values. Mutable stateful handles such as streams, writers,
generators, spools, and `lonejson_json_value` instances may move between
threads, but they are not concurrently callable without external
synchronization.


## Parse limits and reusable storage

For runtimes that use `STRING_ALLOC` or other lonejson-owned dynamic
destinations, `lonejson_default_config()` currently applies these
ceilings:

- `max_dynamic_string_bytes = 128 KiB`
- `max_alloc_bytes = 1 MiB`

Set either field to `0` to disable that specific ceiling for one runtime.

Reused non-empty `STRING_FIXED` fields under `clear_destination = 0` stage the
replacement before commit so parse failures do not clobber the old value. If
you want that path to stay allocation-free, provide caller-owned scratch in the
runtime config that is at least as large as the fixed field capacity. Include
`<stdlib.h>` for this caller-owned heap allocation:

```c
char *fixed_scratch = (char *)malloc(8192u);
if (fixed_scratch == NULL) {
  return 1;
}

config.clear_destination_by_default = 0;
config.fixed_string_scratch = fixed_scratch;
config.fixed_string_scratch_size = 8192u;
```

Keep the scratch alive while the runtime uses it, then call
`free(fixed_scratch)` after freeing the runtime. Without caller scratch,
lonejson allocates temporary staging storage for that
field and counts it against `max_alloc_bytes`.

The Lua binding exposes the same reuse control through
`lonejson.fixed_string_scratch(size)`, passed into `lonejson.new({...})`.


## Short aliases

`lonejson` provides optional short aliases. Functions and types are available as
`lj_*`; macros and constants are available as `LJ_*`. They are enabled by
default and can be disabled with:

```c
#define LONEJSON_DISABLE_SHORT_NAMES 1
```


## Base64

Base64 helpers are available independently of JWT. Use
`lonejson_base64_encode()`/`lonejson_base64_decode()` for caller-provided
buffers, or the `_sink` variants when the encoded/decoded data should be
streamed to a callback instead of materialized by the helper. Variants are
explicit: `LONEJSON_BASE64_STANDARD`, `LONEJSON_BASE64_STANDARD_RAW`,
`LONEJSON_BASE64_URL`, and `LONEJSON_BASE64_URL_RAW`. JWT/JWS segments use
`LONEJSON_BASE64_URL_RAW`. Lua exposes the materialized helpers as
`lonejson.base64_encode(data, variant)` and
`lonejson.base64_decode(text, variant)`, where `variant` is `"standard"`,
`"standard_raw"`, `"url"`, or `"url_raw"`; `"jwt"` is accepted as an alias for
`"url_raw"`.

## Checked record lifecycle

`lonejson_init(runtime, map, record, &error)`, `lonejson_reset(...)` and
`lonejson_cleanup(map, record, &error)` return `lonejson_status`. Their receiver
methods and short aliases return the same status. Check initialization before
using mapped handles, and check reset before reusing a record.

Schema analysis and record lifecycle traversal use bounded inline workspaces
and grow on the heap for deep schemas or recursive object arrays. If traversal
workspace allocation fails, the record remains unchanged; restore allocator
availability and retry. Inline object cycles and cyclic record storage return
`LONEJSON_STATUS_INVALID_ARGUMENT`. Recursive array schemas remain supported.
The JSON parser's input depth limit does not limit schema initialization.

These signatures were introduced in v0.48.0 with shared-library ABI 28.
Rebuild downstream consumers with matching headers and library when the
[receiver ABI](receiver-api.md) changes.
