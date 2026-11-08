# Examples

`make build` writes the standalone C example binaries to `examples/bin/`. That
is the intended repository entrypoint for the non-curl examples.

`parse_string.c` and `serialize_jsonl.c` are intentional single-header
examples. They define `LONEJSON_IMPLEMENTATION` and should be compiled against a
generated standalone header:

```sh
mkdir -p bin
cc -I ../build/debug/generated/single-header/include \
  -o bin/parse_string parse_string.c
```

The remaining standalone examples are linked-library examples. After `make
build`, compile them against the built archive and the normal public header:

```sh
mkdir -p bin
cc -I ../include -o bin/parse_reader parse_reader.c \
  ../build/debug/liblonejson.a
cc -I ../include -o bin/serialize_file serialize_file.c \
  ../build/debug/liblonejson.a
```

The same pattern applies to:

* `array_stream.c` (streams the `items` array from
  `tests/fixtures/array_stream/issues.json` item by item)
* `parse_file.c`
* `parse_reader.c`
* `push_parser.c` (object-framed streaming reader example)
* `generator_pull.c` (pull-style generator example for transport adapters)
* `json_value_buffer.c` (embeds selector/fields JSON directly from memory)
* `json_value_nested_parse.c` (configures a nested mapped `json_value` parse
  sink once, then reuses the same destination across multiple parses)
* `json_value_parse.c` (enables explicit parse capture for opaque embedded JSON
  values and re-emits them)
* `json_value_visitor.c` (streams parsed embedded JSON through visitor
  callbacks without retaining raw bytes)
* `sse_json_value_stream.c` (filters SSE events by `event:` name and streams
  selected event bodies as arbitrary JSON values without a schema map)
* `json_value_source.c` (streams embedded JSON values from filesystem paths)
* `nullable_fields.c` (maps optional nullable primitive fields with explicit
  presence bits and omits absent/null values on serialization)
* `spooled_text.c` (forces spill-to-disk, shows temp path, and confirms cleanup)
* `spooled_bytes.c` (forces Base64 decode spill-to-disk and confirms cleanup)
* `source_text.c` (serializes a caller-declared JSON text field from a filesystem path)
* `source_bytes.c` (serializes a caller-owned fd as a Base64 JSON field)
* `value_rewrite_replace_with.c` (rewrites one selected JSON value by
  inspecting the old value and emitting a replacement through the writer)
* `serialize_string.c`
* `serialize_file.c`
* `fixed_storage.c`

`parse_string.c` and `serialize_jsonl.c` are the embedded single-header
variants in the tree; the rest model linked-library use.

The public API also includes streaming selected-array rewrites
(`lonejson_array_rewrite_*`), whole-value rewrites
(`lonejson_value_rewrite_*`), incremental Server-Sent Events parsing
(`lonejson_sse_*`), and incremental MIME multipart parsing
(`lonejson_multipart_*`). Those surfaces are covered by the integration tests
under `tests/` even when there is no dedicated standalone example program yet.

The linked-library examples intentionally use lonejson's public initializers
and runtime constructor instead of manual `memset` or `{0}` for public
structs: `lonejson_new` / `lj_new`, `lonejson_init` / `lj_init` for mapped
values, `*_init` for handles, and `lonejson_default_*` / `lj_default_*` for
config, visitor, and read-result structs. They also stick to the instance-first
surface only: free functions take `lonejson *` explicitly, and the equivalent
runtime method pointers are available when that call style fits better.

The curl examples are CMake development targets. `make curl-examples` resolves
the pinned host bundle, builds them with the selected toolchain, and writes the
binaries to `examples/bin/` for direct execution:

```sh
make curl-examples
```

`curl_put.c` streams its JSON body through `CURLOPT_READFUNCTION` and also
registers `lonejson_curl_seek_callback` with `CURLOPT_SEEKFUNCTION`. This lets
curl replay a mapped upload after a redirect when its sources are rewindable;
the example keeps the upload adapter as both curl's read and seek userdata.
The adapter reports an unknown size, so it remains streaming rather than
materializing or pre-counting the body.

Start the local test rig first:

```sh
make dev-up
```

The same rootless Podman Kube pod also starts a mock OIDC/OAuth2 provider and a lightweight
API fixture. Run the live OIDC/JWKS bearer-flow check with:

```sh
make test-oidc-e2e
```

Run the server-local M2M Basic/Bearer credential check with:

```sh
make test-m2m-e2e
```

## Lua binding

Build with `make lua-rock`, then follow the [Lua setup guide](../docs/lua.md#run-the-examples)
to run these examples against the installed SDK on Linux or Darwin.

| Example | Demonstrates |
| --- | --- |
| [`lua_binding.lua`](lua_binding.lua) | Schema DSL, reusable records, streams, rewrites, `json_value` and spool-backed fields. |
| [`lua_nullable.lua`](lua_nullable.lua) | Nullable primitive fields and omission of `nil`/`lj.json_null` values on encode. |
| [`lua_json_value_nested.lua`](lua_json_value_nested.lua) | Nested arbitrary JSON values with record reuse. |
| [`lua_json_value_object_array.lua`](lua_json_value_object_array.lua) | Arbitrary JSON in object-array elements; omitted arrays persist and present arrays replace prior contents. |

See [runtime limits](../docs/api.md#parse-limits-and-reusable-storage) and
[large values](../docs/large-values.md) for string, allocation and spool budgets.
