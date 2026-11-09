# Streaming and rewriting

Examples use `user_doc` and `user_doc_map` from the [quick start](../README.md#quick-start). Callbacks and transport handles are supplied by the application. See [C API and ownership](api.md) for runtime and handle lifetimes.

## Read an object-framed stream

`lonejson` stream parsing is object-framed rather than delimiter-framed. It
ignores whitespace between objects, so `{"a":1}{"a":2}`, pretty-printed
objects separated by blank lines, and JSONL-style one-object-per-line input all
fit the same model. This cursor emits mapped top-level objects only; it is not
an iterator for arbitrary scalar, array, or object values. Use
`lonejson_visit_value_*` to process exactly one arbitrary JSON value without a
schema.

```c
lonejson_stream *stream;
lonejson *lj;
user_doc doc;
lonejson_error error;

lj = lonejson_new(NULL, &error);
if (lj == NULL) {
  return 1;
}

lonejson_init(lj, &user_doc_map, &doc, NULL);
stream = lonejson_stream_open_fd(lj, &user_doc_map, fd, &error);
if (stream == NULL) {
  lonejson_free(lj);
  return 1;
}

for (;;) {
  lonejson_stream_result result = lonejson_stream_next(stream, &doc, &error);
  if (result == LONEJSON_STREAM_OBJECT) {
    /* process doc */
    lonejson_cleanup(&user_doc_map, &doc, NULL);
    continue;
  }
  if (result == LONEJSON_STREAM_EOF) {
    break;
  }
  /* inspect lonejson_stream_error(stream) */
  break;
}

lonejson_stream_close(stream);
lonejson_cleanup(&user_doc_map, &doc, NULL);
lonejson_free(lj);
```

## Read a selected array stream

Use a selected-array cursor when one valid JSON document contains an array that
should be consumed item by item. `""` selects a root array. A non-empty v1 path
selects one direct root object key such as `"items"`; implicit fan-out paths
such as `"boards.items"` are rejected so traversal semantics stay explicit.

Mapped item delivery parses each item directly into the destination map:

```c
lonejson_array_stream *items;
lonejson *lj;
user_doc doc;
lonejson_error error;

lj = lonejson_new(NULL, &error);
if (lj == NULL) {
  return 1;
}

lonejson_init(lj, &user_doc_map, &doc, NULL);
items = lonejson_array_stream_open_path(lj, "items", "/tmp/users.json",
                                        &error);
if (items == NULL) {
  lonejson_free(lj);
  return 1;
}

for (;;) {
  lonejson_array_stream_result result =
      lonejson_array_stream_next(items, &user_doc_map, &doc, &error);
  if (result == LONEJSON_ARRAY_STREAM_ITEM) {
    /* process one mapped item */
    lonejson_cleanup(&user_doc_map, &doc, NULL);
    continue;
  }
  if (result == LONEJSON_ARRAY_STREAM_EOF) {
    break;
  }
  /* inspect lonejson_array_stream_error(items) */
  break;
}

lonejson_array_stream_close(items);
lonejson_cleanup(&user_doc_map, &doc, NULL);
lonejson_free(lj);
```

For an initialized record with caller-owned fixed-capacity backing or configured
stateful fields, call `lonejson_array_stream_set_record_reuse(items, 1, &error)`
after opening the cursor and before its first next/push/finish operation. This
selects reuse for that cursor while retaining the borrowed runtime's allocator,
limits and clearing policy for other operations. For example, after initializing
`doc` and configuring its backing storage:

```c
if (items->set_record_reuse(items, 1, &error) !=
    LONEJSON_STATUS_OK) {
  lonejson_array_stream_close(items);
  lonejson_cleanup(&user_doc_map, &doc, NULL);
  return;
}
while (lonejson_array_stream_next(items, &user_doc_map, &doc, &error) ==
       LONEJSON_ARRAY_STREAM_ITEM) {
  /* Consume doc before resetting it for the next item. */
  lonejson_reset(lj, &user_doc_map, &doc, NULL);
}
lonejson_array_stream_close(items);
lonejson_cleanup(&user_doc_map, &doc, NULL); /* Also handles a partial final record. */
```

The option defaults to zero, which inherits the runtime's existing behavior.
With reuse enabled, the caller also resets the record inside push item callbacks
and owns cleanup after callback cancellation or parse failure. The stream does
not automatically reset or clean that record. Zero-length pushes and WOULD_BLOCK
results start processing too, so configure reuse before them. Raw-value and
string delivery are unaffected. The short alias is
`lj_array_stream_set_record_reuse`.

With an open array cursor, raw item delivery explicitly captures each selected
item as a `lonejson_json_value`:

```c
lonejson *lj;
lonejson_json_value value;

lj = lonejson_new(NULL, &error);
lonejson_json_value_init(lj, &value);
while (lonejson_array_stream_next_value(items, &value, &error) ==
       LONEJSON_ARRAY_STREAM_ITEM) {
  lonejson_json_value_write_to_sink(&value, sink_fn, user, &error);
  lonejson_json_value_reset(&value);
}
lonejson_json_value_cleanup(&value);
lonejson_free(lj);
```

Push-fed selected-array streams fit write-callback transports such as libcurl.
For string arrays, `lonejson_array_stream_push_string` exposes true decoded
string chunks through begin/chunk/end callbacks. If a caller wants one callback
per key-like string item, `lonejson_array_stream_push_string_items` builds only
the current string through that chunk path under the default string-size cap,
then invokes the item callback. `lonejson_curl_string_items_parse_*` provides the
same bounded item-callback shape for `CURLOPT_WRITEFUNCTION`.

Mapped array-stream fields are useful when a normal mapped object contains a
large array of nested objects that should be consumed while the enclosing
document is parsed:

```c
typedef struct envelope_doc {
  lonejson_mapped_array_stream items;
} envelope_doc;

static lonejson_status on_item(void *user, void *item, lonejson_error *error) {
  const user_doc *doc = (const user_doc *)item;
  (void)user;
  (void)error;
  /* process doc */
  return LONEJSON_STATUS_OK;
}

static const lonejson_field envelope_fields[] = {
  LONEJSON_FIELD_MAPPED_ARRAY_STREAM_REQ(envelope_doc, items, "items")
};

LONEJSON_MAP_DEFINE(envelope_map, envelope_doc, envelope_fields);

envelope_doc envelope;
lonejson *lj;
user_doc item;
lonejson_mapped_array_stream_handler handler;
lonejson_config config;

config = lonejson_default_config();
config.clear_destination_by_default = 0;
lj = lonejson_new(&config, &error);
lonejson_init(lj, &envelope_map, &envelope, NULL);
lonejson_init(lj, &user_doc_map, &item, NULL);
handler.item_map = &user_doc_map;
handler.item_dst = &item;
handler.item = on_item;
handler.user = NULL;
lonejson_mapped_array_stream_set_handler(&envelope.items, &handler, &error);
lonejson_parse_path(lj, &envelope_map, &envelope, "/tmp/envelope.json",
                    &error);
lonejson_cleanup(&user_doc_map, &item, NULL);
lonejson_cleanup(&envelope_map, &envelope, NULL);
lonejson_free(lj);
```

## Rewrite a selected array while streaming

Use `lonejson_array_rewrite_*` when one selected array should be filtered,
patched, appended to, or rewritten while the surrounding JSON document is
validated and re-emitted. `NULL` or `""` selects a root array. Object paths use
dot-separated keys. Fan-out through arrays is explicit with `[]`, for example
`"boards[].items"`.

```c
lonejson *lj;
static lonejson_status keep_item(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  (void)user;
  (void)context;
  (void)item;
  (void)error;
  result->action = LONEJSON_ARRAY_REWRITE_KEEP;
  return LONEJSON_STATUS_OK;
}

lonejson_array_rewrite_options options = {0};
lonejson_json_value item_value;

lj = lonejson_new(NULL, &error);
lonejson_json_value_init(lj, &item_value);
options.item_value = &item_value;
options.item = keep_item;

status = lonejson_array_rewrite_path(lj, "items", "input.json", "output.json",
                                     &options, &error);
lonejson_json_value_cleanup(&item_value);
lonejson_free(lj);
```

Replacement, insertion, and appended values can come from mapped structs or
rewindable `lonejson_json_value` handles. Output is compact canonical JSON;
atomic replacement of files remains the caller's responsibility.

## Replace one selected value from the old value

Use `lonejson_value_rewrite_*` when the selected value itself should be kept,
dropped, statically replaced, or replaced from a callback. `REPLACE_WITH` is
for old-value-dependent mutations: lonejson streams and validates the document,
reports whether the target existed, and gives scalar metadata to the callback.
The callback emits exactly one replacement through `lonejson_writer`, so JSON
syntax remains owned by lonejson.

```c
/* Excerpt only; the complete example includes integer parsing and errors. */
static lonejson_status increment(
    lonejson_writer *writer, const lonejson_value_rewrite_old_value *old,
    void *user, lonejson_error *error) {
  lonejson_int64 delta = *(const lonejson_int64 *)user;

  if (!old->present) {
    return lonejson_writer_i64(writer, delta, error);
  }
  if (old->type != LONEJSON_VALUE_NUMBER) {
    return LONEJSON_STATUS_TYPE_MISMATCH;
  }
  /* Parse old->number according to the caller's numeric policy. */
  return lonejson_writer_i64(writer, parsed_old_number + delta, error);
}

lonejson_value_rewrite_selector_options options = {0};
lonejson_owned_buffer output;
lonejson_int64 delta = 5;

lonejson_owned_buffer_init(&output);
options.selector = "meta.version";
options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
options.replace = increment;
options.replace_user = &delta;

status = lonejson_value_rewrite_selector_buffer(
    lj, input, input_len, lonejson_owned_buffer_sink, &output, &options, &error);
lonejson_owned_buffer_free(&output);
```

Structured old values are observable through `old_value_visitor`; visitor
events are streamed and balanced, and the complete old value is not materialized.
See `examples/value_rewrite_replace_with.c` for a complete integer increment
program.

## Parse Server-Sent Events and multipart streams

`lonejson_sse_*` incrementally parses Server-Sent Events. `lonejson_sse_push`
streams `data:` payload chunks through callbacks as bytes arrive, while
`lonejson_sse_push_json` parses selected event data directly into a mapped
destination without materializing the full event body.

When a schema map is not the right abstraction, `lonejson_sse_push_json_value`
and `lonejson_sse_finish_json_value` stream selected event bodies as arbitrary
JSON values through one configured `lonejson_json_value` handle. That reuses
the existing parse sink, parse visitor, and explicit capture modes without
forcing a destination struct.

```c
lonejson_sse *sse = lonejson_sse_open(NULL, &error);
lonejson_sse_handler handler = { on_sse_begin, on_sse_data, on_sse_end };

lonejson_sse_push(sse, bytes, len, &handler, user, &error);
lonejson_sse_finish(sse, &handler, user, &error);
lonejson_sse_close(sse);
```

`lonejson_multipart_*` incrementally parses MIME multipart bodies from a
`Content-Type` value containing a boundary. Headers are retained only for the
current part, and part bodies are streamed through the callback path.

```c
lonejson_multipart *mp =
    lonejson_multipart_open(content_type, NULL, &error);
lonejson_multipart_handler handler = { on_part_begin, on_part_data,
                                       on_part_end };

lonejson_multipart_push(mp, bytes, len, &handler, user, &error);
lonejson_multipart_finish(mp, &handler, user, &error);
lonejson_multipart_close(mp);
```
