# Authentication integration

Enable the optional features described in [Integration](integration.md). These application patterns complement the [full auth reference](auth-implementation.md) and [provider boundary contract](third-party-integrations.md).

These are embedding fragments. Designated initializers in the examples use
C99 syntax; C89 callers can assign the same fields individually.

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

## OAuth2 client workflows

### Client credentials

1. Fetch and validate OIDC discovery metadata with
   `lonejson_oidc_fetch_discovery` and a configured HTTP provider.
2. Build a client-credentials body with
   `lonejson_oauth2_client_credentials_body`.
3. Exchange credentials with `lonejson_oauth2_client_credentials_request`, or
   POST with caller-owned HTTP code and parse with
   `lonejson_oauth2_token_response_parse_json`.
4. Treat returned token material as secret credential material.

### Authorization code with PKCE

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

## Client token state

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

## Machine-to-machine credentials

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

## Web application boundaries

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

## Lua authentication

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

## Scope

HTTP transport and TLS policy, credential persistence and locking, browser
interaction, and application authorization belong to the application. Auth
helpers materialize bounded discovery, JWKS and token responses; use the
streaming parse adapters for large general JSON responses.

See the [API reference](auth-implementation.md#limitations) for unsupported
algorithms and grant types, and [verification coverage](auth-implementation.md#verification)
for the C, Lua, fuzz and local provider tests.
