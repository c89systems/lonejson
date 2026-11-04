# lonejson Auth Implementation

This document describes the current JWT, JWK, JWKS, OAuth2, and OIDC
implementation in lonejson. It is a status writeup for the implemented feature
surface, including boundaries, invariants, verification, and known gaps.

The implementation is intentionally not a full identity-provider SDK, HTTP
client, HTTP server, Kore integration, or Vectis integration. It provides
bounded JSON-centered authentication primitives and small flow helpers. Network
fetching, browser launching, request routing, response writing, retry policy,
proxy policy, TLS policy, credential storage, logging policy, and framework
lifetime rules remain caller-owned.

## Feature Gates

The auth surface is compile-time optional.

- `LONEJSON_WITH_OPENSSL` enables OpenSSL-backed cryptographic internals.
- `LONEJSON_WITH_JWT` enables JWT, JWK, JWKS, and claim/signature validation
  APIs. It does not require OpenSSL at the core API boundary.
- `LONEJSON_WITH_CURL` enables existing curl adapter APIs.
- `LONEJSON_WITH_OIDC` enables OAuth2/OIDC helpers. It requires
  `LONEJSON_WITH_JWT`, but does not require curl or OpenSSL at the core API
  boundary.

The corresponding CMake options are:

- `LONEJSON_BUILD_WITH_OPENSSL`
- `LONEJSON_BUILD_WITH_JWT`
- `LONEJSON_BUILD_WITH_CURL`
- `LONEJSON_BUILD_WITH_OIDC`

CMake enforces only the core feature graph. `LONEJSON_BUILD_WITH_OIDC=ON`
without JWT is a configure error. JWT and OIDC builds without OpenSSL/curl are
valid and must compile; trust operations that need crypto fail with an
actionable missing-provider error until an auth provider is installed.

When `LONEJSON_CPKT_ROOT` is set, enabled integrations are resolved
through the cpkt bundle. OIDC-capable release/package builds enable the
curl and JWT/OIDC integration surfaces but keep the OpenSSL adapter disabled for
the plain core SDK. Normal consumers, and JWT/OIDC core consumers that provide
their own auth providers, do not need curl or OpenSSL headers.

## Dependency And Packaging Posture

JWT/OIDC core builds do not own HTTP or crypto dependencies. They expose
bounded parsing, policy, cache, and validation orchestration. Cryptographic
trust decisions go through a `lonejson_auth_provider` installed on the runtime:

```c
lonejson_auth_provider provider;
lonejson_auth_provider_init_openssl(&provider, NULL, &error);
lonejson_set_auth_provider(runtime, &provider, &error);
```

The provider vtable is receiver-style and narrow: verify one JWS, produce
random bytes, and compute SHA-256. Signature validation uses `verify_jws`;
PKCE challenge generation uses `sha256`; PKCE verifier generation and M2M
credential/signup helpers use both `random_bytes` and `sha256`. Downstream
projects that already own OpenSSL can use the optimized OpenSSL initializer.
Projects using another crypto stack can provide equivalent callbacks without
exposing that stack through `lonejson.h`. Both choices are valid; OpenSSL is a
convenience adapter, not the auth contract.
Runtime userdata also exposes method pointers for runtime-backed auth helpers,
including JWT decode/signature verification, JWK/JWKS parsing, OIDC discovery
fetch/parse, JWKS cache update/refresh, OAuth2 token response parsing and
provider-backed token exchanges, and bearer-token validation. These method
pointers are wired to the same implementation as the free functions; there is
no separate runtime code path.

OpenSSL-enabled builds link OpenSSL Crypto only for the optional OpenSSL auth
adapter. That adapter is an integration, not a baseline package dependency. The
plain release SDK must not expose OpenSSL, libcrypto, or libssl in pkg-config
or CMake consumer metadata.

HTTP follows the same rule. OAuth2/OIDC fetch helpers call a
`lonejson_http_provider`; the provider implementation may use libcurl or any
other transport that satisfies the callback contract. Curl helpers are
optimized adapters for the libcurl path, not a mandatory dependency of the
plain SDK.

Release archive verification also checks that public package metadata does not
unconditionally expose curl/OpenSSL requirements to consumers of the plain
`lonejson` package. Binary release archives must still include and verify the
auth/curl ABI symbols when built as release artifacts.

The general third-party integration contract is documented in
`docs/third-party-integrations.md`.

The important release checks are:

- build-tree curl ABI symbol checks,
- build-tree JWT/OIDC ABI symbol checks,
- release archive metadata checks,
- release archive dependency-leak checks,
- release archive ABI symbol checks,
- Darwin Mach-O metadata checks for install names and dependency paths.

## Ownership And Memory Model

The C API keeps parse and trust decisions explicit.

- JWT compact parsing returns slices into the caller-provided token.
- Parsed JWK, JWKS, JWT header, JWT claims, OIDC discovery, OAuth2 token
  response, PKCE, callback, JWKS cache, and bearer validation objects own their
  retained strings where documented.
- Every owning object has an init/cleanup pair.
- JWKS cache selection returns pointers into the caller-owned cache. The caller
  must keep the cache alive while selected JWK pointers or successful bearer
  validation results are used.
- OAuth2 and OIDC URL/body builders write into `lonejson_owned_buffer`.
- Size limits are explicit where attacker-controlled JSON or callback data can
  grow.

JWT and JWK parsing necessarily materializes decoded JWT header/claims JSON and
JWK/JWKS key documents into bounded in-memory structures. This is not a
streaming API for arbitrarily large auth documents. The values involved in
normal JWT/JWK/OIDC flows are expected to be small, and the implementation
rejects configured over-size inputs.

## JWT Compact And Base64url

Implemented public APIs:

- `lonejson_base64_encoded_len`
- `lonejson_base64_encode`
- `lonejson_base64_encode_sink`
- `lonejson_base64_decoded_len`
- `lonejson_base64_decode`
- `lonejson_base64_decode_sink`
- `LONEJSON_BASE64_URL_RAW` for unpadded JWT/JWS base64url segments
- `lonejson_jwt_parse_compact`
- short aliases `lj_base64_*`, `LJ_BASE64_URL_RAW`, and
  `lj_jwt_parse_compact`

Lua exposes materialized base64 helpers as `base64_encode(data, variant)` and
`base64_decode(text, variant)` on the module table and runtime userdata.
Supported variant strings are `standard`, `standard_raw`, `url`, and `url_raw`;
`jwt` aliases `url_raw`.

`lonejson_jwt_parse_compact` validates compact serialization shape only:

- exactly three compact JWT/JWS segments,
- non-empty header segment,
- non-empty payload segment,
- signature segment may be empty at parse time,
- every segment uses unpadded base64url syntax,
- no `=` padding,
- no `+` or `/`,
- no extra fourth segment.

The result contains caller-owned slices:

- `header`,
- `payload`,
- `signature`,
- `signing_input`.

Parsing a compact token does not decode JSON, validate a signature, validate
claims, validate issuer/audience, or establish trust.

## JWK And JWKS

Implemented public APIs:

- `lonejson_jwk_init`
- `lonejson_jwk_cleanup`
- `lonejson_jwks_init`
- `lonejson_jwks_cleanup`
- `lonejson_jwk_parse_json`
- `lonejson_jwks_parse_json`
- `lonejson_jwks_select`
- short aliases `lj_jwk_*`, `lj_jwks_*`

Implemented JWK fields:

- `kty`
- `kid`
- `alg`
- `use`
- `crv`
- `n`
- `e`
- `x`
- `y`
- `k`
- `key_ops`
- `x5c`
- `x5t`
- `x5t#S256`

Implemented JWK shape checks:

- `kty` is required.
- RSA keys require valid base64url `n` and `e`.
- EC keys require `crv` and valid base64url `x` and `y`.
- OKP keys require `crv` and valid base64url `x`.
- symmetric keys require valid base64url `k`.
- JWKS documents require a non-empty `keys` array.

`lonejson_jwks_select` selects the first key matching all non-NULL filters in
`lonejson_jwk_select_options`:

- `kid`,
- `kty`,
- `alg`,
- `use`.

No trust is established by parsing or selecting a JWK. Selection only chooses a
candidate key.

## JWT Header And Claims Decode

Implemented public APIs:

- `lonejson_jwt_header_init`
- `lonejson_jwt_header_cleanup`
- `lonejson_jwt_claims_init`
- `lonejson_jwt_claims_cleanup`
- `lonejson_jwt_decode_compact`
- short aliases `lj_jwt_header_*`, `lj_jwt_claims_*`,
  `lj_jwt_decode_compact`

Decoded header fields:

- `alg`,
- `kid`,
- `typ`,
- `crit`,
- `x5t`,
- `x5t#S256`,
- `x5c`.

Decoded claim fields:

- `iss`,
- `sub`,
- `nonce`,
- `azp`,
- `scope`,
- `scp`,
- `aud` as string,
- `aud` as array of strings,
- `exp`,
- `nbf`,
- `iat`.

`lonejson_jwt_decode_compact` performs:

- compact serialization validation,
- base64url decoding,
- decoded header JSON size checks,
- decoded claims JSON size checks,
- JSON syntax checks,
- duplicate registered header/claim detection,
- embedded NUL rejection for registered header and claim strings,
- registered claim type checks,
- integer time claim checks.

It does not validate signatures, trust, issuer, audience, clock policy, or
required claims.

Relevant policy limits:

- `max_token_bytes`,
- `max_decoded_header_bytes`,
- `max_decoded_claims_bytes`.

Zero decoded JSON limits use internal defaults.

## JWT Claim Validation

Implemented public APIs:

- `lonejson_jwt_validate_claims`
- short alias `lj_jwt_validate_claims`

Claim validation uses `lonejson_jwt_claim_policy`.

Required trust-policy members:

- accepted algorithms,
- accepted issuers,
- accepted audiences,
- current time,
- non-negative allowed clock skew.

Optional policy members:

- expected nonce,
- expected authorized party (`azp`),
- accepted critical JOSE header names,
- required OAuth2 scopes,
- require `azp` for multi-audience tokens,
- require every JWT audience to be accepted,
- required claims,
- maximum token bytes,
- maximum decoded header bytes,
- maximum decoded claims bytes.

Claim validation enforces:

- `alg` must be present and accepted,
- `alg: none` is rejected,
- issuer must match an accepted issuer,
- string or array audience must include an accepted audience,
- nonce must match the expected nonce when configured,
- `azp` must match the expected authorized party when configured,
- every critical JOSE header must be explicitly accepted,
- required OAuth2 scopes must appear in `scope` or `scp`,
- multi-audience tokens must include `azp` when configured,
- every audience must be accepted when strict multi-audience policy is enabled,
- required claims must be present,
- expired tokens fail,
- not-yet-valid tokens fail,
- issued-at values in the future fail beyond configured skew,
- negative clock skew is invalid policy.

This API is a claim trust decision only. It does not validate the JWS
signature. Callers must compose it with signature validation or use the bearer
validation helper.

## JWT Signature Validation

Implemented public APIs:

- `lonejson_jwt_validate_signature`
- `lonejson_jwt_validate_signature_with_runtime`
- `lonejson_set_auth_provider`
- `lonejson_auth_provider_init_openssl` when `LONEJSON_WITH_OPENSSL` is enabled
- short aliases `lj_jwt_validate_signature`,
  `lj_jwt_validate_signature_with_runtime`, `lj_set_auth_provider`,
  `lj_auth_provider_init_openssl`

Implemented algorithm support:

- `RS256`,
- `PS256`,
- `ES256`,
- `EdDSA` for Ed25519 OKP keys.

The compatibility free function has no built-in crypto dependency. The
preferred API is `lonejson_jwt_validate_signature_with_runtime`, which
dispatches to the auth provider installed on the runtime. Consumers that want
the OpenSSL easy path can compile with `LONEJSON_WITH_OPENSSL` and initialize
that provider with `lonejson_auth_provider_init_openssl()`. Without a provider
capable of `verify_jws`, signature validation fails before claims are trusted.

Signature validation enforces:

- header `alg` is present,
- `alg: none` is rejected,
- JWK `alg`, when present, must match the JWT header algorithm,
- JWK `kid`, when present alongside header `kid`, must match,
- JWK `use`, when present, must be `sig`,
- JWK `key_ops`, when present, must allow `verify`,
- `RS256` requires `kty: RSA`,
- RSA modulus and exponent must decode and be bounded,
- JWK `x5c`, when present with the OpenSSL provider, must decode to a trusted
  X.509 chain,
- JWK or header `x5t`/`x5t#S256`, when present, must match the `x5c` leaf
  certificate,
- the `x5c` leaf public key must match the selected JWK key material,
- signature must be present and non-empty,
- cryptographic verification must succeed.

The OpenSSL provider verifies `x5c` chains against
`lonejson_openssl_auth_provider_config.x509_store` when supplied, otherwise
against a temporary store using OpenSSL default verify paths. The provider
copies the `x509_store` pointer value during init; the config object may go out
of scope after init, but the caller owns the store object and must keep it alive
while any runtime can use the provider.
Without `LONEJSON_WITH_OPENSSL`, JWK certificate fields are parsed and syntax
checked only; no certificate trust decision is available from the built-in
provider surface.

This API is a signature trust decision only. It does not validate issuer,
audience, time claims, or required claims.

## OIDC Discovery

Implemented public APIs:

- `lonejson_oidc_discovery_init`
- `lonejson_oidc_discovery_cleanup`
- `lonejson_oidc_discovery_url`
- `lonejson_oidc_discovery_parse_json`
- `lonejson_oidc_discovery_validate_issuer`
- `lonejson_oidc_fetch_discovery`
- short aliases `lj_oidc_discovery_*`

Implemented discovery fields:

- `issuer`,
- `authorization_endpoint`,
- `token_endpoint`,
- `jwks_uri`,
- `introspection_endpoint`,
- `revocation_endpoint`,
- `userinfo_endpoint`.

`lonejson_oidc_discovery_url` builds the discovery URL for HTTPS issuers.
Path-based issuers follow OpenID Connect discovery placement:

```text
https://host/path -> https://host/path/.well-known/openid-configuration
```

Discovery parsing validates required metadata shape and HTTPS URL fields.
`lonejson_oidc_fetch_discovery` composes discovery URL construction, the
runtime HTTP provider, HTTP 2xx checking, response-size enforcement, bounded
JSON parsing, and issuer validation. The helper enforces the configured
discovery response cap even when a caller-provided HTTP provider does not
enforce `request.max_response_bytes` itself. The helper establishes metadata
integrity for the expected issuer; it does not fetch JWKS or validate tokens.

The runtime HTTP provider is installed with `lonejson_set_http_provider` or the
runtime `set_http_provider` method. Providers receive a bounded
`lonejson_http_request` and fill a materialized `lonejson_http_response`.
`lonejson_http_provider_init` initializes a provider from caller-owned callback
config:

- `user_data`,
- `user_agent`,
- `request` callback.

`lonejson_http_provider_init_simple` is the same initialization path for callers
that prefer direct arguments over a config struct. It accepts `user_data`,
`user_agent`, and the `request` callback explicitly, so even the compact setup
form can set the provider's default user-agent.

The helper layer copies the provider's `user_agent` into each request unless a
request already has one. Core lonejson does not embed curl, OpenSSL transport,
event-loop, proxy, retry, TLS, or telemetry policy in the shared library;
applications provide that behavior through the provider callback.

## OIDC JWKS Cache

Implemented public APIs:

- `lonejson_oidc_jwks_cache_init`
- `lonejson_oidc_jwks_cache_cleanup`
- `lonejson_oidc_jwks_cache_update_json`
- `lonejson_oidc_jwks_cache_refresh`
- `lonejson_oidc_jwks_cache_is_fresh`
- `lonejson_oidc_jwks_cache_select`
- `lonejson_oidc_jwks_cache_parse_init`
- `lonejson_oidc_jwks_cache_write_callback`
- `lonejson_oidc_jwks_cache_parse_finish`
- `lonejson_oidc_jwks_cache_parse_cleanup`
- short aliases `lj_oidc_jwks_cache_*`

`lonejson_oidc_jwks_cache_policy` supplies:

- expected issuer,
- JWKS URI,
- maximum JWKS bytes,
- current time,
- TTL seconds.

Cache update:

- requires HTTPS issuer and JWKS URI,
- requires positive TTL,
- requires non-negative current time,
- checks `now + ttl_seconds` overflow,
- enforces bounded JWKS JSON length,
- parses and validates the JWKS document,
- rejects empty JWKS,
- records issuer, URI, fetch time, expiry time, and size limit.

Cache selection:

- requires the cache issuer and URI to match policy,
- requires the cache to be unexpired at `policy->now`,
- selects a JWK using the normal JWK selection filters.

`lonejson_oidc_jwks_cache_refresh` performs a provider-backed GET of
`policy->jwks_uri`, requires HTTP 2xx, then installs the bounded JWKS JSON with
`lonejson_oidc_jwks_cache_update_json`. Callers may still fetch bytes through
their own HTTP code and call `lonejson_oidc_jwks_cache_update_json`, or use the
lower-level `lonejson_oidc_jwks_cache_parse_*` curl write-callback adapter.
Retry, TLS policy, proxy policy, and event-loop integration remain provider or
application policy.

## OAuth2 Client Credentials

Implemented public APIs:

- `lonejson_oauth2_client_credentials_body`
- `lonejson_oauth2_refresh_token_body`
- `lonejson_oauth2_token_introspection_body`
- `lonejson_oauth2_token_revocation_body`
- `lonejson_oidc_authorization_code_token_body`
- `lonejson_oauth2_client_credentials_request`
- `lonejson_oauth2_refresh_token_request`
- `lonejson_oauth2_token_flow_init`
- `lonejson_oauth2_token_flow_cleanup`
- `lonejson_oauth2_token_flow_assign`
- `lonejson_oauth2_token_flow_is_expired`
- `lonejson_oauth2_token_flow_update_response`
- `lonejson_oauth2_token_flow_ensure`
- `lonejson_oauth2_introspect_token_request`
- `lonejson_oauth2_revoke_token_request`
- `lonejson_oidc_fetch_userinfo`
- `lonejson_oidc_authorization_code_token_request`
- `lonejson_oauth2_token_response_init`
- `lonejson_oauth2_token_response_cleanup`
- `lonejson_oauth2_token_response_parse_json`
- `lonejson_oauth2_introspection_response_init`
- `lonejson_oauth2_introspection_response_cleanup`
- `lonejson_oauth2_introspection_response_parse_json`
- `lonejson_oidc_userinfo_response_init`
- `lonejson_oidc_userinfo_response_cleanup`
- `lonejson_oidc_userinfo_response_parse_json`
- short aliases `lj_oauth2_*`

`lonejson_oauth2_client_credentials_body` builds an
`application/x-www-form-urlencoded` body for `client_secret_post` token
endpoint authentication.

Implemented request fields:

- `client_id`,
- `client_secret`,
- `scope`,
- `audience`,
- `resource`,
- `max_body_bytes`.

The helper always sets:

```text
grant_type=client_credentials
```

Then it appends configured fields with form encoding and enforces
`max_body_bytes` or an internal default.

`lonejson_oauth2_refresh_token_body` builds a refresh-token exchange body with
`grant_type=refresh_token`, required `refresh_token`, optional `client_id`,
optional `client_secret`, optional scope narrowing, and the same body limit
rules. `client_id` is required when `client_secret` is supplied.

`lonejson_oauth2_token_introspection_body` and
`lonejson_oauth2_token_revocation_body` build bounded form bodies for RFC 7662
token introspection and RFC 7009 token revocation. They require `token`, accept
optional `token_type_hint`, and support optional `client_id`/`client_secret`
for `client_secret_post`; `client_id` is required when `client_secret` is
supplied. Set `use_basic_auth` to use `client_secret_basic` in provider-backed
requests. In that mode the form body omits `client_id` and `client_secret`, and
the helper sends `Authorization: Basic <base64(form(client_id):form(client_secret))>`,
where each credential component is encoded with OAuth2
`application/x-www-form-urlencoded` rules before joining with `:`.

`lonejson_oidc_authorization_code_token_body` builds an authorization-code
token exchange body with `grant_type=authorization_code`, required `client_id`,
`code`, `redirect_uri`, and `code_verifier`, optional `client_secret`, and the
same body limit rules.

`lonejson_oauth2_token_response_parse_json` parses a bounded successful token
endpoint response.

`lonejson_oauth2_client_credentials_request`,
`lonejson_oauth2_refresh_token_request`, and
`lonejson_oidc_authorization_code_token_request` compose form-body
construction, a provider-backed HTTPS POST, HTTP 2xx checking, and bounded token
response parsing. `lonejson_oauth2_introspect_token_request` and
`lonejson_oauth2_revoke_token_request` use the same provider-backed HTTPS POST
model against discovery's `introspection_endpoint` and `revocation_endpoint`.
Introspection parses a bounded response and requires the RFC 7662 `active`
field. Revocation accepts any HTTP 2xx response body and returns success.
`lonejson_oidc_fetch_userinfo` performs a provider-backed HTTPS GET with
`Authorization: Bearer <access_token>`, validates a bounded JSON response, keeps
the exact JSON bytes, and copies common claims (`sub`, `name`,
`preferred_username`, `email`, `email_verified`) when present.

Implemented response fields:

- `access_token`,
- `token_type`,
- `refresh_token`,
- `scope`,
- `id_token`,
- `error`,
- `error_description`,
- `error_uri`,
- `expires_in`.

Validation rules:

- successful responses require an access token,
- token type must be `Bearer`, case-insensitive,
- provider error responses are rejected as `LONEJSON_STATUS_TYPE_MISMATCH`,
- response JSON is bounded by `max_response_bytes` or an internal default.

Transport execution remains provider-owned. The built-in helpers require a
runtime HTTP provider; callers that need different flow control can still post
the generated body themselves and parse with
`lonejson_oauth2_token_response_parse_json`.

`lonejson_oauth2_token_flow_*` adds a narrowly scoped client-flow state helper.
The flow object owns copied token strings (`access_token`, `token_type`,
`refresh_token`, `scope`, `id_token`) and optional `expires_at`. It is
zero-initializable, can be cleaned repeatedly, and is not durable storage.

Typical C usage:

```c
lonejson_oauth2_token_flow flow;
lonejson_oauth2_token_flow_policy flow_policy;
lonejson_oauth2_token_flow_result flow_result;

lonejson_oauth2_token_flow_init(&flow);
lonejson_oauth2_token_flow_update_response(&flow, &initial_token, now, &error);

memset(&flow_policy, 0, sizeof(flow_policy));
flow_policy.token_endpoint = discovery.token_endpoint;
flow_policy.client_id = client_id;
flow_policy.client_secret = client_secret;
flow_policy.now = now;

status = lonejson_oauth2_token_flow_ensure(lj, &flow, &flow_policy,
                                           &flow_result, &error);
```

`lonejson_oauth2_token_flow_ensure` returns `LONEJSON_STATUS_OK` with one of
these result states:

- `LONEJSON_OAUTH2_TOKEN_FLOW_READY`: the existing access token is usable,
- `LONEJSON_OAUTH2_TOKEN_FLOW_REFRESHED`: the helper refreshed and updated the
  flow,
- `LONEJSON_OAUTH2_TOKEN_FLOW_NEEDS_INTERACTION`: the helper cannot continue
  without caller/user interaction, commonly because no refresh token exists or
  refresh was disabled,
- `LONEJSON_OAUTH2_TOKEN_FLOW_FAILED`: refresh failed; inspect status/error.

The default refresh skew is 60 seconds. Zero `max_retries` means the default of
two additional attempts; set `disable_retry` for exactly one attempt. Retries
apply only to transient provider/callback failures and HTTP 429/5xx token
endpoint responses. No sleep or background scheduler is hidden in the helper;
applications that need wall-clock backoff keep that policy in their HTTP
provider or caller loop.

Refresh responses that omit `refresh_token` preserve the previous refresh token
because many providers do not rotate on every refresh. Responses that include a
new refresh token replace the old value. The helper never refreshes a
server-side presented bearer token; resource-server validation still fails
closed on expiry.

## Authorization Code With PKCE

Implemented public APIs:

- `lonejson_oidc_pkce_init`
- `lonejson_oidc_pkce_cleanup`
- `lonejson_oidc_pkce_challenge`
- `lonejson_oidc_pkce_generate`
- `lonejson_oidc_authorization_url`
- `lonejson_oidc_authorization_callback_init`
- `lonejson_oidc_authorization_callback_cleanup`
- `lonejson_oidc_authorization_callback_parse_query`
- short aliases `lj_oidc_pkce_*`, `lj_oidc_authorization_*`

PKCE support:

- computes S256 challenge from a caller-provided verifier,
- validates verifier length and allowed characters,
- generates random verifiers through the runtime auth provider,
- uses 32 random bytes by default,
- allows verifier byte counts that produce RFC 7636 verifier lengths of
  43..128 characters,
- base64url encodes without padding.

The compatibility free functions `lonejson_oidc_pkce_challenge()` and
`lonejson_oidc_pkce_generate()` have no built-in crypto dependency. Call
`lonejson_oidc_pkce_challenge_with_runtime()` or
`lonejson_oidc_pkce_generate_with_runtime()` after installing an auth provider.
`sha256` is required for challenge computation; both `random_bytes` and
`sha256` are required for verifier generation. Consumers that want the OpenSSL
easy path can compile with `LONEJSON_WITH_OPENSSL` and initialize that provider
with `lonejson_auth_provider_init_openssl()`.

Authorization URL construction requires:

- HTTPS authorization endpoint,
- client id,
- redirect URI,
- state,
- nonce,
- code challenge.

It emits:

- `response_type=code`,
- `client_id`,
- `redirect_uri`,
- optional `scope`,
- `state`,
- `nonce`,
- `code_challenge`,
- `code_challenge_method=S256`,
- optional `audience`,
- optional `resource`.

The redirect URI is caller-provided and may be a local HTTP callback URI for
desktop/CLI flows. The authorization endpoint itself must be HTTPS.

Callback query parsing:

- accepts a query with or without leading `?`,
- percent-decodes form-style query values,
- validates required expected state,
- rejects provider error callbacks,
- rejects duplicate known fields,
- rejects malformed percent encoding,
- requires a code on success,
- applies `max_query_bytes` or an internal default.

Browser launching, local HTTP listener creation, random callback path
management, and ID-token-specific flow orchestration are not implemented in
core lonejson. ID-token nonce validation is available through
`lonejson_jwt_claim_policy.expected_nonce`. Authorization-code token POST
execution is available through the runtime HTTP provider helper or through
caller-owned HTTP code plus token-response parsing.

## Server-Side Bearer Validation

Implemented public APIs:

- `lonejson_auth_failure_string`
- `lonejson_oidc_bearer_validation_init`
- `lonejson_oidc_bearer_validation_cleanup`
- `lonejson_oidc_authorization_bearer_token`
- `lonejson_oidc_validate_bearer_token`
- short aliases `lj_auth_failure_string`,
  `lj_oidc_bearer_validation_*`,
  `lj_oidc_authorization_bearer_token`,
  `lj_oidc_validate_bearer_token`

`lonejson_oidc_authorization_bearer_token` extracts the compact JWT token from
an HTTP `Authorization` header value. It accepts optional leading/trailing HTTP
OWS and case-insensitive `Bearer`, requires a token, and rejects trailing
non-whitespace data.

`lonejson_oidc_validate_bearer_token` composes:

- Authorization Bearer extraction,
- JWT compact parse,
- JWT header and claims decode,
- required `kid`,
- fresh JWKS cache selection,
- signature validation,
- claim policy validation.

Inputs are supplied by `lonejson_oidc_bearer_validation_request`:

- Authorization header value,
- caller-owned JWKS cache,
- JWKS cache policy,
- JWT claim policy.

On success, `lonejson_oidc_bearer_validation` contains:

- `failure == LONEJSON_AUTH_FAILURE_NONE`,
- decoded validated header,
- decoded validated claims,
- selected JWK pointer into the caller-owned JWKS cache.

On failure, the result is classified as one of:

- `missing_credentials`,
- `malformed_token`,
- `cache_unavailable`,
- `key_not_found`,
- `invalid_signature`,
- `expired_token`,
- `not_yet_valid`,
- `issuer_mismatch`,
- `audience_mismatch`,
- `claims_invalid`.

This helper is framework-neutral. It does not read request objects, route
requests, write HTTP responses, refresh the JWKS cache, or own framework
lifetime.

## Lua Facade

The Lua binding exposes the auth surface by calling into the C implementation.
It does not implement independent Lua crypto or protocol logic.

JWT/JWK/JWKS Lua facade:

- `jwt_parse_compact`
- `jwt_decode_compact`
- `jwt_validate_compact_claims`
- `jwt_validate_compact_signature`
- `jwk_parse_json`
- `jwks_parse_json`
- `jwks_select_json`
- `set_openssl_auth_provider` on runtime userdata when compiled with OpenSSL

OIDC/OAuth2 Lua facade:

- `oidc_discovery_url`
- `oidc_discovery_parse_json`
- `oidc_fetch_discovery` on runtime userdata
- `oidc_jwks_cache_select_json`
- `oidc_jwks_cache_refresh` on runtime userdata
- `oauth2_client_credentials_body`
- `oauth2_refresh_token_body`
- `oauth2_token_introspection_body`
- `oauth2_token_revocation_body`
- `oidc_authorization_code_token_body`
- `oauth2_client_credentials_request` on runtime userdata
- `oauth2_refresh_token_request` on runtime userdata
- `oauth2_token_flow_update_response`
- `oauth2_token_flow_is_expired`
- `oauth2_token_flow_ensure` on runtime userdata
- `oauth2_introspect_token_request` on runtime userdata
- `oauth2_revoke_token_request` on runtime userdata
- `oidc_userinfo_request` on runtime userdata
- `oidc_authorization_code_token_request` on runtime userdata
- `oauth2_token_response_parse_json`
- `oauth2_introspection_response_parse_json`
- `oidc_userinfo_response_parse_json`
- `oidc_pkce_challenge`
- `oidc_pkce_generate`
- `oidc_authorization_url`
- `oidc_authorization_callback_parse_query`
- `oidc_validate_bearer_token`

`oidc_pkce_challenge` and `oidc_pkce_generate` are also available on runtime
userdata. Runtime form uses the installed auth provider, so Lua builds without
the OpenSSL adapter can still support PKCE when the embedding application
installs provider callbacks. Calls without an explicit runtime use a temporary
runtime; when the Lua module is compiled with OpenSSL, that temporary runtime
installs the optional OpenSSL provider before cryptographic work.

M2M/API-key Lua facade on runtime userdata:

- `m2m_credential_generate`
- `m2m_verify_authorization`
- `m2m_signup_generate`
- `m2m_signup_complete`

The Lua M2M facade accepts normal Lua values in `claim` and converts them to
validated JSON before calling the C implementation. Callers that already own
JSON may pass `claim_json`. Generated `record_json` values are returned as JSON
strings because store persistence and mutation remain caller-owned. These
runtime methods are registered only when the Lua module is built with the
OpenSSL auth provider, because the C M2M helpers require random bytes and
SHA-256 hashing.

The functions are registered both on the module table and runtime userdata
where applicable. Provider-backed helpers are runtime-only because they require
the runtime's installed provider. Lua runtimes install the optional OpenSSL auth
provider with `runtime:set_openssl_auth_provider()` when the Lua module is
compiled with that adapter. Runtime-form
`runtime:jwt_validate_compact_signature(...)` uses that
runtime auth provider; runtime-form PKCE helpers use the same provider's
`sha256`/`random_bytes` callbacks. Module-table calls may also receive a
runtime as their first argument; otherwise they use a temporary runtime. When
the Lua module is compiled with OpenSSL, module-table temporary runtimes install
the optional OpenSSL provider in the Lua binding layer. Lua runtimes install the
HTTP provider
with `runtime:set_http_provider(callback, user_agent)`. The callback receives a
bounded request table containing `method`, `url`, optional `content_type`,
optional `authorization`, optional `user_agent`, optional `body`, `body_len`,
and `max_response_bytes`, and returns a response table with `status_code` and
optional `content_type`/`body`. The Lua binding does not implement HTTP
transfer itself; the callback is the caller-owned transport boundary while C
still performs URL/body construction, response-size enforcement, JSON parsing,
and validation.

Lua policy tables map directly to the C policy concepts:

- `accepted_algs`,
- `accepted_issuers`,
- `accepted_audiences`,
- `expected_nonce`,
- `required_claims`,
- `now`,
- `allowed_clock_skew`,
- `max_token_bytes`,
- `max_decoded_header_bytes`,
- `max_decoded_claims_bytes`.

Lua OIDC cache policy tables map to:

- `issuer`,
- `jwks_uri`,
- `max_jwks_bytes`,
- `now`,
- `ttl_seconds`.

Lua failure returns follow existing binding conventions. Most auth helpers
return the normal result on success and `nil, error` on failure. Bearer
validation returns `nil, error, failure` on classified auth denial.

## Security Invariants Covered

Implemented and tested invariants include:

- JWT parse does not imply trust.
- JWT signature validation does not imply claim trust.
- JWT claim validation does not imply signature trust.
- `alg: none` is rejected by validation.
- Unsupported algorithms are rejected.
- RS256 requires RSA JWK material.
- JWK `alg`, when present, must match the JWT header algorithm.
- JWK `use`, when present, must be `sig` for signature validation.
- JWK `kid`, when present with header `kid`, must match.
- Server-side bearer validation requires a header `kid` and does not silently
  fall back to an arbitrary key.
- Unknown issuer fails closed.
- Unknown audience fails closed.
- OIDC nonce mismatch fails closed when an expected nonce is configured.
- Expired token fails closed.
- Not-yet-valid token fails closed.
- Issued-at in the future fails beyond configured skew.
- Duplicate registered JWT header/claim fields are rejected.
- Embedded NUL bytes in registered JWT header/claim strings are rejected before
  trust-policy comparison.
- Malformed registered claims are rejected deterministically.
- Oversized decoded JWT header/claims are rejected.
- Oversized discovery responses are rejected even if the HTTP provider ignores
  the request cap.
- Oversized JWKS JSON is rejected.
- Empty JWKS cache installation is rejected.
- Negative JWKS cache policy time is invalid.
- Expired or issuer/URI-mismatched JWKS cache selection fails.
- Token endpoint provider error JSON is rejected.
- OAuth2 token response token type must be Bearer.
- Authorization callback provider error is rejected.
- Authorization callback state mismatch is rejected.
- Authorization callback duplicate fields are rejected.
- Authorization callback query size is bounded.
- M2M credential records do not contain cleartext client secrets or API keys.
- M2M Basic and Bearer verification both authenticate against the same
  caller-owned JSON credential store.
- M2M verification can disable Basic or Bearer per request.
- M2M wrong-secret verification fails closed.
- M2M signup records do not contain cleartext signup secrets.
- M2M signup completion requires the signup secret and required email metadata.
- M2M signup completion can issue Bearer-only credentials.

## Verification

The implementation is covered by:

- C unit and regression tests in `tests/test_jwt.inc.h`,
- Lua facade tests in `tests/test_lua.lua`,
- ABI export checks in `scripts/check_jwt_abi_symbols.sh`,
- release archive verifier checks in `tests/test_release_archive_verify.sh`,
- fuzzer coverage in `fuzz/fuzz_jwt.c`,
- makefile fuzz target registration checks,
- cpkt dependency route tests,
- curl/OpenSSL dependency leakage checks,
- release metadata checks,
- OIDC/OAuth2 e2e in `scripts/test_oidc_e2e.sh`,
- M2M/signup e2e in `scripts/test_m2m_e2e.sh`.

Current e2e coverage is targeted rather than exhaustive for every JOSE policy
branch. `make test-oidc-e2e` uses the local Podman Kube OIDC/OAuth2 provider,
`curl` as the non-lonejson client, and a lonejson-backed fixture server to
exercise discovery, JWKS refresh, bearer validation, authorization-code token
exchange, refresh-token exchange, token-flow refresh, token introspection,
token revocation, and UserInfo. The same live bearer endpoint also verifies
fail-closed rejection for missing bearer credentials, wrong audience, missing
scope, wrong `azp`, and missing `azp`, plus acceptance for strict
multi-audience tokens with the expected `azp` and `scp` array scope tokens.
`make test-m2m-e2e` uses `curl` against a lonejson-backed fixture server to
exercise Basic credentials, Bearer API keys, missing/wrong credentials, signup
completion, consumed-signup rejection, and signup-generated Basic/Bearer
credentials.

Detailed JOSE policy invariants that require custom token/JWK construction,
such as fail-closed `crit`, JWK `key_ops`, and `x5c`
certificate-chain/thumbprint failure modes, are covered by C unit/regression
tests, Lua facade tests where exposed, and JWT fuzz seeds rather than by
dedicated live OIDC e2e scenarios.

The standard auth verification path for the current branch is:

```sh
make test-host
make test-oidc-e2e
make test-m2m-e2e
make fuzz-smoke
```

At the time this document was updated, those gates passed on the feature branch
containing the auth implementation. If new auth behavior is added, the proof
must move with the behavior: C tests for policy detail, Lua tests for facade
parity, fuzz seeds for untrusted inputs, and e2e when the behavior crosses an
HTTP/provider boundary.

## Known Gaps And Non-Implemented Work

The following are deliberate gaps or future work, not hidden behavior.

- No built-in HTTP transport; caller-owned providers perform network I/O.
- Token-flow state helpers retain token state, transparently refresh expired
  access tokens by default when a refresh token is available, apply bounded
  retry policy, and expose opt-out controls.
- No timeout, proxy, redirect, or TLS policy helper; those remain provider-owned.
- No built-in credential persistence, locking, or encryption helper. The M2M
  helpers generate store-ready JSON records, verify caller-owned JSON store
  bytes, and complete signup seeds into generated credentials. Applications
  still own durable storage and concurrent updates.
- M2M e2e now covers Basic credentials, Bearer API keys, missing/wrong
  credentials, signup seed completion, consumed signup rejection, and use of
  signup-generated Basic and Bearer credentials from `curl`.
- No complete browser-launch helper.
- No localhost HTTP listener.
- No random callback-path session object.
- No Kore-specific or Vectis-specific middleware API.
- No HTTP response writer for auth failures.
- No request-router integration.
- No implemented HS256 or `none` validation support. `none` remains rejected.
- No Ed448 support for `EdDSA`; the OpenSSL provider currently supports
  Ed25519 OKP keys.
- Certificate-backed JWK trust requires the OpenSSL auth provider. Non-OpenSSL
  builds parse and syntax-check `x5c`, `x5t`, and `x5t#S256`, but cannot
  validate certificate chains or thumbprints.
- `crit` JOSE header handling is fail-closed: every critical header name must be
  listed in the claim policy `accepted_crit` array.
- `azp`, `scope`, `scp`, and stricter multi-audience policy checks are available
  in `lonejson_jwt_claim_policy`.
- JWK `key_ops` is parsed and signature verification rejects keys that declare
  operations without `verify`.
- No JWKS cache eviction policy beyond one explicit cache object.
- No concurrency synchronization inside the JWKS cache object.
- No encrypted JWT/JWE support. JWE is intentionally out of the current
  OIDC/JWT/JWK completion scope.

Most of these omissions are intentional because lonejson is not meant to own
application flow control. The core value is bounded parsing, explicit
validation, structured failure classification, and facades that are easy for
framework-specific code to compose.

OAuth2 device authorization is a specific grant type for browser-constrained
devices. It is distinct from lonejson's implemented token-flow state helper,
which tracks bearer-token state for supported OAuth2/OIDC flows and decides
whether to refresh, retry, or return a clear "cannot continue" state.

The remaining encryption-specific JOSE target is JWE. It is intentionally not
part of the current OIDC/JWT/JWK completion target.

## Recommended Composition Patterns

Machine-to-machine client credentials:

1. Fetch and validate OIDC discovery metadata with
   `lonejson_oidc_fetch_discovery` and a configured HTTP provider.
2. Build a client-credentials body with
   `lonejson_oauth2_client_credentials_body`.
3. Exchange credentials with `lonejson_oauth2_client_credentials_request`, or
   POST with caller-owned HTTP code and parse with
   `lonejson_oauth2_token_response_parse_json`.
4. Treat returned token material as secret credential material.

Server-local machine-to-machine credentials:

1. Generate a credential with `lonejson_m2m_credential_generate`. The helper
   returns one-time cleartext `client_secret` and/or `api_key` values plus a
   `record_json` object containing only salts, hashes, and the caller-supplied
   opaque `claim` JSON.
2. Store `record_json` in the caller-owned credential store under
   `credentials[]`. The recommended persistence boundary is an admin CLI or
   framework-owned storage layer that controls file permissions, locking,
   encryption, and auditing.
3. In a request handler, pass the HTTP `Authorization` header and store JSON to
   `lonejson_m2m_verify_authorization`. By default it accepts both
   `Authorization: Basic <base64(client_id:client_secret)>` and
   `Authorization: Bearer <api_key>`. Set `allowed_auth_modes` to
   `LONEJSON_M2M_AUTH_BASIC` or `LONEJSON_M2M_AUTH_BEARER` to opt out of one
   mode.
4. On success, use the returned `client_id` and opaque `claim` JSON to make the
   application authorization decision. lonejson authenticates and returns
   facts; it does not decide whether a method, tool, endpoint, tenant, or
   operation is allowed.

Server-local signup seed flow:

1. An admin process calls `lonejson_m2m_signup_generate` with claim/metadata
   JSON and optionally a public signup base URL. The helper returns a one-time
   signup secret, a query string/URL suitable for email or another delivery
   channel, and a store-ready signup record.
2. Store the signup record under `signups[]`. Prefer exposing signup seeding
   through an admin CLI or trusted automation path.
3. The web handler asks the recipient for the required email address and calls
   `lonejson_m2m_signup_complete` with the signup secret, optional signup ID,
   email, and store JSON.
4. On success, show the generated API key and/or client credentials once,
   insert the returned credential record into `credentials[]`, and remove the
   consumed signup record from `signups[]`. The helper returns the signup ID to
   remove, but the caller owns the actual persistent store mutation.

CLI or desktop authorization-code with PKCE:

1. Fetch and validate OIDC discovery metadata with
   `lonejson_oidc_fetch_discovery` and a configured HTTP provider.
2. Generate PKCE with `lonejson_oidc_pkce_generate_with_runtime` on a runtime
   with an auth provider. The OpenSSL easy path is available by initializing
   that provider with `lonejson_auth_provider_init_openssl()`.
3. Build the authorization URL with `lonejson_oidc_authorization_url`.
4. Open the browser using caller-owned platform code.
5. Receive callback query through caller-owned local HTTP code.
6. Parse callback with `lonejson_oidc_authorization_callback_parse_query`.
7. Exchange the code with `lonejson_oidc_authorization_code_token_request`, or
   POST with caller-owned HTTP code and parse with
   `lonejson_oauth2_token_response_parse_json`.
8. Validate any ID token with the JWT/JWKS validation primitives.

Server-side bearer validation:

1. Fetch and validate OIDC discovery metadata with
   `lonejson_oidc_fetch_discovery`.
2. Refresh JWKS with `lonejson_oidc_jwks_cache_refresh`, or fetch with
   caller-owned HTTP code and install with `lonejson_oidc_jwks_cache_update_json`.
3. Keep the JWKS in `lonejson_oidc_jwks_cache`.
4. For each request, call `lonejson_oidc_validate_bearer_token` before
   application logic.
5. Map `lonejson_auth_failure` to framework-specific HTTP responses.
6. Refresh JWKS cache using caller-owned policy when validation fails due to
   cache availability or key miss.

## Release Notes For Consumers

Consumers that do not define the auth feature gates see no auth API and should
not need OpenSSL or curl headers.

Consumers that define `LONEJSON_WITH_JWT` must build against a library compiled
with JWT support. Signature trust APIs require an installed auth provider.

Consumers that define `LONEJSON_WITH_OIDC` must build against a library
compiled with OIDC and JWT support. Bearer validation requires an installed
auth provider. HTTP discovery, JWKS retrieval, and token endpoint POST helpers
require an installed HTTP provider.

Parsing JWTs is never trust. Trust is established only by explicit validation:

- signature validation with a selected key,
- claim validation with an explicit policy,
- or composed bearer validation against a fresh JWKS cache and claim policy.
