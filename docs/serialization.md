# Serialization and allocators

Examples reuse the mapped schema from the [quick start](../README.md#quick-start). Buffer/owned-output APIs materialize their results; sinks, files and generators support incremental output.

## Serialize one mapped object

```c
char buffer[256];
size_t written = 0u;
lonejson *lj;
lonejson_config config;
lonejson_error error;

config = lonejson_default_config();
config.write_pretty = 1;
lj = lonejson_new(&config, &error);
if (lj == NULL) {
  return 1;
}

if (lonejson_serialize_buffer(
      lj, &user_doc_map, &doc, buffer, sizeof(buffer), &written,
      &error) != LONEJSON_STATUS_OK) {
  lonejson_free(lj);
  return 1;
}
lonejson_free(lj);
```

## Emit JSON Lines

```c
user_doc docs[2];
char out[256];
size_t written = 0u;

lonejson_serialize_jsonl_buffer(
  lj, &user_doc_map, docs, 2u, 0u, out, sizeof(out), &written, &error);
```


## Pull-style generation

The generator surface is the transport-facing serializer primitive. It is
bounded and pull-based rather than sink-driven, so adapters like curl upload do
not need worker threads or hidden full-payload buffers. It supports the normal
mapped serializer surface plus `lonejson_source`, `lonejson_spooled`, and
`lonejson_json_value` fields. `json_value` fields are streamed through the
generator directly instead of silently falling back to buffered serialization.

```c
lonejson *lj = lonejson_new(NULL, &error);
lonejson_generator generator;
unsigned char chunk[4096];
size_t out_len;
int eof;

status = lonejson_generator_init(lj, &generator, &user_doc_map, &doc);
if (status != LONEJSON_STATUS_OK) {
  fprintf(stderr, "generator init failed: %s\n", generator.error.message);
  lonejson_free(lj);
  return 1;
}

eof = 0;
while (!eof) {
  status = lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof);
  if (status != LONEJSON_STATUS_OK) {
    fprintf(stderr, "generator read failed: %s\n", generator.error.message);
    break;
  }
  fwrite(chunk, 1u, out_len, stdout);
}
lonejson_generator_cleanup(&generator);
lonejson_free(lj);
```


## Pretty printing

Pretty printing is optional and uses two-space indentation:

```c
lonejson_config config = lonejson_default_config();
config.write_pretty = 1;
```

The same option applies to normal JSON serializers and to JSONL serializers,
where each record is formatted independently and then terminated by `\n`.

When you use the default alloc-returning serializer, release the returned
buffer with `LONEJSON_FREE()`. Alloc-returning and owned-buffer serializers
enforce the runtime's `write_max_output_bytes`; set a larger explicit value
when you intentionally materialize a larger document.

```c
lonejson_config config = lonejson_default_config();
config.write_max_output_bytes = 16u * 1024u * 1024u;
lonejson *lj = lonejson_new(&config, &error);

char *json =
    lonejson_serialize_alloc(lj, &user_doc_map, &doc, NULL, &error);
if (json == NULL) {
  return 1;
}

puts(json);
LONEJSON_FREE(json);
```

Custom allocators are configured on the runtime. For
alloc-returning serializers with a custom allocator, use the owned-buffer
variants:

```c
static void *my_malloc(void *ctx, size_t size) { return malloc(size); }
static void *my_realloc(void *ctx, void *ptr, size_t size) {
  return realloc(ptr, size);
}
static void my_free(void *ctx, void *ptr) { free(ptr); }

lonejson_allocator allocator = lonejson_default_allocator();
lonejson_config config = lonejson_default_config();

allocator.malloc_fn = my_malloc;
allocator.realloc_fn = my_realloc;
allocator.free_fn = my_free;
config.allocator = &allocator;

lonejson *lj = lonejson_new(&config, &error);

lonejson_owned_buffer owned = lonejson_default_owned_buffer();
if (lonejson_serialize_owned(lj, &user_doc_map, &doc, &owned,
                             &error) == LONEJSON_STATUS_OK) {
  puts(owned.data);
}
lonejson_owned_buffer_free(&owned);
lonejson_free(lj);
```
