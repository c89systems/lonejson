# Third-party integration contract

Core `lonejson` has no mandatory third-party runtime or link dependencies.
Applications choose transport and crypto implementations through providers and
callbacks. Optional adapters make libcurl and OpenSSL convenient without making
them dependencies of the plain core SDK. See [integration](integration.md) for
consumer build and link commands.

## Core and adapter boundaries

| Surface | Contract |
| --- | --- |
| Core static/shared libraries | No unresolved third-party adapter symbols or third-party dynamic loader dependencies. A plain consumer links with core metadata alone. |
| Core CMake targets and `lonejson.pc` | No curl, OpenSSL, crypto, TLS or framework link requirements, including private/transitive requirements. |
| Explicit adapter metadata | `lonejson::curl`, `lonejson::openssl`, `lonejson-curl.pc` and `lonejson-openssl.pc` may expose the dependencies the consumer selected. |
| SDK payload | No bundled third-party headers/libraries unless a separate bundled-SDK contract explicitly requires them. |
| Core public header | Self-contained without third-party headers; optional integration types and headers are feature-guarded. |

Build-input provenance does not create a downstream dependency. Package paths
must be relocatable and must not expose local build or dependency-cache paths.
OpenSSL must not be advertised as a core SDK build input.

## Providers and optional features

- `LONEJSON_WITH_CURL` enables curl-compatible callback helpers. The application
  owns its libcurl handles and links curl if it chooses that transport.
- `LONEJSON_WITH_JWT` enables JWT, JWK, JWKS and claim-policy APIs.
- `LONEJSON_WITH_OIDC` enables OAuth2/OIDC orchestration and requires JWT.
- `LONEJSON_WITH_OPENSSL` enables the optional OpenSSL auth-provider adapter.
  JWT/OIDC without OpenSSL remains valid.

OAuth2/OIDC helpers call `lonejson_http_provider`; its implementation may use
libcurl, a framework client, a platform API or a test transport. Crypto helpers
use `lonejson_auth_provider` for `verify_jws`, `sha256` and `random_bytes`.
An application-supplied implementation is supported on the same terms as the
optimized adapters.

Curl helpers may expose curl types, constants and callback signatures. Core
libraries must not call `curl_easy_*`, `curl_multi_*` or other libcurl link
symbols. Replayable mapped uploads use read/seek callbacks with the same upload
adapter as both userdata pointers; see the [rewind contract](curl-upload-rewind.md).

The OpenSSL adapter may accept opaque OpenSSL-owned objects such as an
`X509_STORE *`. Actual `EVP_*`, `RAND_*`, `X509_*`, `BN_*`, `OSSL_*` and
`CRYPTO_*` calls belong at the consumer's opt-in boundary, not inside core
`liblonejson.a` or the shared library. Ownership and lifetime must be explicit.

Binary SDK releases enable the full curl/OpenSSL/JWT/OIDC surface while keeping
core linking independent. Disabling a release feature to hide dependency leakage,
or adding a second mandatory adapter library, violates this contract.

## Verification and new integrations

The archive verifier checks extracted release bytes, including plain static and
shared consumers, exact exports and forbidden private imports. It rejects:

- Third-party requirements in core pkg-config metadata, including
  `Requires.private` or `Libs.private`.
- Third-party `INTERFACE_LINK_LIBRARIES` on `lonejson::lonejson` or
  `lonejson::lonejson_static`.
- Unresolved third-party symbols in core static archives or third-party loader
  dependencies in core shared libraries.
- Uncontracted bundled dependencies, OpenSSL marked as a core build input, or
  local build/cache paths in packaged metadata.

Regression fixtures inject those defects into release-shaped archives and
require actionable verification failures. These are prerelease/release blockers.

For each new integration, define its feature guard and build option, keep the
core buildable without its headers, place implementation behind a provider,
callback or guarded adapter, and document opt-in linking and ownership. Add
negative package checks proving it cannot become a forced core dependency.
An integration that cannot meet this boundary requires an explicit product
decision before implementation.
