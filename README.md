# lonejson

`lonejson` maps JSON directly into C structs and serializes those structs back
into JSON. Define the fields your application uses, bind them to a schema, and
parse or write through that schema with explicit ownership and allocation limits.

The core library supports C89 and has no mandatory third-party runtime or link
dependencies. Use a conventional linked library or the generated standalone
single header. A Lua binding provides schemas, reusable records and the same
streaming primitives.

[Releases](https://github.com/c89systems/lonejson/releases) ·
[Documentation](docs/index.md) ·
[Examples](https://github.com/c89systems/lonejson/tree/trunk/examples)

## What it handles

- Typed field mappings with fixed-capacity or dynamically allocated storage.
- Parsing from buffers, readers, files and paths; serialization to buffers,
  sinks, files and JSON Lines.
- Incremental object streams, selected-array streams, JSON rewrites, SSE and
  multipart parsing.
- Large inbound fields that can spill to disk, and outbound fields streamed
  directly from existing files or descriptors.
- Arbitrary JSON delivered through visitors or sinks, with explicit capture
  when retained values are needed.
- Optional curl and provider-backed JWT/OIDC/OAuth2 integrations. Applications
  keep control of transport, cryptography, credentials and persistence.

## Integrate

Download a target SDK from [Releases](https://github.com/c89systems/lonejson/releases),
include `lonejson.h`, and link `liblonejson`. With the SDK on CMake's package path:

```cmake
find_package(lonejson CONFIG REQUIRED)
target_link_libraries(app PRIVATE lonejson::lonejson)
```

For single-header integration, use the generated release header and define
`LONEJSON_IMPLEMENTATION` in exactly one translation unit:

```c
#define LONEJSON_IMPLEMENTATION
#include "lonejson.h"
```

The source-tree `include/lonejson.h` is the linked-library public header; the
standalone implementation header is a separate release artifact. See
[Integration](docs/integration.md) for strict C89/POSIX build flags, optional
components and dependency rules.

## Quick start

This complete example parses one object into a mapped struct and serializes it
back to JSON. Compile it against the linked SDK, or use the standalone header
as described above.

```c
#include <stdio.h>
#include "lonejson.h"

typedef struct user_doc {
  char *name;
  lonejson_int64 age;
} user_doc;

static const lonejson_field user_doc_fields[] = {
  LONEJSON_FIELD_STRING_ALLOC_REQ(user_doc, name, "name"),
  LONEJSON_FIELD_I64(user_doc, age, "age")
};
LONEJSON_MAP_DEFINE(user_doc_map, user_doc, user_doc_fields);

int main(void) {
  lonejson *lj;
  lonejson_error error;
  lonejson_status status;
  user_doc doc;
  char json[128];

  lj = lonejson_new(NULL, &error);
  if (lj == NULL) {
    fprintf(stderr, "%s\n", error.message);
    return 1;
  }

  lonejson_init(lj, &user_doc_map, &doc);
  status = lonejson_parse_cstr(lj, &user_doc_map, &doc,
                              "{\"name\":\"Ada\",\"age\":37}", &error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_serialize_buffer(lj, &user_doc_map, &doc, json,
                                      sizeof(json), NULL, &error);
  }
  if (status == LONEJSON_STATUS_OK) {
    puts(json);
  } else {
    fprintf(stderr, "%s\n", error.message);
  }

  lonejson_cleanup(&user_doc_map, &doc);
  lonejson_free(lj);
  return status == LONEJSON_STATUS_OK ? 0 : 1;
}
```

Output:

```json
{"name":"Ada","age":37}
```

Free functions and receiver methods expose equivalent operations; for example,
`lj->parse_cstr(lj, ...)` corresponds to `lonejson_parse_cstr(lj, ...)`.
Short aliases (`lj_*` and `LJ_*`) are enabled by default.

Initialize mapped values before use and clean up their owned storage on both
success and failure. For reusable configured storage, use the runtime's reuse
policy or the selected-array stream's `set_record_reuse` method, then reset the
record between items. See [ownership and limits](docs/api.md) and
[streaming record reuse](docs/streaming.md#read-a-selected-array-stream).

## Build from source

Use CMake 3.25 or newer, Ninja and Bash 4.4 or newer. On Linux, Make prepares the
pinned Bootlin toolchain; native Apple Silicon macOS uses Apple tools from
Xcode or Command Line Tools.

```sh
make build
make test
```

The native build prepares the generated headers and compilation database used
by clangd. For routine development, `make finalize-slice` runs focused core,
ABI and Lua checks. `make abi-check` also reports known downstream upgrade order
when the ABI changes. See [building and verification](docs/development.md) for
cross builds, sanitizers, fuzzing, benchmarks and local services.

## Guides

| Guide | Topics |
| --- | --- |
| [Integration](docs/integration.md) | Linked and standalone headers; optional curl, OpenSSL and auth components. |
| [C API and ownership](docs/api.md) | API families, runtime/handle lifetimes, limits, reuse and short aliases. |
| [Streaming and rewriting](docs/streaming.md) | Object/array cursors, push input, record reuse, rewrites, SSE and multipart. |
| [Serialization](docs/serialization.md) | Buffers, JSON Lines, generators, formatting and custom allocators. |
| [Large values](docs/large-values.md) | Spools, file-backed sources, arbitrary JSON and visitors. |
| [Authentication integration](docs/authentication.md) | Provider setup, token flows, M2M credentials and application handlers. |
| [Lua binding](docs/lua.md) | Installation, examples and reusable records. |
| [Building and verification](docs/development.md) | Toolchains, local checks, services, benchmarks and downstream ABI coordination. |
| [Release artifacts](docs/releasing.md) | SDKs, source archives, Lua products, versioning and release requirements. |

The [documentation index](docs/index.md) also links the detailed auth reference,
receiver API audit and integration contracts. Runnable examples are catalogued
in the [examples guide](https://github.com/c89systems/lonejson/blob/trunk/examples/README.md).

## License

[MIT](LICENSE).
