# Documentation

Start with the [project overview and quick start](../README.md). Commands in the
guides run from the source checkout unless a guide says otherwise.

| Guide | Contents |
| --- | --- |
| [Integration](integration.md) | Link the SDK, use the generated standalone header, and enable optional components. |
| [C API and ownership](api.md) | API families, runtime and handle ownership, limits, reuse, aliases and base64. |
| [Streaming and rewriting](streaming.md) | Object framing, selected arrays, record reuse, push input, rewrites, SSE and multipart. |
| [Serialization](serialization.md) | Buffers, JSON Lines, pull generators, pretty printing and allocator ownership. |
| [Large values](large-values.md) | Inbound spools, outbound sources, embedded JSON and visitors. |
| [Authentication integration](authentication.md) | Application HTTP providers, token state, machine credentials and web handlers. |
| [Lua binding](lua.md) | Build, load and test the binding; reusable records and examples. |
| [Building and verification](development.md) | Build policy, local services, checks, benchmarks and downstream ABI upgrades. |
| [Release artifacts](releasing.md) | Artifact inventory, release identity, packaging and maintainer workflow. |

## Detailed references

- [Public C header](https://github.com/c89systems/lonejson/blob/trunk/include/lonejson.h)
- [Full Lua manual](https://github.com/c89systems/lonejson/blob/trunk/lua/README.md)
- [Runnable examples](https://github.com/c89systems/lonejson/blob/trunk/examples/README.md)
- [Receiver API coverage](receiver-api.md)
- [Third-party integration contract](third-party-integrations.md)
- [Authentication API reference](auth-implementation.md)
- [Curl upload rewind](curl-upload-rewind.md)
