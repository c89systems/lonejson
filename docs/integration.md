# Integration

Choose a linked SDK or the generated standalone header. Build commands in these guides run from a source checkout unless stated otherwise.

The core `lonejson` library has no mandatory third-party runtime or link
dependencies. Third-party libraries are integrations, not baseline
dependencies: the plain `lonejson` package metadata must not force consumers to
link curl, OpenSSL, crypto libraries, TLS libraries, framework code, or any
other transport/auth stack. Optional integration APIs may expose third-party
types or require third-party headers only when the matching compile-time feature
is enabled by the consumer.

For source CMake builds, the library options are `LONEJSON_BUILD_WITH_CURL`,
`LONEJSON_BUILD_WITH_OPENSSL`, `LONEJSON_BUILD_WITH_JWT` and
`LONEJSON_BUILD_WITH_OIDC`. OIDC requires JWT; curl and OpenSSL remain optional
provider implementations. A JWT/OIDC build without them is valid, but trust
operations requiring crypto fail until an auth provider is installed. Binary
SDK packaging requires all four surfaces enabled while preserving the plain
core dependency boundary.

The integration model is provider/callback first. Downstream code satisfies the
HTTP and crypto boundaries with whatever implementation it already owns.
`lonejson` may provide optimized adapters for common choices such as libcurl
and OpenSSL/libcrypto, but those adapters are convenience paths, not core
dependencies. A consumer can take the easy path by enabling the adapter and
linking that third-party library, or can provide equivalent callbacks backed by
another stack.

The repository supports two integration styles.

The normal development and binary-distribution path is a conventional linked
library. Include the public header and link `liblonejson.a` or
`liblonejson.so`:

```c
#include "lonejson.h"
```

The standalone single-header release artifact remains available for projects
that prefer vendoring one file. In that mode, define
`LONEJSON_IMPLEMENTATION` in exactly one translation unit before including the
generated standalone header from the release archive:

```c
#define LONEJSON_IMPLEMENTATION
#include "lonejson.h"
```

For strict C89 implementation builds on POSIX hosts, also compile with
`-D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64`. These select the POSIX
declarations used by file, spool and formatting operations. If defining them
in source, place them before every system header in the implementation
translation unit. Linked-library consumers do not need to compile the
implementation again.

The source-tree `include/lonejson.h` is the linked-library public header. The
generated single-header artifact is a separate release output, not the same
file.

curl integration remains optional at compile time. When you want the curl
adapter declarations with packaged CMake metadata, request and link the
explicit adapter target:

```cmake
find_package(lonejson CONFIG REQUIRED COMPONENTS curl)
target_link_libraries(app PRIVATE lonejson::lonejson lonejson::curl)
```

That target defines `LONEJSON_WITH_CURL` for the consumer and links
`CURL::libcurl`. With pkg-config, use `lonejson-curl`. Manual build systems may
define `LONEJSON_WITH_CURL` directly and must also provide curl headers and
link libcurl. `lonejson_curl_upload_init()` sits on top of the public
pull-style generator API and feeds libcurl through `CURLOPT_READFUNCTION`
without materializing the whole JSON payload first. That upload path currently
reports `-1` for the total size because lonejson does not prebuffer or
pre-count the payload. For redirects or authentication retries, install
`lonejson_curl_seek_callback` as `CURLOPT_SEEKFUNCTION` and pass the same
upload adapter through both `CURLOPT_SEEKDATA` and `CURLOPT_READDATA`.
Replay is available only when every emitted mapped value is rewindable; use
`lonejson_curl_upload_is_rewindable()` when an application needs to know that
before it starts the transfer. One-shot readers remain streaming sources and
cause curl's normal rewind failure when a replay is required.

HTTP behavior used by OAuth2/OIDC helpers is callback-backed through
`lonejson_http_provider`. The callback may use libcurl, a platform HTTP client,
test fixtures, or another transport. The curl adapter exists to make the common
libcurl path efficient and easy; it does not make curl a dependency of the
plain `lonejson` SDK.

JWT/OIDC/OAuth2 support follows the same integration rule. Build with
`LONEJSON_WITH_JWT` for JWT, JWK, and JWKS parsing plus explicit claim
validation. Build with `LONEJSON_WITH_OIDC` for discovery, JWKS cache, OAuth2
token exchange helpers, PKCE authorization URLs, callback parsing, and
bearer-token validation. Build consumers with `LONEJSON_WITH_OPENSSL` only
when they deliberately opt into the OpenSSL auth-provider initializer for
signature validation, PKCE hashing, and random verifier generation. The plain
`lonejson` SDK still does not link libcrypto: JWT/OIDC builds install a runtime
`lonejson_auth_provider` when signature validation, PKCE SHA-256, or random
PKCE verifier generation is needed.

Packaged CMake exposes explicit feature targets for that opt-in:

```cmake
find_package(lonejson CONFIG REQUIRED COMPONENTS oidc openssl)
target_link_libraries(app PRIVATE
  lonejson::lonejson
  lonejson::oidc
  lonejson::openssl)
```

`lonejson::oidc` defines `LONEJSON_WITH_JWT` and `LONEJSON_WITH_OIDC`.
`lonejson::openssl` defines `LONEJSON_WITH_OPENSSL` and links
`OpenSSL::Crypto`. The pkg-config equivalents are `lonejson-jwt`,
`lonejson-oidc`, and `lonejson-openssl`.

The auth design is provider-backed. Parsing a JWT is never a trust decision:
decode with `lonejson_jwt_decode_compact()`, select a JWK from a trusted JWKS
cache, validate the signature through the runtime auth provider, and validate
claims with an explicit `lonejson_jwt_claim_policy`. The OpenSSL provider
is the optimized easy path for consumers that choose OpenSSL/libcrypto; it
supports `RS256`, `PS256`, `ES256`, and `EdDSA` for Ed25519 OKP keys.
When a selected JWK includes `x5c`, the OpenSSL provider validates the
certificate chain, verifies `x5t`/`x5t#S256` thumbprints declared by the JWK or
JWT header, and requires the leaf certificate public key to match the JWK. Pass
an OpenSSL `X509_STORE *` through `lonejson_openssl_auth_provider_config` for
private CA/test roots; otherwise OpenSSL default verify paths are used.
`alg: none` is rejected; `HS256`, Ed448, and JWE are not implemented.
Compatibility helpers such as `lonejson_jwt_validate_signature()` and
`lonejson_oidc_pkce_generate()` do not make cryptography a core SDK
dependency. Use the runtime-backed helpers with an installed auth provider, or
initialize that provider with `lonejson_auth_provider_init_openssl()` from a
consumer that has opted into OpenSSL.

See the [integration contract](third-party-integrations.md) for the release
invariants that keep integrations from becoming forced package dependencies.


See [authentication patterns](authentication.md) for HTTP providers, token flows and application handlers, and [curl upload replay](curl-upload-rewind.md) for redirect/retry behavior.
