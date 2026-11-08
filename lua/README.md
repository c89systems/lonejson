# lonejson Lua binding

The Lua binding exposes `lonejson` as a schema-guided JSON decoder and
serializer rather than as a generic `decode(anything)` convenience layer. That
choice is deliberate. The C library is built around mapped object shapes,
reusable records, object-framed streams, selected-array streams, and explicit
handling of large values; the Lua surface keeps those strengths instead of
hiding them behind a looser API that would allocate more and explain less.

At the top level, load the module and instantiate one runtime:

```lua
local lonejson = require("lonejson")
local lj = lonejson.new()
```

The main repository README explains the overall project. This document focuses
on the Lua-facing API and on the practical question of how to use it well from
Lua.

## What the Lua binding is for

The binding supports two broad styles of use.

The first style is table-oriented and convenient. You define a schema once,
decode JSON into ordinary Lua tables, and encode ordinary Lua tables back to
JSON. That is the right choice when simplicity matters more than squeezing the
last allocation out of the pipeline.

The second style is record-oriented and intentionally low-allocation. In that
mode, a schema produces reusable record objects backed by the C mapping layer.
Those records can be cleared and reused, passed through object-framed or
selected-array streams, and combined with spool-backed fields for large text or
byte payloads. This is the intended path for streaming ingestion, ETL, and
services that want the clarity of Lua without giving up the bounded-memory
behavior of the C core.

## Building and testing from this repository

The repository Makefile installs the Lua module into a local LuaRocks tree under
`build/luarocks`:

```sh
make lua-rock
```

Run the Lua integration tests with:

```sh
make lua-test
```

Run the Lua benchmark harness with:

```sh
make lua-bench
```

The Lua benchmark targets are Lua-only. The C benchmark gate remains separate:

```sh
make bench-gate
make lua-bench-gate
```

Broad gates (`test-all`, `prerelease` and `release`) run `bench-check` against
frozen C and Lua baselines for the current host, with an explicit skip when a
baseline is absent. See [performance logs](../perflogs/README.md) for comparison
rules and baseline provenance.

Use the [Lua setup guide](../docs/lua.md) to run scripts against the installed
rock. On Linux the supported runner selects the pinned target runtime; LuaRocks
search paths alone do not select a compatible loader.

Release packaging ships the Lua binding sources in these artifacts:

- `lonejson-lua-<version>.tar.gz`, the standalone Lua source archive
- `lonejson-<version>-1.rockspec`, the release rockspec
- `lonejson-<version>-1.src.rock`, the source rock containing the Lua archive

That source package prefers a curl-enabled native build when the build
environment provides curl headers and libraries. If the curl probe fails, it
falls back automatically to a curl-free build of the same Lua module names.
Set `LONEJSON_LUA_FORCE_CURL=1` to require curl support, or
`LONEJSON_LUA_DISABLE_CURL=1` to force the curl-free build.

The example program in [`examples/lua_binding.lua`](../examples/lua_binding.lua)
is a good companion to this document.

When you also use the C API around the Lua binding, follow the same
initialization rule as the rest of lonejson: instantiate one runtime with
`lonejson_new()`/`lonejson_default_config()` and use the public `*_init`
helpers for public structs instead of manual `memset` or `{0}`.

## C embedder interop

The installed C SDK remains Lua-agnostic. Its public `include/lonejson.h`
defines generic borrowed views such as `lonejson_schema_view` and
`lonejson_record_view`, but it does not include `<lua.h>` or declare Lua adapter
functions.

The Lua-specific adapter header lives with the Lua binding sources as
`src/lua/lonejson_lua.h` and is staged into the Lua source rock. That header is
for C embedders that compile against the Lua module source surface and need to
borrow Lua-created schemas or records. The adapter functions validate Lua
userdata and fill the generic views from `lonejson.h`.

Borrowed views do not transfer ownership. Lua-created objects remain owned by
their `lua_State`; if C code retains one beyond the current stack frame, it must
hold a Lua registry reference to the userdata and must not free, resize, or
retain raw record pointers beyond the documented userdata lifetime.

## Auth facade

When the Lua module is built with JWT/OIDC support, auth helpers are a facade
over the C implementation. Lua does not reimplement crypto, HTTP transfer, or
credential storage. A runtime owns the installed auth and HTTP providers:

```lua
local lonejson = require("lonejson")
local lj = lonejson.new()

lj:set_openssl_auth_provider()
lj:set_http_provider(function(request)
  -- Call curl, a framework HTTP client, or test transport here.
  return {
    status_code = 200,
    content_type = "application/json",
    body = "{}",
  }
end, "my-product/1.0")
```

Runtime methods use the runtime providers. For example,
`lj:jwt_validate_compact_signature(...)` uses the installed auth provider, and
`lj:oauth2_client_credentials_request(...)` uses the installed HTTP provider.
`lj:oidc_pkce_challenge(verifier)` requires provider SHA-256, while
`lj:oidc_pkce_generate()` requires provider random bytes and SHA-256. With the
built-in OpenSSL adapter installed, those are provided by OpenSSL; embeddings
that compile without OpenSSL can still supply equivalent C provider callbacks
before calling through Lua.

M2M/API-key helpers return store-ready JSON strings. Store persistence,
locking, rotation, revocation, and endpoint authorization remain application
logic:

```lua
local credential = lj:m2m_credential_generate({
  claim = { scope = { "read" }, tenant = "acme" },
  auth_modes = "bearer",
})

local store_json = '{"credentials":[' .. credential.record_json .. ']}'
local auth = lj:m2m_verify_authorization({
  store_json = store_json,
  authorization_header = "Bearer " .. credential.api_key,
  allowed_auth_modes = "bearer",
})

if auth.authorized then
  -- auth.client_id and auth.claim are authenticated facts.
end
```

The [authentication guide](../docs/authentication.md) and
[API reference](../docs/auth-implementation.md) describe the complete C
auth surface, policy fields, e2e coverage, and deliberate non-goals.

## The schema DSL

The binding starts with a schema description. A schema is a name plus a list of
field declarations. Each field declaration gives the JSON key, the expected
type, and any relevant options such as `required`, `fixed_capacity`,
`overflow`, or spool settings.

```lua
local lonejson = require("lonejson")
local lj = lonejson.new()

local Address = lj.object {
  fields = {
    lj.field("city", lj.string { required = true }),
    lj.field("zip", lj.i64()),
  },
}

local User = lj.schema("User", {
  lj.field("name", lj.string { required = true }),
  lj.field("age", lj.i64()),
  lj.field("active", lj.bool()),
  lj.field("address", lj.object {
    fields = Address.fields,
  }),
  lj.field("tags", lj.string_array { overflow = "fail" }),
})
```

The field constructors mirror the main families in the C library:

- `lj.string`
- `lj.i64`
- `lj.u64`
- `lj.f64`
- `lj.bool` / `lj.boolean`
- `lj.object`
- `lj.json_value`
- `lj.string_array`
- `lj.i64_array`
- `lj.u64_array`
- `lj.f64_array`
- `lj.boolean_array`
- `lj.object_array`
- `lj.spooled_text`
- `lj.spooled_bytes`

Optional primitive scalar fields can opt into JSON `null` with
`nullable = true`. This is supported for `lj.i64`, `lj.u64`, `lj.f64`, and
`lj.bool`. On decode, both a missing field and JSON `null` become Lua `nil`.
On encode, Lua `nil` or `lj.json_null` omits the field. `nullable = true`
cannot be combined with `required = true`, and it is intentionally not accepted
for strings, arrays, objects, spools, or `json_value` fields because those
families already have their own null/pass-through behavior.

```lua
local Counters = lj.schema("Counters", {
  lj.field("name", lj.string { required = true }),
  lj.field("attempts", lj.u64 { nullable = true }),
  lj.field("enabled", lj.bool { nullable = true }),
})

local row = Counters:decode('{"name":"job","attempts":null}')
assert(row.attempts == nil)

local json = Counters:encode({
  name = "job",
  attempts = lj.json_null,
  enabled = true,
})
-- {"name":"job","enabled":true}
```

`lj.json_value` maps one field to arbitrary nested JSON. In Lua it decodes to
native Lua values: objects become tables tagged as objects, arrays become
tables tagged as arrays, strings become Lua strings, numbers become Lua
numbers, booleans become booleans, and nested JSON `null` values become the
singleton `lj.json_null` so they survive round-trips inside arrays and objects.
When the entire `json_value` field is JSON `null`, the field itself is exposed
as Lua `nil`.

In table-returning decode paths, `json_value` is built directly from lonejson's
streamed parse visitor callbacks into normal Lua values; it does not first
materialize the whole nested value as retained raw JSON bytes on the C side.
Reusable record mode still keeps an explicit retained C representation so the
record can be accessed repeatedly after decode.

Use `lj.json_object(tbl)` and `lj.json_array(tbl)` when you want to preserve
object-vs-array intent while assigning arbitrary JSON values from Lua tables,
especially for empty tables.

```lua
local Query = lj.schema("Query", {
  lj.field("namespace", lj.string { required = true }),
  lj.field("selector", lj.json_value { required = true }),
  lj.field("fields", lj.json_value()),
})

local q = Query:decode([[{
  "namespace": "ops",
  "selector": {"op":"and","clauses":[{"field":"status","value":"open"}]},
  "fields": ["id", "title", {"metrics":["latency", true, 3.14]}]
}]])

assert(q.selector.op == "and")
assert(q.fields[1] == "id")
assert(q.fields[3].metrics[2] == lj.json_null)
```

The shape is intentionally explicit. A schema is not an afterthought; it is the
contract that makes the rest of the API efficient and predictable.

## Runtime config

Decode, stream, and encode policy comes from the instantiated runtime rather
than per-call option tables. Schemas, reusable records, object streams,
selected-array streams, and encoders created from one `lj` all inherit that
runtime's policy. The main runtime fields are:

- `clear_destination_by_default`
- `reject_duplicate_keys_by_default`
- `max_depth`
- `max_dynamic_string_bytes`
- `max_alloc_bytes`
- `write_pretty`
- `write_max_output_bytes`

Reused non-empty fixed-capacity string fields (`lj.string { fixed_capacity = N
}` with `clear_destination_by_default = false`) stage replacement bytes before
commit so a parse failure does not clobber the old value. By default the Lua
binding uses temporary lonejson-owned staging for that path, and that staging
competes under `max_alloc_bytes`.

When you want that reuse path to stay allocation-free, create a reusable
scratch object and pass it into `lonejson.new({...})`:

```lua
local lonejson = require("lonejson")
local scratch = lonejson.fixed_string_scratch(8192)
local lj = lonejson.new({
  clear_destination_by_default = false,
  max_alloc_bytes = 1024,
  fixed_string_scratch = scratch,
})
local MySchema = lj.schema("MySchema", { ... })
```

Streams and array streams retain the runtime-owned scratch object for the
parser lifetime, so the same runtime can safely reuse it across repeated
`:next(...)` calls.

## Table mode

Table mode is the most direct way to use the binding. Decode returns a Lua
table; encode accepts a Lua table.

```lua
local obj = User:decode([[{
  "name": "Ada",
  "age": 37,
  "active": true
}]])

print(obj.name)

local json = User:encode({
  name = "Ada",
  age = 37,
  active = true,
})
```

Convenience entry points exist for the common sources:

- `schema:decode(json_string)`
- `schema:decode_path(path)`
- `schema:decode_fd(file_handle)`
- `schema:write_path(value, path)`
- `schema:write_fd(value, file_handle)`

Pretty printing is optional and matches the C library: two-space indentation
with stable object and array formatting.

## Reusable records

Reusable records are the most important Lua-specific feature for
performance-sensitive code. A record is allocated once by the schema and then
reused across many decode operations.

```lua
local rec = User:new_record()

User:decode_into(rec, [[{"name":"Bob","age":41}]])
print(rec.name, rec.age)

rec:clear()
User:assign(rec, {
  name = "Carol",
  age = 29,
})
```

Records are useful because they preserve the low-allocation structure of the C
library. If the data shape is stable and you are processing many objects of the
same kind, `decode_into` is usually the right entry point.

## Object-framed streams

The stream API is object-framed, not delimiter-framed. Newlines have no special
meaning beyond ordinary whitespace. A stream can therefore read:

- `{"a":1}{"a":2}`
- one-object-per-line JSONL
- pretty-printed objects separated by blank lines

Streams yield mapped top-level objects only. The Lua facade does not expose an
arbitrary-value stream iterator; use `lj.decode_json` / `lj.decode_value` when
one complete schema-free JSON value is needed.

The Lua API mirrors the C design:

```lua
local stream = User:stream_path("/tmp/users.jsonl")
local rec = User:new_record()

while true do
  local obj, err, status = stream:next(rec)
  if status == "object" then
    print(rec.name)
  elseif status == "eof" then
    break
  else
    error(err and err.message or "stream failed")
  end
end

stream:close()
```

When a record is supplied to `stream:next(rec)`, the same record is reused for
each object. That is the intended fast path for continuous streams.

## Selected array streams

For a JSON document that contains one large array, use an array stream cursor
instead of decoding the whole document. The selected path is direct only:
`"items"` selects a root object member named `items`, while `""` selects a root
array. Dotted fan-out paths such as `"boards.items"` are intentionally rejected
in v1.

```lua
local stream = User:array_stream_path("items", "/tmp/users.json")
local rec = User:new_record()

while true do
  local obj, err, status = stream:next(rec)
  if status == "item" then
    print(rec.name)
  elseif status == "eof" then
    break
  else
    error(err and err.message or "array stream failed")
  end
end

stream:close()
```

`array_stream_file`, `array_stream_fd`, and `array_stream_string` mirror the
other source helpers. `stream:next_value()` returns each selected array item as
a native Lua JSON value when you need raw pass-through shape instead of the
schema-mapped record.

## Top-level JSON values

Use `lj.encode_json(value)` / `lj.encode_value(value)` and
`lj.decode_json(json)` / `lj.decode_value(json)` for schema-free top-level JSON
values. Each decode consumes one complete value rather than iterating a
multi-value input. `lj.json_null` represents JSON null when nil would be
ambiguous.

The string-returning encoders grow their output buffer as needed, including
for long strings and escaped content. Runtime-bound encoders enforce
`write_max_output_bytes`; allocation and output-limit failures raise Lua
errors, and the runtime remains reusable after a failed encode.

For callback-driven output, `lj.encode_json_to_sink(value, sink)` and
`lj.encode_value_to_sink(value, sink)` stream encoded chunks to `sink(chunk)`
without first building the complete JSON string.

```lua
local parts = {}

lj.encode_value_to_sink({
  id = "tool-call",
  arguments = lj.json_object({
    city = "Uppsala",
    days = 3,
  }),
}, function(chunk)
  parts[#parts + 1] = chunk
end)

local json = table.concat(parts)
```

## Selected array rewrites

Use `lj.array_rewrite_string(selector, json, options)` or
`lj.array_rewrite_path(selector, input_path, output_path, options)` to transform
one selected array while streaming the surrounding document through the
rewriter. The selector accepts `""` for a root array, direct object paths such
as `"items"`, and explicit fan-out segments such as `"boards[].items"`.

```lua
local out = lj.array_rewrite_string("items", [[{"items":[{"id":1},{"id":2}]}]], {
  item = function(item)
    if item.id == 1 then
      return "drop"
    end
    return { action = "replace", value = { id = item.id, kept = true } }
  end,
  append = function(ctx, emit)
    assert(emit({ id = 3, kept = true }))
  end,
})
```

The item callback receives a native Lua JSON value and a context table. Return
`"keep"`, `"drop"`, or an action table using `replace`, `insert_before`,
`insert_after`, or `replace_and_insert_after`; replacement values are serialized
as JSON in deterministic key order. Parent contexts can be mapped by passing
`parents = { { segment = "boards", schema = Board } }`; callbacks then read
`ctx.parents[1].value`.

## Path compilation and accessors

The binding also exposes compiled accessors for hot paths where repeated string
lookups would be wasteful.

```lua
local city = User:compile_get("address.city")
local tag_count = User:compile_count("tags")

local rec = User:new_record()
User:decode_into(rec, [[{
  "name":"Dora",
  "address":{"city":"Uppsala"},
  "tags":["ops","oncall"]
}]])

print(city(rec))
print(tag_count(rec))
```

There is also a lower-level `compile_path(...)` form, but in normal code the
compiled getter/count closures are the more direct interface.

## Spool-backed fields

Large values are represented in Lua through spool-backed objects rather than as
unbounded strings.

```lua
local Blob = lj.schema("Blob", {
  lj.field("name", lj.string { required = true }),
  lj.field("body", lj.spooled_text {
    memory_limit = 96,
    temp_dir = "/tmp",
  }),
  lj.field("payload", lj.spooled_bytes {
    memory_limit = 96,
    temp_dir = "/tmp",
  }),
})
```

Assigning or decoding into those fields produces spool handles with methods such
as:

- `spool:size()`
- `spool:spilled()`
- `spool:path()`
- `spool:rewind()`
- `spool:read(n)`
- `spool:read_all()`

The helper `lj.chunks(spool, chunk_size)` provides a small iterator wrapper for
chunked reads:

```lua
for chunk in lj.chunks(rec.payload, 16384) do
  sink(chunk)
end
```

This is the right model for large text or byte payloads that should remain
bounded in memory while still fitting into a JSON pipeline.

## Error handling

The binding uses normal Lua return conventions rather than making every
operation throw by default. Successful operations typically return the decoded
value or `true`. Failures generally return `nil` or `false` plus an error table
that carries the message from the underlying C layer.

For stream operations, `stream:next(...)` returns three values:

- object or `nil`
- error table or `nil`
- status string

The status is one of:

- `"object"`
- `"eof"`
- `"would_block"`
- `"error"`

Array streams use the same convention, with `"item"` instead of `"object"` for
successful items. Code should branch on the status instead of guessing from the
object value alone.

## Examples and related files

Useful files in this repository:

- [`examples/lua_binding.lua`](../examples/lua_binding.lua)
- [`tests/test_lua.lua`](../tests/test_lua.lua)
- [Performance logs](../perflogs/README.md)
- [`README.md`](../README.md)

Those files together cover the binding from three angles: example usage,
behavioral tests, and performance measurement.
