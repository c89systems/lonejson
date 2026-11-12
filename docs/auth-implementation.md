# Authentication API reference

This reference covers JWT/JWK/JWKS validation and the supported OAuth2/OIDC
flows. For compile-time features and provider setup, see [Integration](integration.md).
For HTTP callbacks, client flows, M2M stores and web handlers, see
[Authentication integration](authentication.md). The dependency and package
requirements are defined in the [integration contract](third-party-integrations.md).

Parsing a token does not establish trust. Validate its signature against a
trusted key and its claims against an explicit policy, or use the composed
bearer-validation helper. Network transport, TLS policy, credential storage
and application authorization remain caller-owned.

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

The maintained checks are:

| Layer | Coverage |
| --- | --- |
| C regression tests | `tests/test_jwt.inc.h`: parsing, trust policies, provider failures and cleanup. |
| Lua facade | `tests/test_lua.lua`: Lua-exposed workflows and policy parity. |
| Fuzzing | `fuzz/fuzz_jwt.c` and JWT seeds: untrusted token, key and discovery inputs. |
| Package/ABI | `scripts/check_jwt_abi_symbols.sh` and `tests/test_release_archive_verify.sh`: exports, feature metadata and dependency boundaries. |
| Local OIDC/OAuth2 | `make test-oidc-e2e`: discovery, JWKS, exchanges, refresh, introspection, revocation, UserInfo and bearer policy. |
| Local M2M/signup | `make test-m2m-e2e`: Basic/Bearer credentials, rejection, signup consumption and generated credentials. |

The e2e fixtures use curl as the client. Detailed JOSE branches such as `crit`,
`key_ops` and certificate-chain/thumbprint failures are covered by C/Lua/fuzz
checks rather than a separate live-provider scenario for each case. Use
`make test-host-curl` for the native C suite with all auth features enabled;
see [building and verification](development.md) for prerequisites and the
complete gate. These commands describe coverage, not a saved test result.

## Limitations

- HTTP clients/servers, browser launch, localhost callback listeners, framework
  routing, TLS/proxy/redirect policy and durable credential storage are
  application responsibilities.
- Token-flow helpers support configurable refresh and bounded retry for the
  supported flows; device authorization is not an implemented grant type.
- HS256, Ed448 and encrypted JWT/JWE are not implemented. `alg: none` is rejected.
- Certificate-backed JWK trust requires the OpenSSL provider. Other builds
  parse certificate fields but do not validate their chains or thumbprints.
- JWKS caches have no internal synchronization or eviction policy beyond the
  explicit cache object. Callers own concurrency and cache replacement.
