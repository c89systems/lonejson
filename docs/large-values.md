# Large values and arbitrary JSON

Use these field types when payloads outgrow fixed records. Source-backed output and visitors deliver incrementally; spools and explicit capture retain data by choice.

## Inbound spool-backed fields

Use spool-backed fields when a JSON string or base64 payload may be too large to
retain in memory comfortably:

```c
typedef struct ingest_doc {
  lonejson_spooled body;
} ingest_doc;

static const lonejson_field ingest_doc_fields[] = {
  LONEJSON_FIELD_STRING_STREAM_REQ(ingest_doc, body, "body")
};

LONEJSON_MAP_DEFINE(ingest_doc_map, ingest_doc, ingest_doc_fields);
```

`LONEJSON_FIELD_STRING_STREAM_REQ(...)` uses the runtime's default spool class:

- `memory_limit = 64 KiB` before spilling to disk
- `max_bytes = 10 MiB` total logical payload

To change either threshold for one field family, move that field onto a named
runtime spool class:

```c
static const lonejson_field ingest_doc_fields[] = {
  LONEJSON_FIELD_STRING_STREAM_CLASS(ingest_doc, body, "body",
                                     LONEJSON_SPOOL_CLASS_BLOB)
};

lonejson_config config = lonejson_default_config();
config.spool_blob.memory_limit = 64u * 1024u;
config.spool_blob.max_bytes = 32u * 1024u * 1024u;
config.spool_blob.temp_dir = "/tmp";
```

Set `config.spool_blob.max_bytes = 0` to disable the total spool-size ceiling
for that class.

The resulting `lonejson_spooled` value remains in memory when it is small and
spills to a temporary file when it grows past the configured threshold. The
same handle can then be serialized back into JSON or streamed back out as raw
text or raw bytes.

## Outbound source-backed fields

Use source-backed fields when the JSON field value already exists as a file,
file descriptor, or `FILE *`, and you want lonejson to serialize it directly
without creating a spool first.

Text source:

```c
typedef struct outbound_text_doc {
  lonejson_source body;
} outbound_text_doc;

static const lonejson_field outbound_text_fields[] = {
  LONEJSON_FIELD_STRING_SOURCE_REQ(outbound_text_doc, body, "body")
};
```

Binary source encoded as base64:

```c
typedef struct outbound_bytes_doc {
  lonejson_source payload;
} outbound_bytes_doc;

static const lonejson_field outbound_bytes_fields[] = {
  LONEJSON_FIELD_BASE64_SOURCE_REQ(outbound_bytes_doc, payload, "payload")
};
```

Attach the backing source:

```c
lonejson_source_set_path(&doc.payload, "payload.bin", &error);
```

Ownership is explicit. lonejson only auto-closes sources it opened itself;
caller-owned `FILE *` and file descriptors remain caller-owned.

## Embedded arbitrary JSON values

Use `lonejson_json_value` when one mapped field should contain arbitrary JSON
rather than a JSON string:

```c
typedef struct query_doc {
  char namespace_[16];
  lonejson_json_value selector;
  lonejson_json_value fields;
} query_doc;

static const lonejson_field query_doc_fields[] = {
  LONEJSON_FIELD_STRING_FIXED_REQ(query_doc, namespace_, "namespace",
                                  LONEJSON_OVERFLOW_FAIL),
  LONEJSON_FIELD_JSON_VALUE_REQ(query_doc, selector, "selector"),
  LONEJSON_FIELD_JSON_VALUE(query_doc, fields, "fields")
};
```

You can populate the handle from memory, a reader callback, a `FILE *`, a file
descriptor, or a path. lonejson validates that the handle contains exactly one
JSON value when serializing.

On parse, `lonejson_json_value` is stream-first. Callers must opt into one of
three inbound behaviors before decoding:

- `lonejson_json_value_set_parse_sink(...)` streams compact JSON bytes to a
  caller sink as lonejson validates the nested value incrementally
- `lonejson_json_value_set_parse_visitor(...)` streams structured object,
  array, string, number, boolean, and null events to a caller visitor while
  applying the runtime's JSON-value visitor ceilings
- `lonejson_json_value_enable_parse_capture(...)` explicitly captures compact
  JSON bytes into owned storage for later reuse or re-emission

When using inbound parse sinks, parse visitors, or explicit capture, initialize
the destination first and create the runtime with
`clear_destination_by_default = 0` so the configured `json_value` handles
remain attached. The same supported pattern applies when a `json_value` field
lives inside a nested mapped object or a mapped-array-stream item:

```c
lonejson_config config = lonejson_default_config();
config.clear_destination_by_default = 0;
lonejson *lj = lonejson_new(&config, &error);
```

That means you can prepare `doc.response.payload` once, then reuse the same
mapped destination across multiple parses without losing the configured sink,
visitor, or capture mode.

Visitor mode is the zero-retention parse path for arbitrary embedded JSON:

```c
lonejson_value_visitor visitor = lonejson_default_value_visitor();
visitor.object_begin = on_object_begin;
visitor.object_key_begin = on_object_key_begin;
visitor.object_key_chunk = on_object_key_chunk;
visitor.object_key_end = on_object_key_end;
visitor.string_begin = on_string_begin;
visitor.string_chunk = on_string_chunk;
visitor.string_end = on_string_end;
visitor.number_begin = on_number_begin;
visitor.number_chunk = on_number_chunk;
visitor.number_end = on_number_end;
visitor.boolean_value = on_boolean;
visitor.null_value = on_null;

status = lonejson_json_value_set_parse_visitor(&doc.selector, &visitor, user,
                                               &error);
```

## Visiting Arbitrary JSON Values

Use `lonejson_visit_value_*` when you need to parse one arbitrary JSON value
without mapping it into a typed schema or building a DOM. The visitor API emits
structural events plus chunked decoded strings/object keys and bounded number
tokens.

```c
lonejson_value_visitor visitor = lonejson_default_value_visitor();
visitor.object_begin = on_object_begin;
visitor.object_key_begin = on_key_begin;
visitor.object_key_chunk = on_key_chunk;
visitor.object_key_end = on_key_end;
visitor.string_begin = on_string_begin;
visitor.string_chunk = on_string_chunk;
visitor.string_end = on_string_end;
visitor.number_begin = on_number_begin;
visitor.number_chunk = on_number_chunk;
visitor.number_end = on_number_end;
visitor.boolean_value = on_boolean;
visitor.null_value = on_null;

lonejson_config config = lonejson_default_config();
config.json_value_max_depth = 32;
lonejson *lj = lonejson_new(&config, &error);

status = lonejson_visit_value_cstr(lj, json, &visitor, user, &error);
```

Default limits are intentionally conservative. Strings, object keys, and total
input size remain bounded; excessively deep or oversized input fails cleanly
instead of allocating without limit.

In the Lua binding, nested `json_value` nulls round-trip through the singleton
`lj.json_null` so they remain distinguishable inside arrays and objects. A
whole-field JSON `null` still maps to Lua `nil`.

For public structs, prefer lonejson's initializers and default helpers over
manual `memset` or `{0}`:

- `lonejson_init(lj, map, &value, NULL)` for mapped structs
- `lonejson_json_value_init`, `lonejson_source_init`, `lonejson_spooled_init`
  for explicit handles
- `lonejson_config.allocator` before initializing handles when a custom
  allocator should own future allocations
- `lonejson_default_config()`, `lonejson_default_value_visitor()`,
  `lonejson_default_read_result()`, and `lonejson_default_allocator()`
  for configuration/result structs

`lonejson_error` is output-only and does not need prior initialization, but
`lonejson_error_init()` is available when you want an explicit empty state.
