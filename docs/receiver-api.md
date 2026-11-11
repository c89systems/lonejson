# Receiver API coverage

Instance operations have both a public free function and an equivalent receiver
method. The receiver is the first method argument. Constructors that take an
existing runtime also belong on that runtime. Initializers that prepare
caller-owned storage can be called as free functions before the receiver exists.

Runtime record `init`, `reset`, and `cleanup` methods, their free functions, and
short aliases return `lonejson_status` and accept an optional error output.
Traversal-workspace allocation failure preserves the record for retry. See
[checked record lifecycle](api.md#checked-record-lifecycle) for ownership and
error handling.

The public API exposes these 20 receiver surfaces:

| Surface | Operations checked |
| --- | --- |
| `lonejson` | Parsing, validation, visitors, resumable visitor creation, record mutation, stream creation, rewriting, serialization, generators, SSE JSON, and enabled auth/HTTP/curl helpers. |
| `lonejson_stream` | Next, last error, close. |
| `lonejson_array_stream` | Record reuse, mapped/raw next, mapped/string push and finish, last error, close. |
| `lonejson_value_visitor_stream` | Push, finish, close. Open it through the runtime or the free function. |
| `lonejson_sse` | Push, finish, mapped/arbitrary JSON push and finish, close. |
| `lonejson_multipart` | Push, finish, close. |
| `lonejson_spooled` | Reset, cleanup, size, spill state, append, rewind, read, write. |
| `lonejson_source` | Reset, cleanup, source selection, write, replay capability. |
| `lonejson_json_value` via `methods` | Reset, cleanup, source and parse delivery selection, write, replay capability. |
| `lonejson_string_array_stream` | Handler configuration. |
| `lonejson_mapped_array_stream` | Handler configuration. |
| `lonejson_generator` | Read, cleanup. |
| `lonejson_writer` | Structural/scalar events, streamed values, selected-array append, mapped output, finish, cleanup. |
| `lonejson_writer_value_stream` | Open, push, close, cleanup. |
| `lonejson_curl_parse` | Write callback, finish, cleanup. |
| `lonejson_curl_array_parse` | Write callback, finish, cleanup. |
| `lonejson_curl_string_array_parse` | Write callback, finish, cleanup. |
| `lonejson_curl_string_items_parse` | Write callback, finish, cleanup. |
| `lonejson_curl_upload` | Read/seek callbacks, size, replay capability, rewind, cleanup. |
| `lonejson_oidc_jwks_cache_parse` | Write callback, finish, cleanup; construction also belongs to the runtime. |

`error_fn`, `size_fn`, `spilled_fn`, `bool_fn`, and `null_fn` avoid data-member
or language-keyword collisions. Curl callback methods adapt receiver-first calls
to the signatures required by libcurl; install the free callbacks in libcurl.
JSON-value methods use the existing shared method table.

Allocator/provider/visitor/handler callback tables implement callbacks supplied
by the application or a backend; they are not instance-operation shells. Plain
records, maps, options, arrays, and buffers remain plain data rather than receiver shells. Stateless helpers and initialization of such data remain free functions.
Lua C interop takes a foreign `lua_State *`; its operations belong to Lua's
existing API and userdata methods rather than a new LoneJSON C receiver.

SSE and multipart are library-owned shells with private parser state. SSE JSON
methods take the receiver first and runtime second; runtime routes and free
functions remain available. Constructors are called before receiver storage
exists, and plain data helpers remain on their runtime or free-function surface.

Receiver layouts form part of the shared-library ABI, published as
`LONEJSON_ABI_VERSION`. The runtime's `value_visitor_stream_open` method
advances the ABI from v0.48.0's 28 to 29. Rebuild consumers with matching
headers and library. The borrowed schema/record view interface is versioned
independently.
