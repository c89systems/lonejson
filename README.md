# lonejson

`lonejson` is a schema-guided JSON library for C. In normal source-tree use it
is built and linked as `liblonejson`, with a public header in
`include/lonejson.h`. The release process also produces a standalone
single-header artifact for projects that prefer to vendor one file directly.
The library maps JSON objects directly into C structs and back again, with a
particular emphasis on object-framed streams, predictable fixed-capacity
decoding, and pipelines that need to move large values through JSON without
holding everything in memory at once.

The project is built for programs that already know the shape of the data they
care about. Instead of constructing a generic DOM and walking it afterward, you
describe the fields you want, bind them to a `lonejson_map`, and parse or
serialize through that schema. That approach gives the library its character:
it is strongly typed, stream-oriented, and deliberate about ownership and
allocation.

Repository: <https://github.com/c89systems/lonejson>\
Examples: <https://github.com/c89systems/lonejson/tree/trunk/examples>

## Use-case

`lonejson` is not a DOM library, and it is not trying to be one. The normal
workflow is to define a C struct, describe its JSON-visible fields with
`LONEJSON_FIELD_*` macros, bind those fields into a `lonejson_map`, and then
use that map to parse, stream, validate, or serialize. If you want stable
decoding into known types, object-framed streaming that does not depend on line
delimiters, bounded-memory behavior, and explicit control over fixed versus
dynamic storage, that model works very well.

The library also covers two cases that often push JSON tooling into awkward
territory. Inbound values can spill to disk through spool-backed fields when a
string or base64 payload is too large to hold comfortably in memory, and
outbound values can be serialized directly from paths, `FILE *`, or file
descriptors when the data already exists outside the process heap.

## Feature set

At the core is the public header `include/lonejson.h`, paired with the compiled
library in the normal build. That API can parse from C strings, raw buffers,
reader callbacks, `FILE *`, filesystem paths, and object-framed streams built
on the same underlying parser. It can also stream one selected array from a
valid JSON document item by item without decoding the whole document first.
Serialization is available to sink callbacks, caller-supplied buffers,
allocated strings, `FILE *`, filesystem paths, and JSON Lines. Pretty printing
is optional and uses a fixed two-space indentation style.

Field mappings can be fixed-capacity or dynamically allocated. Large inbound
values can be represented by spool-backed text or base64-decoded byte fields,
and large outbound values can be serialized from source-backed text or binary
fields. Arbitrary embedded JSON values can be represented by
`lonejson_json_value`, which lets one mapped field contain an opaque JSON
object, array, scalar, or `null` without turning it into a JSON string. The
same surface also exposes `lonejson_value_visitor` and `lonejson_visit_value_*`
for streaming one arbitrary JSON value as structured callbacks without
building a DOM.

The streaming surface also includes push-fed selected-array streams for
write-callback transports, mapped array-stream fields that deliver object array
items during normal mapped parsing, streaming selected-array rewrites, an
incremental Server-Sent Events parser, and an incremental MIME multipart
parser. Short aliases are enabled by default through `lj_*` and `LJ_*`. The
repository also ships a Lua binding with a schema DSL, reusable records,
object-framed streaming, selected-array streaming, selected-array rewrites,
spool-backed fields, and native-Lua `json_value` decoding built on the same
visitor model.

Initialize mapped destinations with `lonejson_init(...)` before the first parse
when they contain stateful handle fields such as `lonejson_spooled`,
`lonejson_json_value`, or mapped array-stream helpers. Reuse initialized values
with `lonejson_reset(...)`.

When you need caller-owned fixed-capacity array backing storage during parse,
initialize the destination first, configure that backing storage, and parse
with `clear_destination = 0` so lonejson preserves the configured arrays
instead of rebuilding the destination from scratch.

## Integration

The core `lonejson` library has no mandatory third-party runtime or link
dependencies. Third-party libraries are integrations, not baseline
dependencies: the plain `lonejson` package metadata must not force consumers to
link curl, OpenSSL, crypto libraries, TLS libraries, framework code, or any
other transport/auth stack. Optional integration APIs may expose third-party
types or require third-party headers only when the matching compile-time feature
is enabled by the consumer.

The integration model is provider/callback first. Downstream code satisfies the
HTTP and crypto boundaries with whatever implementation it already owns.
`lonejson` may provide optimized adapters for common choices such as libcurl
and OpenSSL/libcrypto, but those adapters are convenience paths, not core
dependencies. A consumer can take the easy path by enabling the adapter and
linking that third-party library, or can provide equivalent callbacks backed by
another stack.

The repository now supports two integration styles.

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
link libcurl. `lonejson_curl_upload_init()` now sits on top of the public
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

See `docs/third-party-integrations.md` for the release and prerelease
invariants that keep integrations from becoming forced package dependencies.

Base64 helpers are available independently of JWT. Use
`lonejson_base64_encode()`/`lonejson_base64_decode()` for caller-provided
buffers, or the `_sink` variants when the encoded/decoded data should be
streamed to a callback instead of materialized by the helper. Variants are
explicit: `LONEJSON_BASE64_STANDARD`, `LONEJSON_BASE64_STANDARD_RAW`,
`LONEJSON_BASE64_URL`, and `LONEJSON_BASE64_URL_RAW`. JWT/JWS segments use
`LONEJSON_BASE64_URL_RAW`. Lua exposes the materialized helpers as
`lonejson.base64_encode(data, variant)` and
`lonejson.base64_decode(text, variant)`, where `variant` is `"standard"`,
`"standard_raw"`, `"url"`, or `"url_raw"`; `"jwt"` is accepted as an alias for
`"url_raw"`.

For command-line tools and services that already link curl, keep curl in the
application and install a small HTTP provider callback. The callback receives a
bounded `lonejson_http_request`, performs the GET or POST with the
application's curl policy, and fills `lonejson_http_response`:

```c
lonejson_error error;
lonejson *lj;
lonejson_auth_provider auth;
lonejson_http_provider http;
struct my_http_state http_state;

lonejson_error_init(&error);
lj = lonejson_new(NULL, &error);
lonejson_auth_provider_init_openssl(&auth, NULL, &error);
lonejson_set_auth_provider(lj, &auth, &error);
lonejson_http_provider_init_simple(&http, &http_state, "my-product/1.0",
                                   my_curl_request, &error);
lonejson_set_http_provider(lj, &http, &error);
```

After that, `lonejson_oidc_fetch_discovery()`,
`lonejson_oidc_jwks_cache_refresh()`,
`lonejson_oauth2_client_credentials_request()`,
`lonejson_oauth2_refresh_token_request()`, and
`lonejson_oidc_authorization_code_token_request()` use the installed HTTP
provider. The auth HTTP provider materializes bounded discovery, JWKS, and
token endpoint responses because those helper APIs are materialized by design;
this keeps TLS trust roots, proxies, redirects, retry policy, and advanced curl
options in application code. Use the lower-level curl parse adapters for
general large JSON response streaming.

For dev-only HTTPS test providers with self-signed certificates, put
`CURLOPT_SSL_VERIFYPEER = 0` and `CURLOPT_SSL_VERIFYHOST = 0` in the caller's
HTTP provider callback. Do not put that policy in lonejson configuration; TLS
verification remains transport-owned.

For client-side token state, use `lonejson_oauth2_token_flow`. Initialize it,
copy a successful token endpoint response into it, then call
`lonejson_oauth2_token_flow_ensure()` before sending a request that needs an
access token. The helper returns `ready`, `refreshed`, `needs_interaction`, or
`failed` state via `lonejson_oauth2_token_flow_result`; it refreshes expired
tokens through the installed HTTP provider when a refresh token and token
endpoint policy are available. Refresh and retry can be disabled per call, and
the helper never starts a browser, persists credentials, sleeps, or schedules
background work. Lua exposes the same workflow as table-based
`oauth2_token_flow_update_response`, `oauth2_token_flow_is_expired`, and
runtime `oauth2_token_flow_ensure`.

For server-local machine-to-machine credentials,
`lonejson_m2m_credential_generate()` creates one-time client secrets/API keys
and a store-ready JSON record containing only salts, hashes, and an opaque claim
JSON value. `lonejson_m2m_verify_authorization()` checks
`Authorization: Basic ...` client credentials or `Authorization: Bearer ...`
API keys against caller-owned store JSON and returns the authenticated
`client_id` plus claim. `lonejson_m2m_signup_generate()` and
`lonejson_m2m_signup_complete()` support admin-seeded signup links where the
handler collects an email address, issues credentials once, and then removes
the consumed signup seed from caller-owned storage.

Use Bearer-only mode when the endpoint is just an API-key gate:

```c
lonejson_m2m_credential_request create = {
    .claim_json = "{\"scope\":[\"read\"],\"tenant\":\"acme\"}",
    .auth_modes = LONEJSON_M2M_AUTH_BEARER
};
lonejson_m2m_credential credential;

lonejson_m2m_credential_init(&credential);
lonejson_m2m_credential_generate(lj, &create, &credential, &error);
/* Show credential.api_key once. Store credential.record_json under credentials[]. */
```

In a web handler, verify the raw `Authorization` header before application
logic:

```c
lonejson_m2m_store store = {
    .json = credential_store_json,
    .len = credential_store_len
};
lonejson_m2m_verify_request verify = {
    .store = &store,
    .authorization_header = authorization_header,
    .allowed_auth_modes = LONEJSON_M2M_AUTH_BEARER
};
lonejson_m2m_authentication auth;

lonejson_m2m_authentication_init(&auth);
status = lonejson_m2m_verify_authorization(lj, &verify, &auth, &error);
if (status != LONEJSON_STATUS_OK ||
    auth.failure != LONEJSON_AUTH_FAILURE_NONE) {
    /* framework code returns 401/403 here */
}
/* auth.client_id and auth.claim are authenticated facts. App code decides
 * whether this client may use this endpoint/method/tenant/operation. */
lonejson_m2m_authentication_cleanup(&auth);
```

Credential store mutation is deliberately caller-owned. The intended persistent
shape is a JSON object containing `credentials[]` and, when signup is used,
`signups[]`. To rotate an API key, generate a replacement credential, insert
the new `record_json`, and remove or mark the old record as revoked. To revoke,
remove the record or set its `revoked` field. lonejson does not lock files,
encrypt stores, keep audit history, or decide access policy from claims.

For Kore, Vectis, or another C web framework, keep lonejson at the auth
boundary instead of adding framework-specific dependencies. Refresh discovery
and JWKS through a configured runtime during startup or cache maintenance, then
call `lonejson_oidc_validate_bearer_token()` from the framework request handler
before application logic:

```c
lonejson_oidc_bearer_validation_request request = {
    .authorization_header = authorization_header,
    .jwks_cache = &jwks_cache,
    .jwks_policy = &jwks_policy,
    .claim_policy = &claim_policy
};
lonejson_oidc_bearer_validation validation;

lonejson_oidc_bearer_validation_init(&validation);
status = lonejson_oidc_validate_bearer_token(lj, &request, &validation,
                                             &error);
if (status != LONEJSON_STATUS_OK || validation.failure != LONEJSON_AUTH_FAILURE_NONE) {
    /* framework code returns 401/403 here */
}
lonejson_oidc_bearer_validation_cleanup(&validation);
```

The Lua facade follows the same provider model. A runtime can install an
OpenSSL auth provider and an HTTP callback with a user-agent:

```lua
local lj = lonejson.new()
lj:set_openssl_auth_provider()
lj:set_http_provider(function(request)
  return {
    status_code = 200,
    content_type = "application/json",
    body = "{}"
  }
end, "my-product/1.0")
```

Lua M2M/API-key usage mirrors C while keeping storage explicit:

```lua
local lj = lonejson.new()
lj:set_openssl_auth_provider()

local pkce = lj:oidc_pkce_generate()

local credential = lj:m2m_credential_generate({
  claim = { scope = { "read" }, tenant = "acme" },
  auth_modes = "bearer",
})
-- Show credential.api_key once. Store credential.record_json under credentials[].

local store_json = '{"credentials":[' .. credential.record_json .. ']}'
local auth = lj:m2m_verify_authorization({
  store_json = store_json,
  authorization_header = "Bearer " .. credential.api_key,
  allowed_auth_modes = "bearer",
})
if not auth.authorized then
  -- framework code returns 401/403 here
end
-- auth.client_id and auth.claim are authenticated facts; app code authorizes.
```

The Lua M2M/API-key methods are available when the Lua module is compiled with
the OpenSSL auth provider, because generation and verification require random
bytes and SHA-256 hashing.

Lua signup helpers return store-ready JSON strings and one-time secrets:

```lua
local signup = lj:m2m_signup_generate({
  base_url = "https://app.example/signup",
  claim = { scope = { "write" }, plan = "trial" },
})
-- Send signup.url to the recipient and store signup.record_json under signups[].

local complete = lj:m2m_signup_complete({
  store_json = '{"signups":[' .. signup.record_json .. ']}',
  signup_id = signup.signup_id,
  signup_secret = signup.signup_secret,
  email = "user@example.com",
  credential_auth_modes = "bearer",
})
-- Show complete.credential.api_key once, insert complete.credential.record_json
-- under credentials[], and remove complete.signup_id from signups[].
```

Remaining auth DX gaps are explicit. lonejson does not provide a built-in HTTP
server, browser launcher, localhost callback listener, credential-store
persistence/locking, proxy policy, redirect policy, TLS policy, or
Kore/Vectis-specific handler wrapper. Those are application lifecycle concerns.
Current provider-backed helpers cover discovery, JWKS refresh, token endpoint
exchanges, token-flow refresh/retry, server-side token introspection, token
revocation, UserInfo, fail-closed JOSE `crit`, JWK `key_ops`, OIDC `azp`,
OAuth2 `scope`/`scp`, strict multi-audience policy, and OpenSSL-backed
certificate-chain/thumbprint validation for certificate-backed JWKs
(`x5c`, `x5t`, `x5t#S256`). JWE is intentionally deferred as a separate
encryption feature. This is implemented for lonejson's JSON-centered auth
scope, not as a full identity-provider SDK, complete OAuth2 grant matrix, or
complete JOSE implementation.
Potential future simplifications would be an examples-level curl HTTP provider
callback and an examples-level Kore/Vectis adapter. Those should preserve the
current binary dependency boundary; putting `curl_easy_*` calls directly in
`liblonejson.so` would not.

The local rootless Podman Kube development rig includes a mock OIDC/OAuth2
provider and API fixture. `make test-oidc-e2e` starts the Podman pod, obtains a token
from the mock provider using `curl` rather than lonejson, starts the
lonejson-backed fixture server, and verifies discovery, JWKS refresh, token
introspection, UserInfo, revocation, bearer rejection, and bearer acceptance
against the live endpoints. The same e2e checks fail-closed bearer behavior for
missing credentials, wrong audience, missing scope, wrong/missing `azp`, and
acceptance for strict multi-audience tokens and `scp` array scopes. Podman
commands run through `scripts/devenv.sh` using the checked-in Kube manifest
template, with pod-local connections between services. The e2e also exercises
refresh-token grant exchange against the mock provider and requires a returned
access token.

`make test-m2m-e2e` starts a tiny lonejson-backed API fixture and uses `curl`
as the non-lonejson client to verify Basic client credentials, Bearer API keys,
missing credentials, wrong-secret rejection, signup completion with email
metadata, consumed-signup rejection, and use of signup-generated Basic and
Bearer credentials. Detailed JOSE policy branches such as `crit`, `key_ops`,
and `x5c` certificate-chain/thumbprint failure modes are covered by C/Lua
regression tests and JWT fuzz seeds rather than by a separate live OIDC e2e
scenario for each branch.

Short aliases are enabled by default. Disable them if they collide with another
project:

```c
#define LONEJSON_DISABLE_SHORT_NAMES 1
#include "lonejson.h"
```

## Public API overview

The API is arranged in a few clear families. Mapping is done with
`LONEJSON_FIELD_*` and `LONEJSON_MAP_DEFINE(...)`. One-shot parsing is handled
by `lonejson_parse_cstr`, `lonejson_parse_buffer`, `lonejson_parse_reader`,
`lonejson_parse_filep`, and `lonejson_parse_path`, with `lonejson_validate_*`
variants available when syntax validation is all you need. Streaming uses
`lonejson_stream_open_*`, `lonejson_stream_next`, `lonejson_stream_error`, and
`lonejson_stream_close`. Selected-array cursors use
`lonejson_array_stream_open_*`, `lonejson_array_stream_next`,
`lonejson_array_stream_next_value`, `lonejson_array_stream_error`, and
`lonejson_array_stream_close`. Push-fed selected-array parsing uses
`lonejson_array_stream_open_push`, `lonejson_array_stream_push*`, and
`lonejson_array_stream_finish*`. Streaming selected-array rewrite uses
`lonejson_array_rewrite_*`. Protocol helpers are available through
`lonejson_sse_*` and `lonejson_multipart_*`. Serialization is exposed through
`lonejson_serialize_*`, `lonejson_serialize_jsonl_*`, and the pull-style
`lonejson_generator_init/read/cleanup` trio. Ownership and reuse of mapped
values is handled by `lonejson_cleanup` and `lonejson_reset`.

Large-value handling is expressed explicitly. `lonejson_spooled` is for inbound
values that may spill to disk, with thresholds chosen from the runtime's named
spool classes. `lonejson_source` and the corresponding
`lonejson_source_set_*` helpers are for outbound values that should be
serialized directly from an existing path, `FILE *`, or file descriptor.
`lonejson_json_value` covers the related case where a typed envelope needs to
embed one already-formed JSON value directly, and `lonejson_visit_value_*`
covers the lower-level case where one arbitrary JSON value should be visited
without a schema. Parse, write, and arbitrary-value limits are configured on
the instantiated `lonejson` runtime through `lonejson_config`.



Every public operation hangs off one instantiated runtime. You can call the
free functions such as `lonejson_parse_cstr(lj, ...)` or the equivalent method
pointer route `lj->parse_cstr(lj, ...)`; they are intentionally the same API
surface. Treat `lonejson *` as an owner-only handle: do not copy `struct
lonejson` by value, and only free the original pointer returned by
`lonejson_new()`.

Serializer-owned output APIs such as `lonejson_serialize_alloc()` and
`lonejson_serialize_owned()` are materializing APIs and are capped by the
runtime's `write_max_output_bytes`, which defaults to
`LONEJSON_WRITE_MAX_OUTPUT_BYTES` (8 MiB). Fixed caller buffers are bounded by
their explicit capacity, and sink/file/generator serializers remain streaming
or caller-bounded rather than using this materialized-output cap.

The new generator surface is the transport-facing serializer primitive. It is
bounded and pull-based rather than sink-driven, so adapters like curl upload do
not need worker threads or hidden full-payload buffers. It supports the normal
mapped serializer surface plus `lonejson_source`, `lonejson_spooled`, and
`lonejson_json_value` fields. `json_value` fields are streamed through the
generator directly instead of silently falling back to buffered serialization.

Concurrency follows the usual runtime/handle split. Different threads may call
into the same `lonejson *` concurrently, or use different runtimes
concurrently, as long as they operate on distinct mutable handles or
destination values. Mutable stateful handles such as streams, writers,
generators, spools, and `lonejson_json_value` instances may move between
threads, but they are not concurrently callable without external
synchronization.

```c
lonejson *lj = lonejson_new(NULL, &error);
lonejson_generator generator;
unsigned char chunk[4096];
size_t out_len;
int eof;

status = lonejson_generator_init(lj, &generator, &event_map, &event);
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

## Quick start

### Parse one object into a struct

```c
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
  lonejson_config config;
  user_doc doc;
  lonejson_error error;
  lonejson_status status;

  config = lonejson_default_config();
  config.max_dynamic_string_bytes = 256u * 1024u;
  lj = lonejson_new(&config, &error);
  if (lj == NULL) {
    return 1;
  }

  lonejson_init(lj, &user_doc_map, &doc);
  status = lonejson_parse_cstr(
    lj, &user_doc_map, &doc, "{\"name\":\"Ada\",\"age\":37}", &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_free(lj);
    return 1;
  }

  lonejson_cleanup(&user_doc_map, &doc);
  lonejson_free(lj);
  return 0;
}
```

For runtimes that use `STRING_ALLOC` or other lonejson-owned dynamic
destinations, `lonejson_default_config()` currently applies these
ceilings:

- `max_dynamic_string_bytes = 128 KiB`
- `max_alloc_bytes = 1 MiB`

Set either field to `0` to disable that specific ceiling for one runtime.

Reused non-empty `STRING_FIXED` fields under `clear_destination = 0` stage the
replacement before commit so parse failures do not clobber the old value. If
you want that path to stay allocation-free, provide caller-owned scratch in the
runtime config that is at least as large as the fixed field capacity:

```c
char fixed_scratch[8192];

config.clear_destination_by_default = 0;
config.fixed_string_scratch = fixed_scratch;
config.fixed_string_scratch_size = sizeof(fixed_scratch);
```

Without caller scratch, lonejson allocates temporary staging storage for that
field and counts it against `max_alloc_bytes`.

The Lua binding exposes the same reuse control through
`lonejson.fixed_string_scratch(size)`, passed into `lonejson.new({...})`.

### Read an object-framed stream

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

stream = lonejson_stream_open_fd(lj, &user_doc_map, fd, &error);
if (stream == NULL) {
  lonejson_free(lj);
  return 1;
}

for (;;) {
  lonejson_stream_result result = lonejson_stream_next(stream, &doc, &error);
  if (result == LONEJSON_STREAM_OBJECT) {
    /* process doc */
    lonejson_cleanup(&user_doc_map, &doc);
    continue;
  }
  if (result == LONEJSON_STREAM_EOF) {
    break;
  }
  /* inspect lonejson_stream_error(stream) */
  break;
}

lonejson_stream_close(stream);
lonejson_free(lj);
```

### Read a selected array stream

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
    lonejson_cleanup(&user_doc_map, &doc);
    continue;
  }
  if (result == LONEJSON_ARRAY_STREAM_EOF) {
    break;
  }
  /* inspect lonejson_array_stream_error(items) */
  break;
}

lonejson_array_stream_close(items);
lonejson_free(lj);
```

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
lonejson_init(lj, &envelope_map, &envelope);
lonejson_init(lj, &user_doc_map, &item);
handler.item_map = &user_doc_map;
handler.item_dst = &item;
handler.item = on_item;
handler.user = NULL;
lonejson_mapped_array_stream_set_handler(&envelope.items, &handler, &error);
lonejson_parse_path(lj, &envelope_map, &envelope, "/tmp/envelope.json",
                    &error);
lonejson_cleanup(&user_doc_map, &item);
lonejson_cleanup(&envelope_map, &envelope);
lonejson_free(lj);
```

### Rewrite a selected array while streaming

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

### Replace one selected value from the old value

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
    input, input_len, lonejson_owned_buffer_sink, &output, &options, &error);
lonejson_owned_buffer_free(&output);
```

Structured old values are observable through `old_value_visitor`; visitor
events are streamed and balanced, and the complete old value is not materialized.
See `examples/value_rewrite_replace_with.c` for a complete integer increment
program.

### Parse Server-Sent Events and multipart streams

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

### Serialize one mapped object

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

### Emit JSON Lines

```c
user_doc docs[2];
char out[256];
size_t written = 0u;

lonejson_serialize_jsonl_buffer(
  &user_doc_map, docs, 2u, 0u, out, sizeof(out), &written, NULL, NULL);
```

## Large field handling

### Inbound spool-backed fields

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

### Outbound source-backed fields

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
lonejson_source_set_path(&doc.payload, "payload.bin");
```

Ownership is explicit. lonejson only auto-closes sources it opened itself;
caller-owned `FILE *` and file descriptors remain caller-owned.

### Embedded arbitrary JSON values

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
                                               NULL, &error);
```

### Visiting Arbitrary JSON Values

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

- `lonejson_init(map, &value)` for mapped structs
- `lonejson_json_value_init`, `lonejson_source_init`, `lonejson_spooled_init`
  for explicit handles
- `lonejson_json_value_init_with_allocator` and
  `lonejson_spooled_init_with_allocator` when an explicit allocator should own
  future internal allocations
- `lonejson_default_config()`, `lonejson_default_value_visitor()`,
  `lonejson_default_read_result()`, and `lonejson_default_allocator()`
  for configuration/result structs

`lonejson_error` is output-only and does not need prior initialization, but
`lonejson_error_init()` is available when you want an explicit empty state.

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

Custom allocators can be attached per parse or write operation. For
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

## Benchmarks

The repository includes two benchmark harnesses:

- `make bench` runs the C benchmark suite and compares lonejson against the
  frozen baseline in `perflogs/hosts/<host-id>/baseline.json`
- `make lua-bench` runs the Lua benchmark suite and compares Lua lanes against
  `perflogs/hosts/<host-id>/lua/baseline.json`, with informational sibling
  ratios against the latest C lonejson run for the same host id
  through the embedded target runner and pinned Bootlin runtime on Linux;
  native Darwin uses its system Lua runtime.

`<host-id>` is the first field of `uname -n | md5sum`; benchmark JSON stores
that hash instead of the raw hostname.

The C benchmark harness is now self-contained. It no longer depends on YAJL or
another external C comparator library. When the benchmark schema, case set, or
measurement method changes, refresh the committed baseline with:

```sh
make bench-freeze-baseline
```

## Short aliases

`lonejson` provides optional short aliases. Functions and types are available as
`lj_*`; macros and constants are available as `LJ_*`. They are enabled by
default and can be disabled with:

```c
#define LONEJSON_DISABLE_SHORT_NAMES 1
```

## Lua binding

The repository also ships a Lua binding with schema-guided decoding, reusable
records, object-framed streams, selected-array streams, spool-backed fields,
and native-Lua arbitrary `json_value` fields backed by the C visitor path.
The native Lua module uses the public `lonejson.h` boundary and links against
`liblonejson`; it does not embed or redefine the core C API.

Build and install it into the local LuaRocks tree:

```sh
make lua-rock
```

For out-of-tree LuaRocks builds, provide the directory containing
`liblonejson` with `LONEJSON_LIBDIR`, and make that same directory available to
the platform dynamic loader when requiring the module.

Run the integration test:

```sh
make lua-test
```

Run the main Lua example:

```sh
eval "$(luarocks path --tree build/luarocks)" && lua examples/lua_binding.lua
```

Run the nested `json_value` Lua example:

```sh
eval "$(luarocks path --tree build/luarocks)" && lua examples/lua_json_value_nested.lua
```

Run the object-array `json_value` Lua example:

```sh
eval "$(luarocks path --tree build/luarocks)" && lua examples/lua_json_value_object_array.lua
```

The Lua binding now supports `json_value` fields at top level, inside nested
object fields, and inside object-array element schemas.

For reusable records on a runtime configured with
`clear_destination_by_default = false`, omitted fields remain as they were,
while present arrays replace the previous array contents instead of silently
appending.

## Example programs

See [examples/README.md](examples/README.md) for the full set. The example
programs cover one-shot parsing, object-framed streams, fixed-storage mappings,
selected-array streams, spool-to-disk fields, source-backed outbound fields,
string/file/JSONL serialization, optional curl integration, and the Lua binding.

Online:

- <https://github.com/c89systems/lonejson/tree/trunk/examples>

## Release artifacts

Release packaging is Makefile-driven:

```sh
make release
```

That command produces:

- per-target C SDK archives, `lonejson-<version>-<target-id>.tar.gz`
- a source-only archive, `lonejson-<version>.tar.gz`
- a compressed standalone single-header artifact,
  `lonejson-<version>.h.gz`
- a standalone Lua source archive, `lonejson-lua-<version>.tar.gz`
- a versioned Lua rockspec, `lonejson-<version>-1.rockspec`, and packed source
  rock, `lonejson-<version>-1.src.rock`
- a SHA-256 manifest, `dist/lonejson-<version>-CHECKSUMS`

Binary preparation keeps unrelated `dist/` artifacts and records its selected
inventory under `build/packaging/checksums/`. Use `make package-verify-binary` for
that scope. The full upload checksum manifest requires every selected SDK, Lua
product, header, and source archive; full verification also rejects unlisted
release artifacts. See [Local Verification](docs/local-verification.md).

The compressed header artifact is the standalone embedded form for
single-header integration. Its version macros are regenerated to match the
resolved release version, from `VERSION` when present in a source package,
otherwise from an exact lightweight `vX.Y.Z` tag on `HEAD`, and otherwise
`0.0.0`. Before `make release` cleans generated state or builds artifacts, it
runs the read-only `make lifecycle-version-contract` check: the owning checkout
must be clean and attached to its authoritative default branch, HEAD must have
a verified commit signature and exactly one lightweight `vX.Y.Z` tag, and no
candidate override may be set. Develop and verify on one branch, then squash
onto the local default branch before final amendment/signing and tagging.
Keep remote refs unchanged until the clean local release gate passes; never
push to trigger CI. See [local verification](docs/local-verification.md).
Ordinary CTest checks CMake overrides separately without mutating tags. The project-prefixed
`LONEJSON_VERSION_OVERRIDE=X.Y.Z` escape hatch is for release-candidate
rehearsals only; Make exports an explicit command-line override, and CMake
honors an environment override for the current configure without caching it for
later normal builds. The
source-only archive contains the repository source tree
without generated build output or local stash material. The Lua source package
prefers a curl-enabled native build when curl is available in the build
environment and falls back automatically to a curl-free build otherwise. The
same release version source of truth is used for the source-only archive and
the generated standalone header artifact.

## Compiler and cross-toolchain policy

Linux native debug, host, and release builds use pinned Bootlin stable toolchain
collections end to end: GCC, GNU ld, binutils, debugger, libc sysroot, and
target runtime all come from one cached collection rather than a host compiler
or any `~/.local/cross` installation. CMake rejects a target compiler whose
`-dumpmachine` triple does not match the collection, and package verification
uses the configured Bootlin target tools. Native memory checking uses the
host-installed Valgrind Memcheck. Fuzzing uses a pinned AFL++ GCC-plugin build
tied to the cached Bootlin x86_64 GCC collection; neither route uses a host
compiler. ThreadSanitizer probes the selected native Bootlin GCC collection at
gate time, after toolchain resolution, and skips only when that compiler cannot
build the TSan probe. To prepare AFL++ for fuzzing:

```sh
make toolchains-aflpp
```

Make entrypoints prepare their selected checksum-pinned Bootlin collection;
CMake configuration and compiler probes only discover prepared tools. Before
direct native CMake use, run `make deps-host`. To warm all
Linux release collections before a matrix build:

```sh
make toolchains-all
```

They are shared across pkt.systems projects at
`${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/cpkt/toolchains}`.
Cache overrides must use owned `build/` or `.cache/` paths or the declared
global cpkt cache; another checkout's state cannot be borrowed. Host Bash 4.4
or newer is required. Native Apple Silicon macOS
builds use the active Apple compiler and macOS SDK reported by `xcrun`, including
native presets, the Darwin release preset, curl examples, and LuaRocks. Install
Xcode or Command Line Tools first. Building Darwin artifacts on Linux requires
an explicit osxcross/SDK setup; the project never downloads Apple SDKs.
`scripts/cpkt-toolchains.sh discover` reports every supported target. Use
`discover arm64-apple-darwin` for native Apple tools on macOS or local
osxcross/host-MIG readiness on Linux.
Shared Bootlin and AFL++ provisioning waits at most
`${CPKT_TOOLCHAIN_LOCK_TIMEOUT:-600}` seconds for the matching cache lock.
Each operation owns its mutable checkout workspaces. Run lifecycle commands
serially with stable sources, configuration, and prepared tool collections.
`make` defaults to two build jobs; set `LONEJSON_BUILD_JOBS` or
`CMAKE_BUILD_PARALLEL_LEVEL` to change that limit. CTest presets run serially,
reject empty selections, and stop on failure. The terminal or job runner cancels
the entire job/process group; signalling only a helper PID is unsupported.
After copying or restoring inputs with preserved timestamps, invalidate the
affected build and verification state explicitly.

The core JSON implementation has no cpkt API dependency. Optional curl/auth
features, Lua bindings, tests and SDK builds use the core SDK from
[`github.com/c89systems/cpkt` 0.6.1](https://github.com/c89systems/cpkt/releases/tag/v0.6.1).
No `cpktdb` or `cpktmisc` package is required. Select the extracted prefix with
`LONEJSON_CPKT_ROOT`. Pinned cpkt SDK archives use the adjacent shared cache
`${CPKT_DEPENDENCY_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/cpkt/deps}`.
Each archive is verified by SHA-256 before reuse and is keyed by that digest;
only the extracted SDK root belongs to this checkout under `.cache/`. `make
clean` preserves both shared caches.

Run `make build` after checkout or full cleanup to prepare the native headers
and `build/debug/compile_commands.json` used by `.clangd`. After this dependency
migration, refresh old CMake caches with `cmake --fresh --preset <preset>`.

## Local Podman Kube e2e

Install rootless Podman with its `pasta` network helper on the host.
`make test-e2e` starts one checkout-specific pod, waits for HTTPS, OIDC, API-fixture,
and upload-sink readiness, then runs the curl, OIDC, and M2M workflows. The
five containers are declared in `devenv.yaml.in`; static configuration lives
under `devenv/`. Services communicate through pod-local loopback connections.
All published ports bind to host loopback.

`make dev-up`, `dev-ps`, `dev-logs`, `dev-down`, and `dev-reset` use
`scripts/devenv.sh`. The generated manifest is `build/devenv/devenv.yaml`;
state, temporary files, logs, and local credentials are under that same
`build/devenv/` root. The TLS CA is
`build/devenv/credentials/server.crt`. Each image's writer maps to the invoking
host user, and the e2e gate checks actual container-written file ownership.
`dev-reset` stops the pod with the saved manifest before deleting generated
state as that user. `make clean` also stops this pod before removing `build/`.

Published service ports can be overridden for parallel checkouts:
`LONEJSON_OAUTH2_E2E_PORT` (8090), `LONEJSON_OIDC_E2E_PORT` (18443),
`LONEJSON_API_FIXTURE_E2E_PORT` (18080), `LONEJSON_NGINX_HTTP_E2E_PORT` (8080),
and `LONEJSON_NGINX_HTTPS_E2E_PORT` (8443). Overrides must be distinct ports
between 1024 and 65535. Reset the pod before changing ports. On success,
`test-e2e` removes its pod and generated service state. On failure it stops
the pod and retains state for diagnosis. Set
`LONEJSON_E2E_KEEP_DEVSERVICES=1` to retain the pod and state for debugging.
Run `make dev-reset` when finished.

## Verification

The standard verification commands are:

```sh
make test
make test-all
make test-e2e
make cross-sanitizers
make asan
make bench-gate
make lua-bench-gate
make fuzz
```

`make test` is the debug lifecycle gate. It builds current debug artifacts,
runs the debug CTest preset, and then runs the Lua integration test. Do not use
raw `ctest --preset debug` as completion or release evidence; raw CTest is only
diagnostic after a matching configure/build step. See
[Local Verification](docs/local-verification.md) for the exact boundary.

`make test-all` is the broader deterministic local confidence gate: debug,
host, curl/auth host, cross presets, host sanitizers, Valgrind, deterministic
local e2e, fuzz smoke, and C/Lua benchmark gates when frozen baselines exist
for the current host.
`make cross-sanitizers` is an extra hardening check for the currently supported
QEMU sanitizer route, `armhf-linux-gnu` ASan/UBSan; it is intentionally outside
the normal release gate because native ASan/UBSan and TSan provide the
mandatory sanitizer coverage. `make prerelease` runs the complete release
pipeline without cleaning generated state first: native debug/Lua tests,
sanitizers, Valgrind, deterministic e2e, fuzz smoke and benchmark checks, then
the release matrix that builds, checksums, and verifies binary SDKs and Lua
artifacts. `make release` is the final clean release gate; it first validates clean committed source and an exact lightweight
release tag, then cleans generated state, then runs the same release
pipeline, followed by source-archive reconstruction and full artifact
verification. Source reconstruction is reserved for the final release path.
`make test-all-bindings` is a compatibility alias for the Lua
binding suite; it no longer expands to the full world gate. Use `bench-check`,
`bench-gate`, and Lua benchmark targets for focused performance verification.
Hosts without frozen baselines report an explicit benchmark skip; establish
baselines deliberately with the documented freeze targets.

## License

See [LICENSE](LICENSE).
