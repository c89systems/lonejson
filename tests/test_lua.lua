local test_temp_dir = assert(rawget(_G, "LONEJSON_TEST_TEMP_DIR") or os.getenv("LONEJSON_TEST_TEMP_DIR"), "run Lua tests through make lua-test")
local lonejson = require("lonejson")

local function new_runtime(extra)
  local config = {
    spool_default = {
      memory_limit = 32,
      temp_dir = test_temp_dir,
    },
    spool_blob = {
      memory_limit = 32,
      temp_dir = test_temp_dir,
    },
  }
  local key

  if extra ~= nil then
    for key, value in pairs(extra) do
      config[key] = value
    end
  end
  return lonejson.new(config)
end

local lj = new_runtime()

local function assert_eq(a, b, msg)
  if a ~= b then
    error((msg or "assert_eq failed") .. ": expected " .. tostring(b) .. ", got " .. tostring(a))
  end
end

local function assert_true(v, msg)
  if not v then
    error(msg or "assert_true failed")
  end
end

local function assert_rewrite_callback_failed(value, err, needle)
  assert_true(value == nil)
  assert_true(err ~= nil)
  assert_eq(err.status, "callback_failed")
  assert_true(err.message:find(needle, 1, true) ~= nil, err.message)
end

local function stable_int_text(value)
  if type(value) == "number" and value % 1 == 0 then
    return string.format("%.0f", value)
  end
  return tostring(value)
end

local function path_key(path)
  return table.concat(path, "/")
end

local function exists(path)
  local f

  if path == nil then
    return false
  end
  f = io.open(path, "rb")
  if f == nil then
    return false
  end
  f:close()
  return true
end

local function test_record_method_lookup_dispatch()
  local runtime = lonejson.new()
  local schema = runtime.schema("MethodLookupDispatch", {
    runtime.field("getaway", runtime.string { fixed_capacity = 16 }),
    runtime.field("garden", runtime.i64()),
    runtime.field("title", runtime.string { fixed_capacity = 16 }),
    runtime.field("to_tableau", runtime.bool()),
    runtime.field("counter", runtime.i64()),
    runtime.field("clearance", runtime.string { fixed_capacity = 16 }),
    runtime.field("get", runtime.string { fixed_capacity = 16 }),
    runtime.field("numbers", runtime.i64_array { fixed_capacity = 4 }),
  })
  local record = schema:new_record()
  schema:assign(record, {getaway = "g", garden = 17, title = "t",
    to_tableau = true, counter = 9, clearance = "c", get = "shadow",
    numbers = {3, 5}})
  assert(record.getaway == "g" and record.garden == 17)
  assert(record.title == "t" and record.to_tableau == true)
  assert(record.counter == 9 and record.clearance == "c")
  local ok, err = pcall(function() return record.unknown end)
  assert(not ok and err:find("unknown field", 1, true))
  assert(type(record.get) == "function" and record:get("get") == "shadow")
  assert(record:get("title") == "t" and record:count("numbers") == 2)
  local values = record:to_table()
  assert(values.getaway == "g" and values.numbers[2] == 5)
  record:clear()
  assert(record.garden == 0 and record.title == "")
  assert(record.to_tableau == false and record:count("numbers") == 0)
end
test_record_method_lookup_dispatch()

local function test_record_path_errors()
  local runtime = lonejson.new()
  local schema = runtime.schema("PathErrors", {
    runtime.field("value", runtime.i64()),
    runtime.field("numbers", runtime.i64_array { fixed_capacity = 4 }),
    runtime.field("child", runtime.object { fields = {
      runtime.field("value", runtime.i64()),
    } }),
  })
  local record = schema:new_record()
  schema:assign(record, {value = 7, numbers = {3}, child = {value = 11}})
  local cases = {
    {string.rep("x", 128), "path segment too long"},
    {".value", "malformed record path"},
    {"unknown", "unknown field"},
    {"child.unknown", "unknown field"},
    {"numbers[1", "malformed array index"},
    {"numbers[1].value", "not an object array"},
    {"value.child", "not an object"},
    {"numbers[1]extra", "malformed record path"},
    {"value[1]", "not an array"},
  }
  for _, case in ipairs(cases) do
    for _ = 1, 2 do
      local ok, err = pcall(function() schema:compile_path(case[1]) end)
      assert(not ok and err:find(case[2], 1, true), tostring(err))
      ok, err = pcall(function() return record:get(case[1]) end)
      assert(not ok and err:find(case[2], 1, true), tostring(err))
    end
  end
  assert(record:get("child.value") == 11 and record.value == 7)
end
test_record_path_errors()

assert_eq(lonejson.base64_encode("Hello"), "SGVsbG8=")
assert_eq(lonejson.base64_encode("\255", "url_raw"), "_w")
assert_eq(lonejson.base64_decode("SGVsbG8="), "Hello")
assert_eq(lj:base64_decode("_w", "jwt"), "\255")
assert_true(lonejson.monotonic_ns == nil)
do
  local bad_base64, bad_base64_err = lonejson.base64_decode("AA=", "url_raw")
  assert_true(bad_base64 == nil)
  assert_eq(bad_base64_err.status, "invalid_json")
end

local function build_test_schema(runtime, name)
  return runtime.schema(name, {
  lj.field("name", lj.string { required = true }),
  lj.field("age", lj.i64()),
  lj.field("active", lj.bool()),
  lj.field("body", lj.spooled_text()),
  lj.field("payload", lj.spooled_bytes { class = "blob" }),
  lj.field("meta", lj.object {
    fields = {
      lj.field("city", lj.string()),
      lj.field("zip", lj.i64()),
    },
  }),
  lj.field("nums", lj.i64_array { fixed_capacity = 8, overflow = "fail" }),
})
end

local function build_merge_schema(runtime, name)
  return runtime.schema(name, {
    lj.field("id", lj.string()),
    lj.field("age", lj.i64()),
    lj.field("selector", lj.json_value()),
    lj.field("fields", lj.json_value()),
  })
end

local function build_nested_query_schema(runtime, name)
  return runtime.schema(name, {
    lj.field("type", lj.string { required = true }),
    lj.field("response", lj.object {
      required = true,
      fields = {
        lj.field("status", lj.string()),
        lj.field("selector", lj.json_value { required = true }),
        lj.field("fields", lj.json_value()),
      },
    }),
  })
end

local function build_batch_schema(runtime, name)
  return runtime.schema(name, {
    lj.field("batch_id", lj.string()),
    lj.field("items", lj.object_array {
      fields = {
        lj.field("id", lj.string { required = true }),
        lj.field("payload", lj.json_value { required = true }),
        lj.field("tags", lj.json_value()),
      },
    }),
  })
end

local function build_fixed_large_schema(runtime, name)
  return runtime.schema(name, {
    lj.field("payload", lj.string { required = true, fixed_capacity = 8192, overflow = "fail" }),
  })
end

local fixed_string_scratch = lonejson.fixed_string_scratch(8192)
local merge_lj = new_runtime({ clear_destination_by_default = false })
local allow_dup_lj = new_runtime({ reject_duplicate_keys_by_default = false })
local pretty_lj = new_runtime({ write_pretty = true })
local pretty_limited_output_lj = new_runtime({
  write_pretty = true,
  write_max_output_bytes = 32,
})
local pretty_limited_json_value_lj = new_runtime({
  write_pretty = true,
  json_value_max_string_bytes = 1,
})
local limited_output_lj = new_runtime({ write_max_output_bytes = 32 })
local default_cap_lj = new_runtime({ write_max_output_bytes = 0 })
local fixed_large_budget_lj = new_runtime({
  clear_destination_by_default = false,
  max_alloc_bytes = 1,
})
local fixed_large_scratch_lj = new_runtime({
  clear_destination_by_default = false,
  max_alloc_bytes = 1,
  fixed_string_scratch = fixed_string_scratch,
})
local limited_json_value_lj = new_runtime({
  clear_destination_by_default = false,
  json_value_max_string_bytes = 1,
})

do
  local colon_lj = new_runtime()
  local sink_chunks = {}
  local schema = colon_lj:schema("ColonSchema", {
    lj.field("name", lj.string { required = true }),
  })

  assert_true(schema ~= nil)
  assert_eq(colon_lj:encode_json({ alpha = 1 }), '{"alpha":1}')
  colon_lj:encode_json_to_sink({ beta = true }, function(chunk)
    sink_chunks[#sink_chunks + 1] = chunk
    return true
  end)
  assert_eq(table.concat(sink_chunks), '{"beta":true}')
  assert_eq(colon_lj:decode_json('{"gamma":2}').gamma, 2)
  assert_eq(schema:decode('{"name":"colon"}').name, "colon")
end

if lonejson.jwt_parse_compact ~= nil then
  local jwt_token =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0." ..
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJuYmYiOjkwMCwiaWF0IjoxMDAwfQ." ..
      "c2ln"
  local aud_array_token =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0." ..
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOlsiYXBpIiwib3RoZXIiXSwiZXhwIjoyMDAwfQ." ..
      "c2ln"
  local nonce_token =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0." ..
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJu" ..
      "b25jZSI6Im5vbmNlLTQ1NiJ9." ..
      "c2ln"
  local jose_policy_token =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIiwiY3JpdCI6WyJleHAt" ..
      "dGVzdCJdLCJ4NXQiOiJBQSIsIng1dCNTMjU2IjoiQUEiLCJ4NWMiOlsiQUE9PSJdfQ." ..
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjpbImFwaSIsImNsaWVudCJdLCJh" ..
      "enAiOiJjbGllbnQiLCJzY29wZSI6InJlYWQgd3JpdGUiLCJzY3AiOlsiYWRtaW4iLCJi" ..
      "aWxsaW5nLnJlYWQiXSwiZXhwIjoyMDAwfQ." ..
      "c2ln"
  local policy = {
    accepted_algs = { "RS256" },
    accepted_issuers = { "issuer" },
    accepted_audiences = { "api" },
    required_claims = { "iss", "aud", "exp" },
    now = 1000,
    allowed_clock_skew = 30,
  }
  local parts = lonejson.jwt_parse_compact(jwt_token)
  local decoded = lj:jwt_decode_compact(jwt_token)
  local validated = lj:jwt_validate_compact_claims(jwt_token, policy)
  local aud_array = lonejson.jwt_validate_compact_claims(aud_array_token, policy)
  local nonce_policy = {
    accepted_algs = { "RS256" },
    accepted_issuers = { "issuer" },
    accepted_audiences = { "api" },
    required_claims = { "iss", "aud", "exp", "nonce" },
    expected_nonce = "nonce-456",
    now = 1000,
  }
  local nonce_validated = lj:jwt_validate_compact_claims(nonce_token, nonce_policy)
  local jose_policy = {
    accepted_algs = { "RS256" },
    accepted_issuers = { "issuer" },
    accepted_audiences = { "api", "client" },
    accepted_crit = { "exp-test" },
    required_scopes = { "read", "admin" },
    expected_azp = "client",
    require_azp_when_multiple_audiences = true,
    require_all_audiences_accepted = true,
    now = 1000,
  }
  local jose_validated = lj:jwt_validate_compact_claims(jose_policy_token, jose_policy)
  local jwk = lonejson.jwk_parse_json(
      '{"kty":"RSA","kid":"rsa1","use":"sig","alg":"RS256","key_ops":["verify"],' ..
      '"x5t":"AA","x5t#S256":"AA","x5c":["AA=="],"n":"AQIDBA","e":"AQAB"}')
  local signed_token =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6InJzYS10ZXN0IiwidHlwIjoiSldUIn0." ..
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJuYmYiOjkwMCwiaWF0IjoxMDAwfQ." ..
      "PoouvDAoloqvsfTQxadOTpQGXKyeHq0lx6WQEPv0qvg59KMuy8lD-XBTBPCF_MpQGoe3DS84CSg27iktG7z12Qv6TX1gqbJUO2wkwhuW4dIWFPY9tDhI3e05W5yz8D70wARx7CL9tHKWsOpLwHRWf5ugfrq1PuofcC9atB7D-QUfrmmJ01NXbQl4aq6DJ02M7azHTLq-15X3TuE2CHN5P_zo_zkaJT8V0QhoQ3MUhUE_pBxMtAByRIUOEW32RbWjYgkwZ_zxaVkbhXv1CYznQCzikX2wXn9OQ_1z0TCH7bT5Ao3EXEQeiK7Fhuq8lyPFbkhDc_yCjxFRjm7ufSFZbg"
  local signed_jwk_json =
      '{"kty":"RSA","kid":"rsa-test","use":"sig","alg":"RS256",' ..
      '"n":"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4GcspGtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1OgnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWyYoQ",' ..
      '"e":"AQAB"}'
  local signed_jwks_json = '{"keys":[' .. signed_jwk_json .. ']}'
  local jwks_json =
      '{"keys":[{"kty":"RSA","kid":"rsa1","use":"sig","alg":"RS256","n":"AQIDBA","e":"AQAB"},' ..
      '{"kty":"EC","kid":"ec1","use":"sig","alg":"ES256","crv":"P-256","x":"AAEC","y":"AwQF"}]}'
  local jwks = lj:jwks_parse_json(jwks_json)
  local selected = lonejson.jwks_select_json(jwks_json, { kid = "ec1", kty = "EC", use = "sig" })
  local missing = lonejson.jwks_select_json(jwks_json, { kid = "missing" })
  local bad, err

  if lj.set_openssl_auth_provider ~= nil then
    local no_provider_lj = new_runtime()
    bad, err = no_provider_lj:jwt_validate_compact_signature(signed_token, signed_jwk_json)
    assert_true(bad == nil)
    assert_eq(err.status, "type_mismatch")
    assert_true(lj:set_openssl_auth_provider())
  end

  assert_eq(parts.signature, "c2ln")
  assert_eq(parts.signing_input, parts.header .. "." .. parts.payload)
  assert_eq(decoded.header.alg, "RS256")
  assert_eq(decoded.header.kid, "k1")
  assert_eq(decoded.claims.iss, "issuer")
  assert_eq(decoded.claims.sub, "s")
  assert_eq(decoded.claims.aud, "api")
  assert_eq(decoded.claims.exp, 2000)
  assert_eq(validated.claims.aud, "api")
  assert_eq(aud_array.claims.aud[1], "api")
  assert_eq(aud_array.claims.aud[2], "other")
  assert_eq(nonce_validated.claims.nonce, "nonce-456")
  assert_eq(jwk.kid, "rsa1")
  assert_eq(jwk.key_ops[1], "verify")
  assert_eq(jwk["x5t#S256"], "AA")
  assert_eq(jwk.x5c[1], "AA==")
  assert_eq(jwk.n, "AQIDBA")
  assert_eq(jose_validated.header.crit[1], "exp-test")
  assert_eq(jose_validated.header.x5t, "AA")
  assert_eq(jose_validated.claims.azp, "client")
  assert_eq(jose_validated.claims.scope, "read write")
  assert_eq(jose_validated.claims.scp[1], "admin")
  assert_true(lj:jwt_validate_compact_signature(signed_token, signed_jwk_json))
  assert_eq(#jwks.keys, 2)
  assert_eq(jwks.keys[2].kid, "ec1")
  assert_eq(selected.kid, "ec1")
  assert_true(missing == nil)

  if lonejson.oidc_discovery_url ~= nil then
    local discovery_json =
        '{"issuer":"https://id.example/tenant",' ..
        '"authorization_endpoint":"https://id.example/auth",' ..
        '"token_endpoint":"https://id.example/token",' ..
        '"jwks_uri":"https://id.example/jwks",' ..
        '"introspection_endpoint":"https://id.example/introspect",' ..
        '"revocation_endpoint":"https://id.example/revoke",' ..
        '"userinfo_endpoint":"https://id.example/userinfo"}'
    local discovery = lj:oidc_discovery_parse_json(discovery_json, "https://id.example/tenant")
    local cache_policy = {
      issuer = "https://id.example/tenant",
      jwks_uri = "https://id.example/jwks",
      now = 1000,
      ttl_seconds = 60,
      max_jwks_bytes = 4096,
    }
    local cache_selected = lj:oidc_jwks_cache_select_json(
        jwks_json, cache_policy, { kid = "rsa1", kty = "RSA", alg = "RS256", use = "sig" })
    local cache_missing = lonejson.oidc_jwks_cache_select_json(
        jwks_json, cache_policy, { kid = "missing" })
    local runtime_only_auth_methods = {
      "jwt_parse_compact",
      "jwt_decode_compact",
      "jwt_validate_compact_claims",
      "jwt_validate_compact_signature",
      "jwk_parse_json",
      "jwks_parse_json",
      "jwks_select_json",
      "oauth2_client_credentials_body",
      "oauth2_refresh_token_body",
      "oauth2_token_introspection_body",
      "oauth2_token_revocation_body",
      "oidc_authorization_code_token_body",
      "oauth2_client_credentials_request",
      "oauth2_refresh_token_request",
      "oauth2_token_flow_ensure",
      "oauth2_introspect_token_request",
      "oauth2_revoke_token_request",
      "oidc_userinfo_request",
      "oidc_authorization_code_token_request",
      "oauth2_token_response_parse_json",
      "oauth2_introspection_response_parse_json",
      "oidc_userinfo_response_parse_json",
      "oidc_validate_bearer_token",
      "oidc_discovery_parse_json",
      "oidc_jwks_cache_select_json",
    }
    local free_only_auth_helpers = {
      "oauth2_token_flow_update_response",
      "oauth2_token_flow_is_expired",
      "oidc_pkce_challenge",
      "oidc_pkce_generate",
      "oidc_authorization_url",
      "oidc_authorization_callback_parse_query",
      "oidc_discovery_url",
    }
    local raw_lj = assert(lonejson.core.new())
    for _, name in ipairs(runtime_only_auth_methods) do
      if lonejson[name] ~= nil then
        assert_eq(type(lj[name]), "function")
        assert_eq(type(raw_lj[name]), "function")
      end
    end
    for _, name in ipairs(free_only_auth_helpers) do
      assert_eq(type(lonejson[name]), "function")
      assert_eq(lj[name], nil)
      assert_eq(raw_lj[name], nil)
    end
    local token_body = lj:oauth2_client_credentials_body({
      client_id = "client id",
      client_secret = "s+e&c=r%t",
      scope = "read write",
      audience = "https://api.example/a?b=c",
      resource = "urn:example:resource",
    })
    local refresh_body = lj:oauth2_refresh_token_body({
      refresh_token = "r+e&f",
      client_id = "client id",
      client_secret = "secret",
      scope = "read write",
    })
    local introspection_body = lonejson.oauth2_token_introspection_body({
      token = "a+b&c",
      token_type_hint = "access_token",
      client_id = "client id",
      client_secret = "secret",
    })
    local introspection_basic_body = lonejson.oauth2_token_introspection_body({
      token = "access",
      token_type_hint = "access_token",
      client_id = "client",
      client_secret = "secret",
      use_basic_auth = true,
    })
    local revocation_body = lj:oauth2_token_revocation_body({
      token = "refresh",
      token_type_hint = "refresh_token",
      client_id = "client",
      client_secret = "secret",
    })
    local code_body = lonejson.oidc_authorization_code_token_body({
      client_id = "client id",
      code = "code+123",
      redirect_uri = "http://127.0.0.1:1234/cb",
      code_verifier = "verifier value",
      client_secret = "secret",
    })
    local token_response = lonejson.oauth2_token_response_parse_json(
        '{"access_token":"token","token_type":"Bearer","expires_in":3600,"scope":"read write"}')
    local token_flow = lonejson.oauth2_token_flow_update_response({
      refresh_token = "old-refresh",
    }, token_response, 1000)
    local token_flow_expired = lonejson.oauth2_token_flow_is_expired(
        token_flow, 4550, 60)
    local introspection_response = lj:oauth2_introspection_response_parse_json(
        '{"active":true,"scope":"read write","client_id":"client","sub":"sub","aud":"api","exp":123}')
    local userinfo_response = lonejson.oidc_userinfo_response_parse_json(
        '{"sub":"sub","email":"user@example.com","email_verified":true,"extra":1}')
    local pkce_challenge = lonejson.oidc_pkce_challenge(
        "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk")
    local pkce = lonejson.oidc_pkce_generate()
    local auth_url = lonejson.oidc_authorization_url({
      authorization_endpoint = "https://id.example/auth",
      client_id = "client id",
      redirect_uri = "http://127.0.0.1:1234/cb",
      scope = "openid profile",
      state = "state-123",
      nonce = "nonce-456",
      code_challenge = "challenge",
    })
    local callback = lonejson.oidc_authorization_callback_parse_query(
        "?code=abc%2B123&state=state+123", "state 123")
    local bearer = lj:oidc_validate_bearer_token(
        "Bearer " .. signed_token, signed_jwks_json, cache_policy, policy)
    local http_requests = {}
    local provider_lj = new_runtime()
    local provider_ok = provider_lj:set_http_provider(function(request)
      http_requests[#http_requests + 1] = request
      assert_eq(request.user_agent, "lonejson-lua-test/1")
      if request.url == "https://id.example/tenant/.well-known/openid-configuration" then
        assert_eq(request.method, "GET")
        return { status_code = 200, body = discovery_json }
      end
      if request.url == "https://id.example/jwks" then
        assert_eq(request.method, "GET")
        return { status_code = 200, body = jwks_json }
      end
      if request.url == "https://id.example/token" then
        assert_eq(request.method, "POST")
        assert_eq(request.content_type, "application/x-www-form-urlencoded")
        if request.body:find("grant_type=client_credentials", 1, true) ~= nil then
          return { status_code = 200, body = '{"access_token":"client-token","token_type":"Bearer","expires_in":60}' }
        end
        if request.body:find("grant_type=refresh_token", 1, true) ~= nil then
          return { status_code = 200, body = '{"access_token":"refresh-token","token_type":"Bearer"}' }
        end
        if request.body:find("grant_type=authorization_code", 1, true) ~= nil then
          return { status_code = 200, body = '{"access_token":"code-token","token_type":"Bearer","id_token":"id.jwt"}' }
        end
      end
      if request.url == "https://id.example/introspect" then
        assert_eq(request.method, "POST")
        assert_eq(request.content_type, "application/x-www-form-urlencoded")
        assert_true(request.body:find("token=access", 1, true) ~= nil)
        if request.authorization ~= nil then
          assert_eq(request.authorization, "Basic Y2xpZW50OnNlY3JldA==")
          assert_true(request.body:find("client_secret=", 1, true) == nil)
        end
        return { status_code = 200, body = '{"active":true,"scope":"read","client_id":"client","sub":"subject"}' }
      end
      if request.url == "https://id.example/revoke" then
        assert_eq(request.method, "POST")
        assert_eq(request.content_type, "application/x-www-form-urlencoded")
        assert_true(request.body:find("token=refresh", 1, true) ~= nil)
        if request.authorization ~= nil then
          assert_eq(request.authorization, "Basic Y2xpZW50OnNlY3JldA==")
          assert_true(request.body:find("client_secret=", 1, true) == nil)
        end
        return { status_code = 200, body = "{}" }
      end
      if request.url == "https://id.example/userinfo" then
        assert_eq(request.method, "GET")
        assert_eq(request.authorization, "Bearer access")
        return { status_code = 200, body = '{"sub":"subject","email":"subject@example.com","email_verified":true}' }
      end
      return nil, "unexpected request " .. tostring(request.url)
    end, "lonejson-lua-test/1")
    local fetched_discovery = provider_lj:oidc_fetch_discovery("https://id.example/tenant", 4096)
    local refreshed_cache = provider_lj:oidc_jwks_cache_refresh(cache_policy)
    local requested_client_token = provider_lj:oauth2_client_credentials_request(
        "https://id.example/token", {
          client_id = "client id",
          client_secret = "s+e&c=r%t",
          scope = "read write",
        }, 4096)
    local requested_refresh_token = provider_lj:oauth2_refresh_token_request(
        "https://id.example/token", {
          refresh_token = "r+e&f",
          client_id = "client id",
          client_secret = "secret",
        }, 4096)
    local ensured_flow, ensured_flow_result = provider_lj:oauth2_token_flow_ensure({
      access_token = "old-token",
      refresh_token = "r+e&f",
      expires_at = 1000,
    }, {
      token_endpoint = "https://id.example/token",
      client_id = "client id",
      client_secret = "secret",
      now = 1001,
      max_response_bytes = 4096,
      disable_retry = true,
    })
    local bad_ensured_flow, bad_ensured_flow_err =
        provider_lj:oauth2_token_flow_ensure({
          access_token = "old-token",
          refresh_token = "r+e&f",
          expires_at = 1000,
        }, {
          client_id = "client id",
          now = 1001,
        })
    local requested_introspection = provider_lj:oauth2_introspect_token_request(
        "https://id.example/introspect", {
          token = "access",
          token_type_hint = "access_token",
          client_id = "client",
          client_secret = "secret",
          use_basic_auth = true,
        }, 4096)
    local revoked = provider_lj:oauth2_revoke_token_request(
        "https://id.example/revoke", {
          token = "refresh",
          token_type_hint = "refresh_token",
          client_id = "client",
          client_secret = "secret",
          use_basic_auth = true,
        })
    local requested_userinfo = provider_lj:oidc_userinfo_request(
        "https://id.example/userinfo", {
          access_token = "access",
        })
    local requested_code_token = provider_lj:oidc_authorization_code_token_request(
        "https://id.example/token", {
          client_id = "client id",
          code = "code+123",
          redirect_uri = "http://127.0.0.1:1234/cb",
          code_verifier = "verifier value",
        }, 4096)

    assert_eq(lonejson.oidc_discovery_url("https://id.example/tenant/"),
              "https://id.example/tenant/.well-known/openid-configuration")
    assert_eq(discovery.issuer, "https://id.example/tenant")
    assert_eq(discovery.token_endpoint, "https://id.example/token")
    assert_eq(discovery.jwks_uri, "https://id.example/jwks")
    assert_eq(discovery.introspection_endpoint, "https://id.example/introspect")
    assert_eq(discovery.revocation_endpoint, "https://id.example/revoke")
    assert_eq(discovery.userinfo_endpoint, "https://id.example/userinfo")
    assert_eq(cache_selected.kid, "rsa1")
    assert_true(cache_missing == nil)
    assert_eq(token_body,
              "grant_type=client_credentials&client_id=client+id&" ..
              "client_secret=s%2Be%26c%3Dr%25t&scope=read+write&" ..
              "audience=https%3A%2F%2Fapi.example%2Fa%3Fb%3Dc&" ..
              "resource=urn%3Aexample%3Aresource")
    assert_eq(refresh_body,
              "grant_type=refresh_token&refresh_token=r%2Be%26f&" ..
              "client_id=client+id&client_secret=secret&scope=read+write")
    assert_eq(introspection_body,
              "token=a%2Bb%26c&token_type_hint=access_token&" ..
              "client_id=client+id&client_secret=secret")
    assert_eq(introspection_basic_body,
              "token=access&token_type_hint=access_token")
    assert_eq(revocation_body,
              "token=refresh&token_type_hint=refresh_token&" ..
              "client_id=client&client_secret=secret")
    assert_eq(code_body,
              "grant_type=authorization_code&client_id=client+id&" ..
              "code=code%2B123&" ..
              "redirect_uri=http%3A%2F%2F127.0.0.1%3A1234%2Fcb&" ..
              "code_verifier=verifier+value&client_secret=secret")
    assert_eq(token_response.access_token, "token")
    assert_eq(token_response.token_type, "Bearer")
    assert_eq(token_response.expires_in, 3600)
    assert_eq(token_flow.access_token, "token")
    assert_eq(token_flow.refresh_token, "old-refresh")
    assert_eq(token_flow.expires_at, 4600)
    assert_true(token_flow_expired)
    assert_true(introspection_response.active)
    assert_eq(introspection_response.scope, "read write")
    assert_eq(introspection_response.client_id, "client")
    assert_eq(introspection_response.sub, "sub")
    assert_eq(introspection_response.aud, "api")
    assert_eq(introspection_response.exp, 123)
    assert_eq(userinfo_response.sub, "sub")
    assert_eq(userinfo_response.email, "user@example.com")
    assert_true(userinfo_response.email_verified)
    assert_true(userinfo_response.json:find('"extra":1', 1, true) ~= nil)
    assert_eq(pkce_challenge, "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM")
    assert_eq(#pkce.code_verifier, 43)
    assert_eq(#pkce.code_challenge, 43)
    assert_eq(auth_url,
              "https://id.example/auth?response_type=code&" ..
              "client_id=client+id&" ..
              "redirect_uri=http%3A%2F%2F127.0.0.1%3A1234%2Fcb&" ..
              "scope=openid+profile&state=state-123&nonce=nonce-456&" ..
              "code_challenge=challenge&code_challenge_method=S256")
    assert_eq(callback.code, "abc+123")
    assert_eq(callback.state, "state 123")
    assert_true(bearer.authorized)
    assert_eq(bearer.failure, "none")
    assert_eq(bearer.header.kid, "rsa-test")
    assert_eq(bearer.claims.iss, "issuer")
    assert_eq(bearer.jwk.kid, "rsa-test")
    assert_true(provider_ok)
    assert_eq(fetched_discovery.issuer, "https://id.example/tenant")
    assert_eq(fetched_discovery.jwks_uri, "https://id.example/jwks")
    assert_eq(refreshed_cache.issuer, "https://id.example/tenant")
    assert_eq(refreshed_cache.jwks_uri, "https://id.example/jwks")
    assert_true(refreshed_cache.fetched_at == 1000)
    assert_true(refreshed_cache.expires_at == 1060)
    assert_eq(refreshed_cache.jwks.keys[1].kid, "rsa1")
    assert_eq(requested_client_token.access_token, "client-token")
    assert_eq(requested_client_token.expires_in, 60)
    assert_eq(requested_refresh_token.access_token, "refresh-token")
    assert_eq(ensured_flow.access_token, "refresh-token")
    assert_eq(ensured_flow.refresh_token, "r+e&f")
    assert_eq(ensured_flow_result.state, "refreshed")
    assert_true(ensured_flow_result.refreshed)
    assert_eq(ensured_flow_result.attempts, 1)
    assert_true(bad_ensured_flow == nil)
    assert_eq(bad_ensured_flow_err.status, "invalid_argument")
    assert_true(requested_introspection.active)
    assert_eq(requested_introspection.sub, "subject")
    assert_true(revoked)
    assert_eq(requested_userinfo.sub, "subject")
    assert_eq(requested_userinfo.email, "subject@example.com")
    assert_eq(requested_code_token.access_token, "code-token")
    assert_eq(requested_code_token.id_token, "id.jwt")
    assert_eq(#http_requests, 9)

    bad, err = lonejson.oidc_discovery_parse_json(discovery_json, "https://id.example")
    assert_true(bad == nil)
    assert_eq(err.status, "type_mismatch")

    bad, err = lonejson.oauth2_client_credentials_body({
      client_id = "client",
      client_secret = "secret",
      max_body_bytes = 8,
    })
    assert_true(bad == nil)
    assert_eq(err.status, "overflow")

    bad, err = lonejson.oauth2_refresh_token_body({
      refresh_token = "refresh",
      client_secret = "secret",
    })
    assert_true(bad == nil)
    assert_eq(err.status, "invalid_argument")

    bad, err = lonejson.oauth2_token_introspection_body({
      token = "access",
      client_secret = "secret",
    })
    assert_true(bad == nil)
    assert_eq(err.status, "invalid_argument")

    bad, err = lonejson.oauth2_token_revocation_body({
      token = "refresh",
      max_body_bytes = 8,
    })
    assert_true(bad == nil)
    assert_eq(err.status, "overflow")

    bad, err = lj:oidc_authorization_code_token_body({
      client_id = "client",
      code = "code",
      redirect_uri = "http://127.0.0.1/cb",
    })
    assert_true(bad == nil)
    assert_eq(err.status, "invalid_argument")

    bad, err = lj:oauth2_token_response_parse_json(
        '{"access_token":"token","token_type":"mac"}')
    assert_true(bad == nil)
    assert_eq(err.status, "type_mismatch")

    bad, err = lj:oauth2_token_response_parse_json('{"error":"invalid_client"}')
    assert_true(bad == nil)
    assert_eq(err.status, "type_mismatch")

    bad, err = lj:oauth2_introspection_response_parse_json('{"scope":"read"}')
    assert_true(bad == nil)
    assert_eq(err.status, "invalid_json")

    bad, err = lj:oidc_userinfo_response_parse_json('{"email_verified":"yes"}')
    assert_true(bad == nil)
    assert_eq(err.status, "type_mismatch")

    bad, err = provider_lj:oauth2_client_credentials_request(
        "https://id.example/token", {
          client_id = "client id",
          client_secret = "s+e&c=r%t",
        }, 8)
    assert_true(bad == nil)
    assert_eq(err.status, "overflow")

    provider_lj:set_http_provider(function()
      return nil, "provider boom"
    end, "lonejson-lua-test/2")
    bad, err = provider_lj:oidc_fetch_discovery("https://id.example/tenant", 4096)
    assert_true(bad == nil)
    assert_eq(err.status, "callback_failed")
    assert_true(err.message:find("provider boom", 1, true) ~= nil)

    provider_lj:set_http_provider(nil)
    bad, err = provider_lj:oidc_fetch_discovery("https://id.example/tenant", 4096)
    assert_true(bad == nil)
    assert_eq(err.status, "type_mismatch")

    bad, err = lonejson.oidc_pkce_challenge("too-short")
    assert_true(bad == nil)
    assert_eq(err.status, "invalid_argument")

    bad, err = lonejson.oidc_authorization_callback_parse_query(
        "code=abc&state=wrong", "state")
    assert_true(bad == nil)
    assert_eq(err.status, "type_mismatch")
    bad, err = lonejson.oidc_authorization_callback_parse_query(
        "code=abc&state=state%00extra", "state")
    assert_true(bad == nil)
    assert_eq(err.status, "invalid_json")
    bad, err = lonejson.oidc_authorization_callback_parse_query(
        "error=access_denied&error=server_error&state=state", "state")
    assert_true(bad == nil)
    assert_eq(err.status, "duplicate_field")

    bad, err = lj:oidc_jwks_cache_select_json(jwks_json, {
      issuer = "https://id.example/tenant",
      jwks_uri = "https://id.example/jwks",
      now = 1000,
      ttl_seconds = 0,
    }, { kid = "rsa1" })
    assert_true(bad == nil)
    assert_eq(err.status, "invalid_argument")

    bad, err = lj:oidc_jwks_cache_select_json(jwks_json, {
      issuer = "https://id.example/tenant",
      jwks_uri = "https://id.example/jwks",
      now = 1000,
      ttl_seconds = 60,
      max_jwks_bytes = 4,
    }, { kid = "rsa1" })
    assert_true(bad == nil)
    assert_eq(err.status, "overflow")

    local failure
    bad, err, failure = lj:oidc_validate_bearer_token(
        "Bearer not-a-jwt", signed_jwks_json, cache_policy, policy)
    assert_true(bad == nil)
    assert_eq(err.status, "invalid_json")
    assert_eq(failure, "malformed_token")

    bad, err, failure = lj:oidc_validate_bearer_token(
        "Bearer " .. signed_token, signed_jwks_json, cache_policy, {
          accepted_algs = { "RS256" },
          accepted_issuers = { "other" },
          accepted_audiences = { "api" },
          required_claims = { "iss", "aud", "exp" },
          now = 1000,
        })
    assert_true(bad == nil)
    assert_eq(err.status, "type_mismatch")
    assert_eq(failure, "issuer_mismatch")

    if lj.set_openssl_auth_provider ~= nil and lj.m2m_credential_generate ~= nil then
      local api_credential = lj:m2m_credential_generate({
        claim = { scope = { "read" }, tenant = "acme" },
        auth_modes = "bearer",
      })
      local api_store = '{"credentials":[' .. api_credential.record_json .. ']}'
      local api_auth = lj:m2m_verify_authorization({
        store_json = api_store,
        authorization_header = "Bearer " .. api_credential.api_key,
        allowed_auth_modes = "bearer",
      })
      local wrong_api, wrong_api_err, wrong_api_failure =
          lj:m2m_verify_authorization({
            store_json = api_store,
            authorization_header = "Bearer wrong-api-key",
            allowed_auth_modes = "bearer",
          })
      local missing_api_ok, missing_api, missing_api_err, missing_api_failure =
          pcall(function()
            return lj:m2m_verify_authorization({
              store_json = api_store,
              allowed_auth_modes = "bearer",
            })
          end)
      local malformed_api, malformed_api_err, malformed_api_failure =
          lj:m2m_verify_authorization({
            store_json = api_store,
            authorization_header = "Basic !!!",
          })
      local malformed_store, malformed_store_err, malformed_store_failure =
          lj:m2m_verify_authorization({
            store_json = '{"credentials":{}}',
            authorization_header = "Bearer wrong-api-key",
          })
      local signup = lj:m2m_signup_generate({
        base_url = "https://app.example/signup",
        claim = { scope = { "write" }, plan = "trial" },
      })
      local signup_store = '{"signups":[' .. signup.record_json .. ']}'
      local complete = lj:m2m_signup_complete({
        store_json = signup_store,
        signup_id = signup.signup_id,
        signup_secret = signup.signup_secret,
        email = "user@example.com",
        credential_auth_modes = "bearer",
      })
      local completed_store =
          '{"credentials":[' .. complete.credential.record_json .. ']}'
      local completed_auth = lj:m2m_verify_authorization({
        store_json = completed_store,
        authorization_header = "Bearer " .. complete.credential.api_key,
        allowed_auth_modes = { api_key = true },
      })
      local bad_signup, bad_signup_err = lj:m2m_signup_complete({
        store_json = signup_store,
        signup_id = signup.signup_id,
        signup_secret = "wrong-secret",
        email = "user@example.com",
        credential_auth_modes = "bearer",
      })
      local basic_credential = lj:m2m_credential_generate({
        claim_json = '{"scope":["admin"],"tenant":"ops"}',
        auth_modes = { "basic" },
      })
      local basic_store = '{"credentials":[' .. basic_credential.record_json .. ']}'
      local basic_auth = lj:m2m_verify_authorization({
        store_json = basic_store,
        authorization_header = "Basic " ..
            lonejson.base64_encode(
                basic_credential.client_id .. ":" ..
                basic_credential.client_secret),
        allowed_auth_modes = { basic = true },
      })

      assert_eq(api_credential.client_secret, nil)
      assert_true(api_credential.api_key ~= nil)
      assert_true(api_credential.record_json:find(api_credential.api_key, 1, true) == nil)
      assert_true(api_auth.authorized)
      assert_eq(api_auth.auth_mode, "bearer")
      assert_eq(api_auth.client_id, api_credential.client_id)
      assert_eq(api_auth.claim.tenant, "acme")
      assert_eq(api_auth.claim.scope[1], "read")
      assert_true(wrong_api == nil)
      assert_eq(wrong_api_err.status, "type_mismatch")
      assert_eq(wrong_api_failure, "invalid_signature")
      assert_true(missing_api_ok)
      assert_true(missing_api == nil)
      assert_eq(missing_api_err.status, "type_mismatch")
      assert_eq(missing_api_failure, "missing_credentials")
      assert_true(malformed_api == nil)
      assert_eq(malformed_api_err.status, "type_mismatch")
      assert_eq(malformed_api_failure, "invalid_signature")
      assert_true(malformed_store == nil)
      assert_eq(malformed_store_err.status, "type_mismatch")
      assert_eq(malformed_store_failure, "cache_unavailable")
      assert_true(signup.signup_id ~= nil)
      assert_true(signup.signup_secret ~= nil)
      assert_true(signup.url:find("signup_id=", 1, true) ~= nil)
      assert_true(signup.url:find("signup_secret=", 1, true) ~= nil)
      assert_true(signup.record_json:find(signup.signup_secret, 1, true) == nil)
      assert_eq(complete.signup_id, signup.signup_id)
      assert_eq(complete.email, "user@example.com")
      assert_eq(complete.credential.client_secret, nil)
      assert_true(complete.credential.api_key ~= nil)
      assert_true(completed_auth.authorized)
      assert_eq(completed_auth.claim.plan, "trial")
      assert_true(bad_signup == nil)
      assert_eq(bad_signup_err.status, "type_mismatch")
      assert_true(basic_credential.client_secret ~= nil)
      assert_eq(basic_credential.api_key, nil)
      assert_true(basic_auth.authorized)
      assert_eq(basic_auth.auth_mode, "basic")
      assert_eq(basic_auth.claim.tenant, "ops")
      assert_eq(basic_auth.claim.scope[1], "admin")
    end
  end

  bad, err = lj:jwt_validate_compact_claims(jwt_token, {
    accepted_algs = { "none" },
    accepted_issuers = { "issuer" },
    accepted_audiences = { "api" },
    now = 1000,
  })
  assert_true(bad == nil)
  assert_eq(err.status, "type_mismatch")

  nonce_policy.expected_nonce = "other"
  bad, err = lj:jwt_validate_compact_claims(nonce_token, nonce_policy)
  assert_true(bad == nil)
  assert_eq(err.status, "type_mismatch")

  bad, err = lj:jwt_validate_compact_claims(jwt_token, {
    accepted_algs = { "RS256" },
    accepted_issuers = { "issuer" },
    accepted_audiences = { "api" },
    required_claims = { "nonce" },
    now = 1000,
  })
  assert_true(bad == nil)
  assert_eq(err.status, "missing_required_field")

  bad, err = lonejson.jwt_validate_compact_signature(
      signed_token:sub(1, -2) .. (signed_token:sub(-1) == "A" and "B" or "A"),
      signed_jwk_json)
  assert_true(bad == nil)
  assert_eq(err.status, "type_mismatch")

  bad, err = lonejson.jwt_decode_compact(
      "eyJhbGciOiJSUzI1NiIsImFsZyI6IkVTMjU2In0." ..
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9.c2ln")
  assert_true(bad == nil)
  assert_eq(err.status, "duplicate_field")
end

do
  local events = {}
  local chunks = {}
  local ok, err = lj:visit_path_value_string('{"a":{"b":[true,null,"xy",12]}}', {
    object_begin = function(path)
      events[#events + 1] = "object:" .. path_key(path)
    end,
    object_key_chunk = function(path, chunk)
      events[#events + 1] = "key:" .. path_key(path) .. "=" .. chunk
    end,
    array_begin = function(path)
      events[#events + 1] = "array:" .. path_key(path)
    end,
    boolean_value = function(path, value)
      events[#events + 1] = "bool:" .. path_key(path) .. "=" .. tostring(value)
    end,
    null_value = function(path)
      events[#events + 1] = "null:" .. path_key(path)
    end,
    string_chunk = function(path, chunk)
      chunks[#chunks + 1] = path_key(path) .. "=" .. chunk
    end,
    number_chunk = function(path, chunk)
      events[#events + 1] = "number:" .. path_key(path) .. "=" .. chunk
    end,
  })

  assert_true(ok, err and err.message)
  assert_true(table.concat(events, "|"):find("object:", 1, true) ~= nil)
  assert_true(table.concat(events, "|"):find("key:=a", 1, true) ~= nil)
  assert_true(table.concat(events, "|"):find("key:a=b", 1, true) ~= nil)
  assert_true(table.concat(events, "|"):find("array:a/b", 1, true) ~= nil)
  assert_true(table.concat(events, "|"):find("bool:a/b/0=true", 1, true) ~= nil)
  assert_true(table.concat(events, "|"):find("null:a/b/1", 1, true) ~= nil)
  assert_true(table.concat(events, "|"):find("number:a/b/3=12", 1, true) ~= nil)
  assert_eq(table.concat(chunks), "a/b/2=xy")
end

do
  local path = test_temp_dir .. "/lonejson-lua-path-visitor.json"
  local f = assert(io.open(path, "wb"))
  local seen = {}

  f:write('{"from":"file","items":[1]}')
  f:close()
  assert_true(lonejson.visit_path_value_path(path, {
    string_chunk = function(p, chunk)
      seen[#seen + 1] = path_key(p) .. "=" .. chunk
    end,
  }))
  assert_eq(seen[1], "from=file")

  f = assert(io.open(path, "rb"))
  seen = {}
  assert_true(lj:visit_path_value_file(f, {
    number_chunk = function(p, chunk)
      seen[#seen + 1] = path_key(p) .. "=" .. chunk
    end,
  }))
  assert_eq(seen[1], "items/0=1")
  f:seek("set", 0)
  seen = {}
  assert_true(lj:visit_path_value_fd(f, {
    object_key_chunk = function(p, chunk)
      seen[#seen + 1] = path_key(p) .. "=" .. chunk
    end,
  }))
  assert_eq(seen[1], "=from")
  f:close()
  os.remove(path)
end

do
  local ok, err = lj:visit_path_value_string('{"a":1}', {
    number_chunk = function()
      error("path callback failed")
    end,
  })
  assert_true(ok == nil)
  assert_true(err ~= nil)
  assert_eq(err.status, "callback_failed")
  assert_true(err.message:find("path callback failed", 1, true) ~= nil, err.message)
end

local Test = build_test_schema(lj, "Test")
local TestPretty = build_test_schema(pretty_lj, "TestPretty")
local TestLimitedOutput = build_test_schema(limited_output_lj, "TestLimitedOutput")
local TestAllowDup = build_test_schema(allow_dup_lj, "TestAllowDup")

local UInts = lj.schema("UInts", {
  lj.field("value", lj.u64()),
  lj.field("values", lj.u64_array { fixed_capacity = 8, overflow = "fail" }),
  lj.field("tags", lj.string_array { fixed_capacity = 8, overflow = "fail" }),
})

local Query = lj.schema("Query", {
  lj.field("id", lj.string { required = true }),
  lj.field("selector", lj.json_value { required = true }),
  lj.field("fields", lj.json_value()),
  lj.field("last_error", lj.json_value()),
})

local Merge = build_merge_schema(lj, "Merge")
local MergeNoClear = build_merge_schema(merge_lj, "MergeNoClear")

local NestedQuery = build_nested_query_schema(lj, "NestedQuery")
local NestedQueryNoClear =
    build_nested_query_schema(merge_lj, "NestedQueryNoClear")

local Batch = build_batch_schema(lj, "Batch")
local BatchNoClear = build_batch_schema(merge_lj, "BatchNoClear")
local NestedQueryLimitedNoClear =
    build_nested_query_schema(limited_json_value_lj, "NestedQueryLimitedNoClear")

local FixedLarge = build_fixed_large_schema(lj, "FixedLarge")
local FixedLargeBudget =
    build_fixed_large_schema(fixed_large_budget_lj, "FixedLargeBudget")
local FixedLargeScratch =
    build_fixed_large_schema(fixed_large_scratch_lj, "FixedLargeScratch")

local Nullable = lj.schema("Nullable", {
  lj.field("required_count", lj.i64 { required = true }),
  lj.field("amount", lj.i64 { nullable = true }),
  lj.field("seed", lj.u64 { nullable = true }),
  lj.field("ratio", lj.f64 { nullable = true }),
  lj.field("enabled", lj.bool { nullable = true }),
  lj.field("child", lj.object {
    fields = {
      lj.field("priority", lj.i64 { nullable = true }),
    },
  }),
})

do
  local function long_text(ch, n)
    return string.rep(ch, n)
  end

  local budget_rec = FixedLargeBudget:new_record()
  local scratch_rec = FixedLargeScratch:new_record()
  local first = long_text("a", 6000)
  local second = long_text("b", 7000)
  local third = long_text("c", 6500)
  local ok, err, stream, obj, status

  assert_eq(fixed_string_scratch:size(), 8192)

  ok, err = FixedLargeBudget:decode_into(budget_rec, '{"payload":"' .. first .. '"}')
  assert_true(err == nil)
  assert_eq(budget_rec.payload, first)

  ok, err = FixedLargeBudget:decode_into(budget_rec, '{"payload":"' .. second .. '"}')
  assert_true(err ~= nil and err.status == "overflow")
  assert_eq(budget_rec.payload, first)

  ok, err = FixedLargeScratch:decode_into(scratch_rec, '{"payload":"' .. first .. '"}')
  assert_true(err == nil)
  assert_eq(scratch_rec.payload, first)

  ok, err = FixedLargeScratch:decode_into(scratch_rec, '{"payload":"' .. second .. '"}')
  assert_true(err == nil)
  assert_eq(scratch_rec.payload, second)

  ok, err = FixedLargeScratch:decode_into(
      scratch_rec, '{"payload":"' .. second .. '","payload":"' .. third .. '"}')
  assert_true(err ~= nil and err.status == "duplicate_field")
  assert_eq(scratch_rec.payload, second)

  ok, err = FixedLargeScratch:decode_into(
      scratch_rec, '{"payload":"' .. third .. '\\u0000tail"}')
  assert_true(err ~= nil and err.status == "type_mismatch")
  assert_eq(scratch_rec.payload, second)

  stream = FixedLargeScratch:stream_string(
      '{"payload":"' .. first .. '"}{"payload":"' .. second .. '"}')
  obj, err, status = stream:next(scratch_rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.payload, first)
  obj, err, status = stream:next(scratch_rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.payload, second)
  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
end

do
  local function new_gc_temp_dir_runtime()
    local temp_dir = table.concat({"", "tmp"}, "/")
    return lonejson.new({
      spool_default = {
        memory_limit = 1,
        temp_dir = temp_dir,
      },
      spool_blob = {
        memory_limit = 1,
        temp_dir = temp_dir,
      },
    }), temp_dir
  end

  local runtime, temp_dir = new_gc_temp_dir_runtime()
  local Temp = build_test_schema(runtime, "TempSpoolGc")
  local rec
  local body_path

  collectgarbage("collect")
  collectgarbage("collect")

  rec = Temp:decode('{"name":"Alice","body":"' .. string.rep("x", 128) .. '"}')
  body_path = rec.body:path()
  assert_true(body_path ~= nil)
  assert_true(body_path:sub(1, #temp_dir) == temp_dir)
  assert_true(exists(body_path))
end

do
  local weak = setmetatable({}, {__mode = "v"})
  local scratch = lonejson.fixed_string_scratch(8192)
  local ok, err

  weak[1] = scratch
  ok, err = pcall(function()
    lonejson.new({
      fixed_string_scratch = scratch,
      max_alloc_bytes = -1,
    })
  end)
  assert_true(not ok)
  assert_true(type(err) == "string")
  scratch = nil
  collectgarbage("collect")
  collectgarbage("collect")
  assert_true(weak[1] == nil)
end

do
  local saved_new = lonejson.core.new
  local ok, err

  lonejson.core.new = function()
    return {
      status = "allocation_failed",
      message = "failed to allocate runtime",
    }
  end

  ok, err = pcall(function()
    lonejson.new({})
  end)

  lonejson.core.new = saved_new
  assert_true(not ok)
  assert_true(type(err) == "table")
  assert_eq(err.status, "allocation_failed")
  assert_eq(err.message, "failed to allocate runtime")
end

do
  local obj = Test:decode('{"name":"Alice","age":3,"active":true}')
  assert_eq(obj.name, "Alice")
  assert_eq(obj.age, 3)
  assert_eq(obj.active, true)
end

do
  local obj =
      Nullable:decode('{"required_count":7,"child":{"priority":null}}')
  assert_eq(obj.required_count, 7)
  assert_true(obj.amount == nil)
  assert_true(obj.seed == nil)
  assert_true(obj.ratio == nil)
  assert_true(obj.enabled == nil)
  assert_true(obj.child.priority == nil)

  obj = Nullable:decode(
      '{"required_count":8,"amount":-12,"seed":42,"ratio":1.25,"enabled":true,"child":{"priority":3}}')
  assert_eq(obj.amount, -12)
  assert_eq(obj.seed, 42)
  assert_eq(obj.ratio, 1.25)
  assert_eq(obj.enabled, true)
  assert_eq(obj.child.priority, 3)

  local rec = Nullable:new_record()
  local amount_get = Nullable:compile_get("amount")
  local child_get = Nullable:compile_get("child.priority")
  Nullable:decode_into(rec, '{"required_count":9,"amount":null,"child":{}}')
  assert_true(rec.amount == nil)
  assert_true(rec:get("amount") == nil)
  assert_true(amount_get(rec) == nil)
  assert_true(rec:get("child.priority") == nil)
  assert_true(child_get(rec) == nil)
  Nullable:decode_into(rec, '{"required_count":10,"amount":5,"child":{"priority":6}}')
  assert_eq(rec.amount, 5)
  assert_eq(amount_get(rec), 5)
  assert_eq(child_get(rec), 6)

  local json = Nullable:encode({
    required_count = 11,
    amount = lj.json_null,
    seed = nil,
    ratio = 2.5,
    enabled = false,
    child = { priority = lj.json_null },
  })
  assert_true(json:find('"required_count":11', 1, true) ~= nil)
  assert_true(json:find('"ratio":2.5', 1, true) ~= nil)
  assert_true(json:find('"enabled":false', 1, true) ~= nil)
  assert_true(json:find('"amount"', 1, true) == nil)
  assert_true(json:find('"seed"', 1, true) == nil)
  assert_true(json:find('"priority"', 1, true) == nil)

  for _, bad in ipairs({
    '{"required_count":1,"amount":"x"}',
    '{"required_count":1,"seed":-1}',
    '{"required_count":1,"ratio":true}',
    '{"required_count":1,"enabled":0}',
  }) do
    local value, err = Nullable:decode(bad)
    assert_eq(value, bad)
    assert_true(err ~= nil and err.status == "type_mismatch",
                "nullable primitive accepted invalid JSON: " .. bad)
  end

  local ok = pcall(function()
    lj.schema("BadRequiredNullable", {
      lj.field("x", lj.i64 { required = true, nullable = true }),
    })
  end)
  assert_true(not ok)
  ok = pcall(function()
    lj.schema("BadStringNullable", {
      lj.field("x", lj.string { nullable = true }),
    })
  end)
  assert_true(not ok)
  ok = pcall(function()
    lj.schema("BadArrayNullable", {
      lj.field("x", lj.i64_array { nullable = true }),
    })
  end)
  assert_true(not ok)
end

do
  local description = string.rep("Synthetic context. ", 12)
  assert_eq(#description, 228)
  for _, encoder in ipairs({
    lonejson.encode_json, lonejson.encode_value,
    lonejson.core.encode_json, lonejson.core.encode_value,
    lj.encode_json, lj.encode_value,
    pretty_lj.encode_json, pretty_lj.encode_value,
  }) do
    for _, n in ipairs({125, 126, 127, 128, 253, 254, 255, 256, 512, 1024}) do
      local text = string.rep("x", n)
      assert_eq(encoder(text), '"' .. text .. '"')
      for _, escaped in ipairs({
        text .. '"\\\n', '"\\\n' .. text, string.rep('x"\\\n', n),
        text .. '\0\b\f\r\t', string.rep('\0\b\f\r\t', n),
      }) do
        assert_eq(lj.decode_json(encoder(escaped)), escaped)
      end
      local object = { [text] = text }
      assert_eq(lj.decode_json(encoder(object))[text], text)
    end
    for _, count in ipairs({0, 8, 9, 16, 17, 64}) do
      local object = {}
      for i = 1, count do
        object["field" .. i .. string.rep("x", i * 8)] = i
      end
      local decoded = lj.decode_json(encoder(object))
      local seen = 0
      for key, value in pairs(decoded) do
        assert_eq(value, object[key])
        seen = seen + 1
      end
      assert_eq(seen, count)
    end
    for _, depth in ipairs({16, 32, 64, 128}) do
      local value = "leaf"
      for _ = 1, depth do value = {value} end
      local encoded = encoder(value)
      assert_eq(encoded:gsub('%s', ''),
                string.rep("[", depth) .. '"leaf"' .. string.rep("]", depth))
      value = {value}
      if depth == 128 then
        local ok, err = pcall(encoder, value)
        assert_true(not ok)
        assert_true(err:find("nesting exceeds", 1, true) ~= nil, err)
      end
    end
    for _, count in ipairs({1, 10, 50, 100, 200, 400, 600}) do
      local records = {}
      for i = 1, count do
        records[i] = {
          id = i,
          name = "Synthetic opportunity " .. i,
          stage_id = (i % 5) + 1,
          owner_id = (i % 3) + 1,
          expected_revenue = i * 1000,
          contact_ids = {i + 1000},
          description = description,
        }
      end
      local decoded = lj.decode_json(encoder({
        backend = "synthetic-probe", total = count, opportunities = records,
      }))
      assert_eq(decoded.backend, "synthetic-probe")
      assert_eq(decoded.total, count)
      assert_eq(#decoded.opportunities, count)
      for i, row in ipairs(decoded.opportunities) do
        assert_eq(row.id, i)
        assert_eq(row.name, "Synthetic opportunity " .. i)
        assert_eq(row.stage_id, (i % 5) + 1)
        assert_eq(row.owner_id, (i % 3) + 1)
        assert_eq(row.expected_revenue, i * 1000)
        assert_eq(#row.contact_ids, 1)
        assert_eq(row.contact_ids[1], i + 1000)
        assert_eq(row.description, description)
      end
    end
  end
  for _, pretty in ipairs({false, true}) do
    for _, n in ipairs({253, 254, 255, 256, 512, 1024}) do
      for _, text in ipairs({string.rep("x", n), string.rep('x"\\\n', n)}) do
        local expected = lj.encode_json(text)
        local exact = new_runtime({
          write_pretty = pretty, write_max_output_bytes = #expected,
        })
        local short = new_runtime({
          write_pretty = pretty, write_max_output_bytes = #expected - 1,
        })
        for _, method in ipairs({"encode_json", "encode_value"}) do
          assert_eq(exact[method](text), expected)
          local ok, err = pcall(short[method], text)
          assert_true(not ok)
          assert_true(err:find("max_output_bytes", 1, true) ~= nil, err)
          assert_true(err:match("^[%g%s]+$") ~= nil, err)
          assert_eq(short[method]("ok"), '"ok"')
        end
      end
    end
  end
end

do
  local pretty_expected
  local huge
  assert_eq(lj.encode_json("a\nb"), [["a\nb"]])
  assert_eq(lj.encode_value("a\nb"), [["a\nb"]])
  assert_eq(lj.encode_json(lj.json_null), "null")
  assert_eq(lj.encode_json({ b = true, a = lj.json_array({ 1, lj.json_null }) }), '{"a":[1,null],"b":true}')
  pretty_expected = table.concat({
    "{",
    '  "a": [',
    "    1,",
    "    null",
    "  ],",
    '  "b": true',
    "}",
  }, "\n")
  assert_eq(pretty_lj.encode_json({ b = true, a = lj.json_array({ 1, lj.json_null }) }),
            pretty_expected)
  assert_eq(pretty_limited_json_value_lj.encode_json("ab"), [["ab"]])
  do
    local chunks = {}
    local pretty_chunks = {}
    lj.encode_json_to_sink({ z = "sink", a = lj.json_array({ true, false }) }, function(chunk)
      chunks[#chunks + 1] = chunk
    end)
    pretty_lj.encode_json_to_sink({ b = true, a = lj.json_array({ 1, lj.json_null }) }, function(chunk)
      pretty_chunks[#pretty_chunks + 1] = chunk
    end)
    assert_eq(table.concat(chunks), '{"a":[true,false],"z":"sink"}')
    assert_eq(table.concat(pretty_chunks), pretty_expected)
    assert_true(#pretty_chunks > 1)
  end
  huge = string.rep("x", 9 * 1024 * 1024)
  do
    local total = 0
    lj.encode_json_to_sink(huge, function(chunk)
      total = total + #chunk
    end)
    assert_eq(total, #huge + 2)
  end
  do
    local ok, err = pcall(function()
      lj.encode_value_to_sink({ a = 1 }, function()
        error("sink boom")
      end)
    end)
    assert_true(not ok)
    assert_true(tostring(err):find("sink boom", 1, true) ~= nil)
  end
  do
    local ok, err = pcall(function()
      pretty_lj.encode_json_to_sink({ a = 1, b = 2 }, function()
        error("pretty sink boom")
      end)
    end)
    assert_true(not ok)
    assert_true(tostring(err):find("pretty sink boom", 1, true) ~= nil)
  end
  do
    local ok, err = pcall(function()
      limited_output_lj.encode_json({ alpha = "abcdefghijklmnopqrstuvwxyz" })
    end)
    assert_true(not ok)
    assert_true(tostring(err):find("max_output_bytes", 1, true) ~= nil)
  end
  do
    local ok, err = pcall(function()
      pretty_limited_output_lj.encode_json({ alpha = string.rep("x", 4096) })
    end)
    assert_true(not ok)
    assert_true(tostring(err):find("max_output_bytes", 1, true) ~= nil)
    for _ = 1, 64 do
      ok, err = pcall(function()
        pretty_limited_output_lj.encode_json({ alpha = string.rep("x", 4096) })
      end)
      assert_true(not ok)
      assert_true(tostring(err):find("max_output_bytes", 1, true) ~= nil)
    end
  end
  do
    local ok, err = pcall(function()
      default_cap_lj.encode_json(huge)
    end)
    assert_true(not ok)
    assert_true(tostring(err):find("max_output_bytes", 1, true) ~= nil)
  end
  assert_eq(lj.decode_json('"x\\ny"'), "x\ny")
  assert_eq(lj.decode_value('"x\\ny"'), "x\ny")
  assert_true(lj.decode_json("null") == lj.json_null)
  local decoded = lj.decode_json('{"a":[1,null],"b":false}')
  assert_eq(decoded.a[1], 1)
  assert_true(decoded.a[2] == lj.json_null)
  assert_eq(decoded.b, false)
  do
    local ok, err = pcall(function()
      limited_json_value_lj.decode_json('"ab"')
    end)
    assert_true(not ok)
    assert_true(tostring(err):find("string", 1, true) ~= nil)
  end
  local ok = pcall(function()
    lj.decode_json("true false")
  end)
  assert_true(not ok)
end

do
  local rec = Test:new_record()
  local city_path = Test:compile_path("meta.city")
  local second_num_path = Test:compile_path("nums[2]")
  local city_get = Test:compile_get("meta.city")
  local nums_count = Test:compile_count("nums")
  local pretty_expected
  Test:decode_into(rec, '{"name":"Bob","age":5,"meta":{"city":"Uppsala","zip":44},"nums":[1,2,3]}')
  assert_eq(rec.name, "Bob")
  assert_eq(rec.age, 5)
  assert_eq(rec.meta.city, "Uppsala")
  assert_eq(rec:get("meta.city"), "Uppsala")
  assert_eq(rec:get(city_path), "Uppsala")
  assert_eq(city_path(rec), "Uppsala")
  assert_eq(city_get(rec), "Uppsala")
  assert_eq(rec:count("nums"), 3)
  assert_eq(city_path:get(rec), "Uppsala")
  assert_eq(Test:compile_path("nums"):count(rec), 3)
  assert_eq(nums_count(rec), 3)
  assert_eq(rec:get("nums[2]"), 2)
  assert_eq(rec:get(second_num_path), 2)
  assert_eq(second_num_path(rec), 2)
  assert_eq(rec.nums[2], 2)
  local json = Test:encode(rec)
  assert_true(json:find('"name":"Bob"', 1, true) ~= nil)
  pretty_expected = table.concat({
    "{",
    '  "name": "Bob",',
    '  "age": 5,',
    '  "active": false,',
    '  "body": "",',
    '  "payload": "",',
    '  "meta": {',
    '    "city": "Uppsala",',
    '    "zip": 44',
    "  },",
    '  "nums": [',
    "    1,",
    "    2,",
    "    3",
    "  ]",
    "}",
  }, "\n")
  assert_eq(TestPretty:encode(rec:to_table()), pretty_expected)
  local ok, err = pcall(function()
    TestLimitedOutput:encode(rec:to_table())
  end)
  assert_true(not ok)
  assert_true(tostring(err):find("max_output_bytes", 1, true) ~= nil)
end

do
  local text = ("abc123-"):rep(24)
  local raw = string.char(0, 1, 2, 3, 4, 5, 6, 7) .. ("XYZ"):rep(20)
  local rec = Test:new_record()
  local body_path
  local payload_path
  Test:assign(rec, {
    name = "Spool",
    body = text,
    payload = raw,
  })
  assert_true(rec.body:spilled())
  assert_true(rec.payload:spilled())
  assert_eq(rec.body:read_all(), text)
  assert_eq(rec.payload:read_all(), raw)
  body_path = rec.body:path()
  payload_path = rec.payload:path()
  assert_true(exists(body_path))
  assert_true(exists(payload_path))
  rec.body:rewind()
  assert_eq(rec.body:read(6), text:sub(1, 6))
  rec.body:rewind()
  local body_chunks = {}
  assert_true(rec.body:write_to(function(chunk)
    body_chunks[#body_chunks + 1] = chunk
  end, 5))
  assert_eq(table.concat(body_chunks), text)
  rec:clear()
  assert_true(not exists(body_path))
  assert_true(not exists(payload_path))
  Test:assign(rec, {
    name = "Spool",
    body = text,
    payload = raw,
  })
  local roundtrip = Test:decode(Test:encode(rec))
  assert_eq(roundtrip.body:read_all(), text)
  assert_eq(roundtrip.payload:read_all(), raw)
end

do
  local path = test_temp_dir .. "/lonejson-lua-test.json"
  local file_path = test_temp_dir .. "/lonejson-lua-file.json"
  local rec = Test:new_record()
  local f
  local pretty
  Test:assign(rec, {
    name = "Path",
    age = 7,
  })
  assert_true(TestPretty:write_path(rec:to_table(), path))
  f = assert(io.open(path, "rb"))
  pretty = assert(f:read("*a"))
  f:close()
  assert_true(pretty:find('\n  "name": "Path"', 1, true) ~= nil)
  local obj = Test:decode_path(path)
  assert_eq(obj.name, "Path")
  os.remove(path)

  f = assert(io.open(file_path, "wb"))
  assert_true(TestPretty:write_file({ name = "File", age = 11 }, f))
  f:close()
  f = assert(io.open(file_path, "rb"))
  obj = Test:decode_file(f)
  f:close()
  assert_eq(obj.name, "File")
  assert_eq(obj.age, 11)
  os.remove(file_path)
end

do
  local path = test_temp_dir .. "/lonejson-lua-fd.json"
  local f
  local obj

  local rec = Test:new_record()
  Test:assign(rec, {
    name = "FD",
    age = 9,
  })

  f = assert(io.open(path, "wb"))
  assert_true(Test:write_fd(rec, f))
  f:close()

  f = assert(io.open(path, "rb"))
  obj = Test:decode_fd(f)
  assert_eq(obj.name, "FD")
  assert_eq(obj.age, 9)
  f:close()
  os.remove(path)
end

do
  local path = test_temp_dir .. "/lonejson-lua-stream.jsonl"
  local file_path = test_temp_dir .. "/lonejson-lua-stream-file.jsonl"
  local fd_path = test_temp_dir .. "/lonejson-lua-stream-fd.jsonl"
  local f
  local stream
  local rec = Test:new_record()
  local obj, err, status

  f = assert(io.open(path, "wb"))
  f:write('{"name":"One","age":1}{"name":"Two","age":2}')
  f:close()

  stream = Test:stream_path(path)
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.name, "One")
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_eq(obj.name, "Two")
  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
  os.remove(path)

  f = assert(io.open(file_path, "wb"))
  f:write('{"name":"FileOne","age":3}{"name":"FileTwo","age":4}')
  f:close()
  f = assert(io.open(file_path, "rb"))
  stream = Test:stream_file(f)
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.name, "FileOne")
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_eq(obj.name, "FileTwo")
  stream:close()
  f:close()
  os.remove(file_path)

  f = assert(io.open(fd_path, "wb"))
  f:write('{"name":"FdOne","age":5}{"name":"FdTwo","age":6}')
  f:close()
  f = assert(io.open(fd_path, "rb"))
  stream = Test:stream_fd(f)
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.name, "FdOne")
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_eq(obj.name, "FdTwo")
  stream:close()
  f:close()
  os.remove(fd_path)
end

do
  local stream
  local rec = Test:new_record()
  local obj, err, status

  stream = Test:stream_string('{"name":"One","age":1}{"name":"Two","age":2}')
  collectgarbage("collect")

  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.name, "One")
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.name, "Two")
  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
end

do
  local OptionalText = lj.schema("OptionalText", {
    lj.field("name", lj.string()),
    lj.field("nick", lj.string()),
  })
  local stream =
      OptionalText:stream_string(
          '{"name":"One","nick":"alpha"}{"name":"Two"}')
  local rec = OptionalText:new_record()
  local obj, err, status

  rec:clear()
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.name, "One")
  assert_eq(obj.nick, "alpha")

  rec:clear()
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.name, "Two")
  assert_true(obj.nick == nil)

  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
end

do
  local max_u64 = "18446744073709551615"
  local above_i64 = "9223372036854775808"
  local obj = UInts:decode('{"value":18446744073709551615,"values":[1,9223372036854775808,18446744073709551615],"tags":["a","b"]}')
  assert_eq(type(obj.value), "string")
  assert_eq(obj.value, max_u64)
  assert_eq(type(obj.values[2]), "string")
  assert_eq(obj.values[2], above_i64)
  assert_eq(obj.values[3], max_u64)

  local rec = UInts:new_record()
  UInts:assign(rec, {
    value = max_u64,
    values = { 1, above_i64, max_u64 },
    tags = { [2] = "b", [1] = "a", extra = "ignored" },
  })
  assert_eq(rec.value, max_u64)
  assert_eq(rec.values[2], above_i64)
  assert_eq(UInts:encode(rec), '{"value":18446744073709551615,"values":[1,9223372036854775808,18446744073709551615],"tags":["a","b"]}')
end

do
  local default_lj = lonejson.new()
  local DefaultJsonValue = default_lj.schema("DefaultJsonValue", {
    default_lj.field("v", default_lj.json_value { required = true }),
  })
  local obj = DefaultJsonValue:decode('{"v":{"a":1}}')

  assert_true(obj.v ~= nil)
  assert_eq(getmetatable(obj.v).__lonejson_json_kind, "object")
  assert_eq(obj.v.a, 1)
end

do
  local obj = Query:decode('{"id":"q-42","selector":{"op":"and","clauses":[{"field":"status","value":"open"},{"field":"priority","gte":3}]},"fields":["id","title",{"metrics":[true,null,3.14]}],"last_error":null}')
  assert_eq(obj.id, "q-42")
  assert_eq(getmetatable(obj.selector).__lonejson_json_kind, "object")
  assert_eq(obj.selector.op, "and")
  assert_eq(getmetatable(obj.selector.clauses).__lonejson_json_kind, "array")
  assert_eq(obj.selector.clauses[2].gte, 3)
  assert_eq(getmetatable(obj.fields).__lonejson_json_kind, "array")
  assert_eq(obj.fields[3].metrics[1], true)
  assert_true(obj.fields[3].metrics[2] == lj.json_null)
  assert_true(obj.last_error == nil)
end

do
  local obj = Query:decode('{"id":"q-null","selector":{"a":null,"b":[1,null,2]},"fields":{"inner":[null,true]},"last_error":null}')
  local encoded

  assert_true(obj.selector.a == lj.json_null)
  assert_true(obj.selector.b[2] == lj.json_null)
  assert_true(obj.fields.inner[1] == lj.json_null)
  assert_true(obj.last_error == nil)

  encoded = Query:encode(obj)
  assert_true(encoded:find('"a":null', 1, true) ~= nil)
  assert_true(encoded:find('"b":[1,null,2]', 1, true) ~= nil)
  assert_true(encoded:find('"inner":[null,true]', 1, true) ~= nil)
end

do
  local obj = NestedQuery:decode(
      '{"type":"null-selector","response":{"status":"ok","selector":null,"fields":null}}')

  assert_true(obj.response.selector == nil)
  assert_true(obj.response.fields == nil)
end

do
  local obj = Batch:decode(
      '{"batch_id":"b-null","items":[{"id":"i1","payload":null,"tags":null}]}')

  assert_true(obj.items[1].payload == nil)
  assert_true(obj.items[1].tags == nil)
end

do
  local path = test_temp_dir .. "/lonejson-lua-query-stream.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local obj, err, status

  f:write('{"id":"stream-1","selector":{"kind":"term","field":"name","value":"alice"},"fields":["id",{"deep":[true,false,null,4.5]}],"last_error":{"code":"bad","retryable":false}}')
  f:close()

  stream = Query:stream_path(path)
  obj, err, status = stream:next()
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.selector.kind, "term")
  assert_eq(getmetatable(obj.fields).__lonejson_json_kind, "array")
  assert_eq(obj.fields[2].deep[1], true)
  assert_eq(obj.fields[2].deep[2], false)
  assert_true(obj.fields[2].deep[3] == lj.json_null)
  assert_eq(obj.last_error.code, "bad")
  assert_eq(obj.last_error.retryable, false)
  obj, err, status = stream:next()
  assert_eq(status, "eof")
  stream:close()
  os.remove(path)
end

do
  local path = test_temp_dir .. "/lonejson-lua-nested-stream.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local obj, err, status

  f:write('{"type":"stream-1","response":{"status":"ok","selector":{"kind":"term","value":"alice"},"fields":["id",{"deep":[true,null]}]}}')
  f:close()

  stream = NestedQuery:stream_path(path)
  obj, err, status = stream:next()
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.response.selector.kind, "term")
  assert_eq(obj.response.selector.value, "alice")
  assert_eq(obj.response.fields[1], "id")
  assert_true(obj.response.fields[2].deep[2] == lj.json_null)
  obj, err, status = stream:next()
  assert_eq(status, "eof")
  stream:close()
  os.remove(path)
end

do
  local path = test_temp_dir .. "/lonejson-lua-batch-stream.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local obj, err, status

  f:write('{"batch_id":"b1","items":[{"id":"i1","payload":{"k":"v"},"tags":[true,null]},{"id":"i2","payload":[1,2,3]}]}')
  f:close()

  stream = Batch:stream_path(path)
  obj, err, status = stream:next()
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.items[1].payload.k, "v")
  assert_eq(obj.items[1].tags[1], true)
  assert_true(obj.items[1].tags[2] == lj.json_null)
  assert_eq(obj.items[2].payload[3], 3)
  obj, err, status = stream:next()
  assert_eq(status, "eof")
  stream:close()
  os.remove(path)
end

do
  local rec = Query:new_record()
  Query:decode_into(rec, '{"id":"record-null","selector":{"x":[null,1],"y":{"z":null}}}')
  assert_true(rec.selector.x[1] == lj.json_null)
  assert_true(rec.selector.y.z == lj.json_null)
  assert_true(Query:encode(rec):find('"x":[null,1]', 1, true) ~= nil)
end

do
  local rec = MergeNoClear:new_record()

  MergeNoClear:assign(rec, {
    id = "old",
    age = 7,
    selector = lj.json_object { a = 1 },
    fields = lj.json_array { "keep" },
  })
  MergeNoClear:decode_into(rec, '{"selector":{"b":2}}')

  assert_eq(rec.id, "old")
  assert_eq(rec.age, 7)
  assert_eq(getmetatable(rec.selector).__lonejson_json_kind, "object")
  assert_true(rec.selector.a == nil)
  assert_eq(rec.selector.b, 2)
  assert_eq(getmetatable(rec.fields).__lonejson_json_kind, "array")
  assert_eq(rec.fields[1], "keep")
end

do
  local obj = NestedQuery:decode(
      '{"type":"nested","response":{"status":"ok","selector":{"x":[null,1],"y":{"z":null}},"fields":[true,{"deep":[null,4]}]}}')
  assert_eq(obj.type, "nested")
  assert_eq(obj.response.status, "ok")
  assert_true(obj.response.selector.x[1] == lj.json_null)
  assert_true(obj.response.selector.y.z == lj.json_null)
  assert_eq(obj.response.fields[1], true)
  assert_true(obj.response.fields[2].deep[1] == lj.json_null)
  assert_eq(obj.response.fields[2].deep[2], 4)
end

do
  local obj = Batch:decode(
      '{"batch_id":"b1","items":[{"id":"i1","payload":{"a":[null,1]},"tags":["x",{"deep":true}]},{"id":"i2","payload":[true,null,2]}]}')
  assert_eq(obj.batch_id, "b1")
  assert_eq(obj.items[1].id, "i1")
  assert_true(obj.items[1].payload.a[1] == lj.json_null)
  assert_eq(obj.items[1].payload.a[2], 1)
  assert_eq(obj.items[1].tags[1], "x")
  assert_eq(obj.items[1].tags[2].deep, true)
  assert_eq(obj.items[2].id, "i2")
  assert_eq(obj.items[2].payload[1], true)
  assert_true(obj.items[2].payload[2] == lj.json_null)
  assert_eq(obj.items[2].payload[3], 2)
end

do
  local rec = NestedQueryNoClear:new_record()

  NestedQueryNoClear:decode_into(
      rec,
      '{"type":"first","response":{"status":"ok","selector":{"a":1},"fields":["keep"]}}')
  assert_eq(rec.type, "first")
  assert_eq(rec.response.status, "ok")
  assert_eq(rec.response.selector.a, 1)
  assert_eq(rec.response.fields[1], "keep")

  NestedQueryNoClear:decode_into(
      rec,
      '{"type":"second","response":{"selector":{"b":[true,null]},"fields":{"deep":{"v":2}}}}')
  assert_eq(rec.type, "second")
  assert_true(rec.response.status == nil)
  assert_true(rec.response.selector.a == nil)
  assert_eq(rec.response.selector.b[1], true)
  assert_true(rec.response.selector.b[2] == lj.json_null)
  assert_eq(rec.response.fields.deep.v, 2)
end

do
  local rec = NestedQueryLimitedNoClear:new_record()
  local ok, err
  local stream
  local obj, status

  ok, err = NestedQueryLimitedNoClear:decode_into(
      rec,
      '{"type":"limited","response":{"selector":{"value":"ab"}}}')
  assert_true(ok ~= nil)
  assert_true(err ~= nil and err.status == "overflow")

  stream = NestedQueryLimitedNoClear:stream_string(
      '{"type":"limited","response":{"selector":{"value":"ab"}}}')
  obj, err, status = stream:next(rec)
  assert_true(not obj)
  assert_true(err ~= nil and err.status == "overflow")
  assert_eq(status, "error")
  stream:close()
end

do
  local rec = BatchNoClear:new_record()

  BatchNoClear:decode_into(
      rec, '{"batch_id":"b1","items":[{"id":"i1","payload":{"a":1},"tags":["keep"]}]}')
  assert_eq(rec.batch_id, "b1")
  assert_eq(rec.items[1].id, "i1")
  assert_eq(rec.items[1].payload.a, 1)
  assert_eq(rec.items[1].tags[1], "keep")

  BatchNoClear:decode_into(
      rec,
      '{"batch_id":"b2","items":[{"id":"i2","payload":[true,null],"tags":{"fresh":2}}]}')
  assert_eq(rec.batch_id, "b2")
  assert_eq(rec.items[1].id, "i2")
  assert_eq(rec.items[1].payload[1], true)
  assert_true(rec.items[1].payload[2] == lj.json_null)
  assert_eq(rec.items[1].tags.fresh, 2)
end

do
  local path = test_temp_dir .. "/lonejson-lua-merge-stream.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local rec = MergeNoClear:new_record()
  local obj, err, status

  f:write('{"id":"stream-old","age":9,"selector":{"a":1},"fields":["keep"]}')
  f:write('{"selector":{"b":2}}')
  f:close()

  stream = MergeNoClear:stream_path(path)
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.id, "stream-old")
  assert_eq(obj.age, 9)
  assert_eq(obj.selector.a, 1)
  assert_eq(obj.fields[1], "keep")

  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(err == nil)
  assert_eq(obj.id, "stream-old")
  assert_eq(obj.age, 9)
  assert_true(obj.selector.a == nil)
  assert_eq(obj.selector.b, 2)
  assert_eq(obj.fields[1], "keep")

  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
  os.remove(path)
end

do
  local path = test_temp_dir .. "/lonejson-lua-nested-merge-stream.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local rec = NestedQueryNoClear:new_record()
  local obj, err, status

  f:write('{"type":"stream-old","response":{"status":"ok","selector":{"a":1},"fields":["keep"]}}')
  f:write('{"type":"stream-new","response":{"selector":{"b":2}}}')
  f:close()

  stream = NestedQueryNoClear:stream_path(path)
  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.type, "stream-old")
  assert_eq(rec.response.status, "ok")
  assert_eq(rec.response.selector.a, 1)
  assert_eq(rec.response.fields[1], "keep")

  obj, err, status = stream:next(rec)
  assert_eq(status, "object")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.type, "stream-new")
  assert_true(rec.response.status == nil)
  assert_true(rec.response.selector.a == nil)
  assert_eq(rec.response.selector.b, 2)
  assert_true(rec.response.fields == nil)

  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
  os.remove(path)
end

do
  local rec = Query:new_record()
  local encoded
  local roundtrip

  Query:assign(rec, {
    id = "assign",
    selector = lj.json_object {
      op = "or",
      clauses = lj.json_array {
        lj.json_object { field = "state", value = "ready" },
        lj.json_object { field = "retries", lt = 5 },
      },
    },
    fields = lj.json_array { "id", "title", lj.json_object { nested = lj.json_array { true, false, 1.25 } } },
    last_error = nil,
  })
  encoded = Query:encode(rec)
  assert_true(encoded:find('"selector":{"clauses":[', 1, true) ~= nil)
  assert_true(encoded:find('"last_error":null', 1, true) ~= nil)
  roundtrip = Query:decode(encoded)
  assert_eq(roundtrip.selector.clauses[1].field, "state")
  assert_eq(roundtrip.fields[3].nested[3], 1.25)
  assert_eq(getmetatable(roundtrip.fields).__lonejson_json_kind, "array")
end

do
  local stream =
    Test:array_stream_string("items", '{"meta":{"source":"skip"},"items":[{"name":"A","age":1},{"name":"B","age":2}]}')
  local obj, err, status

  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "A")
  assert_eq(obj.age, 1)

  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "B")
  assert_eq(obj.age, 2)

  obj, err, status = stream:next()
  assert_eq(status, "eof")
  assert_true(err == nil)
  stream:close()
end

do
  local stream
  local obj, err, status

  stream = Test:array_stream_string(
      "items",
      '{"items":[{"name":"GC","age":3},{"name":"Keep","age":4}]}')
  collectgarbage("collect")

  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "GC")
  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "Keep")
  obj, err, status = stream:next()
  assert_eq(status, "eof")
  stream:close()
end

do
  local stream =
    Query:array_stream_string("queries", '{"queries":[{"id":"q1","selector":{"x":null},"fields":["id",{"deep":[true,null]}]},{"id":"q2","selector":{"ok":true}}]}')
  local rec = Query:new_record()
  local obj, err, status

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.id, "q1")
  assert_true(rec.selector.x == lj.json_null)
  assert_eq(rec.fields[2].deep[1], true)
  assert_true(rec.fields[2].deep[2] == lj.json_null)

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.id, "q2")
  assert_eq(rec.selector.ok, true)

  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
end

do
  local stream =
    NestedQuery:array_stream_string("items", '{"items":[{"type":"q1","response":{"status":"ok","selector":{"x":null},"fields":["id",{"deep":[true,null]}]}},{"type":"q2","response":{"selector":{"ok":true}}}]}')
  local rec = NestedQuery:new_record()
  local obj, err, status

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.type, "q1")
  assert_eq(rec.response.status, "ok")
  assert_true(rec.response.selector.x == lj.json_null)
  assert_eq(rec.response.fields[2].deep[1], true)
  assert_true(rec.response.fields[2].deep[2] == lj.json_null)

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.type, "q2")
  assert_true(rec.response.status == nil)
  assert_eq(rec.response.selector.ok, true)

  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
end

do
  local stream =
    MergeNoClear:array_stream_string("items", '{"items":[{"id":"stream-old","age":9,"selector":{"a":1},"fields":["keep"]},{"selector":{"b":2}}]}')
  local rec = MergeNoClear:new_record()
  local obj, err, status

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.id, "stream-old")
  assert_eq(rec.age, 9)
  assert_eq(rec.selector.a, 1)
  assert_eq(rec.fields[1], "keep")

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.id, "stream-old")
  assert_eq(rec.age, 9)
  assert_true(rec.selector.a == nil)
  assert_eq(rec.selector.b, 2)
  assert_eq(rec.fields[1], "keep")

  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
end

do
  local stream =
    BatchNoClear:array_stream_string("", '[{"batch_id":"b1","items":[{"id":"i1","payload":{"x":null},"tags":["keep"]}]},{"batch_id":"b2","items":[{"id":"i2","payload":[true,null],"tags":{"fresh":2}}]}]')
  local rec = BatchNoClear:new_record()
  local obj, err, status

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.batch_id, "b1")
  assert_eq(rec.items[1].id, "i1")
  assert_true(rec.items[1].payload.x == lj.json_null)
  assert_eq(rec.items[1].tags[1], "keep")

  obj, err, status = stream:next(rec)
  assert_eq(status, "item")
  assert_true(obj == rec)
  assert_true(err == nil)
  assert_eq(rec.batch_id, "b2")
  assert_eq(rec.items[1].id, "i2")
  assert_true(rec.items[1].payload[1] == true)
  assert_true(rec.items[1].payload[2] == lj.json_null)
  assert_eq(rec.items[1].tags.fresh, 2)

  obj, err, status = stream:next(rec)
  assert_eq(status, "eof")
  stream:close()
end

do
  local stream = Query:array_stream_string("", '[{"a":1},null,[true,null],"text"]')
  local value, err, status

  value, err, status = stream:next_value()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(value.a, 1)
  assert_eq(getmetatable(value).__lonejson_json_kind, "object")

  value, err, status = stream:next_value()
  assert_eq(status, "item")
  assert_true(value == nil)
  assert_true(err == nil)

  value, err, status = stream:next_value()
  assert_eq(status, "item")
  assert_eq(value[1], true)
  assert_true(value[2] == lj.json_null)
  assert_eq(getmetatable(value).__lonejson_json_kind, "array")

  value, err, status = stream:next_value()
  assert_eq(status, "item")
  assert_eq(value, "text")

  value, err, status = stream:next_value()
  assert_eq(status, "eof")
  stream:close()
end

do
  local path = test_temp_dir .. "/lonejson-lua-array-stream.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local obj, err, status

  f:write('{"items":[{"name":"Path","age":11}]}')
  f:close()

  stream = Test:array_stream_path("items", path)
  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "Path")
  obj, err, status = stream:next()
  assert_eq(status, "eof")
  stream:close()
  os.remove(path)
end

do
  local path = test_temp_dir .. "/lonejson-lua-array-stream-fd.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local obj, err, status

  f:write('{"items":[{"name":"FD","age":12}]}')
  f:close()

  f = assert(io.open(path, "rb"))
  stream = Test:array_stream_fd("items", f)
  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "FD")
  stream:close()
  f:close()
  os.remove(path)
end

do
  local path = test_temp_dir .. "/lonejson-lua-array-stream-file.json"
  local f = assert(io.open(path, "wb"))
  local stream
  local obj, err, status

  f:write('{"items":[{"name":"File","age":13}]}')
  f:close()

  f = assert(io.open(path, "rb"))
  stream = Test:array_stream_file("items", f)
  obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "File")
  stream:close()
  f:close()
  os.remove(path)
end

do
  local bad = Test:array_stream_string("boards.items", '{"boards":[{"items":[{"name":"x"}]}]}')
  assert_true(type(bad) == "table")
  assert_true(bad.status ~= nil)
end

do
  local stream = Test:array_stream_string("items", '{"meta":{"x":1,"x":2},"items":[{"name":"bad"}]}')
  local obj, err, status = stream:next()
  assert_eq(status, "error")
  assert_true(obj == nil)
  assert_true(err ~= nil)
  assert_eq(err.status, "duplicate_field")
  stream:close()
end

do
  local stream = TestAllowDup:array_stream_string("items", '{"meta":{"x":1,"x":2},"items":[{"name":"ok","age":14}]}')
  local obj, err, status = stream:next()
  assert_eq(status, "item")
  assert_true(err == nil)
  assert_eq(obj.name, "ok")
  assert_eq(obj.age, 14)
  obj, err, status = stream:next()
  assert_eq(status, "eof")
  stream:close()
end

do
  local stream = Query:array_stream_string("", '[{"ok":true}x]')
  local value, err, status = stream:next_value()
  assert_eq(status, "error")
  assert_true(value == nil)
  assert_true(err ~= nil)
  stream:close()
end

do
  local input = '{"items":[{"id":"a","n":1},{"id":"b","n":2},{"id":"c","n":3}],"tail":true}'
  local seen = {}
  local output = lj.array_rewrite_string("items", input, {
    item = function(item)
      seen[#seen + 1] = item.id
      if item.id == "a" then
        return "keep"
      end
      if item.id == "b" then
        return { action = "replace", replacement = { id = "bb", n = 20 } }
      end
      return { action = "insert_after", insert = { id = "after-c", n = 4 } }
    end,
    append = function(ctx, emit)
      assert_eq(ctx.selector, "items")
      local ok, err = emit({ id = "appended", n = 5 })
      assert_true(ok, err and err.message)
    end,
  })
  assert_eq(table.concat(seen, ","), "a,b,c")
  assert_eq(output, '{"items":[{"id":"a","n":1},{"id":"bb","n":20},{"id":"c","n":3},{"id":"after-c","n":4},{"id":"appended","n":5}],"tail":true}')
end

do
  local Parent = lj.schema("RewriteParent", {
    lj.field("id", lj.string { required = true }),
  })
  local selected = {}
  local output = lj.array_rewrite_string("boards[].items", '{"boards":[{"id":"left","items":[{"v":1},{"v":2}]},{"id":"right","items":[{"v":3}]}]}', {
    parents = {
      { segment = "boards", schema = Parent },
    },
    item = function(item, ctx)
      selected[#selected + 1] = ctx.parents[1].value.id .. ":" .. tostring(item.v)
      if ctx.parents[1].value.id == "left" then
        return "drop"
      end
      return { action = "replace", value = { v = item.v * 10 } }
    end,
    append = function(ctx, emit)
      if ctx.parents[1].value.id == "right" then
        assert_true(emit({ v = 40 }))
      end
    end,
  })
  assert_eq(table.concat(selected, ","), "left:1,left:2,right:3")
  assert_eq(output, '{"boards":[{"id":"left","items":[]},{"id":"right","items":[{"v":30},{"v":40}]}]}')
end

do
  local input_path = test_temp_dir .. "/lonejson-lua-array-rewrite-in.json"
  local output_path = test_temp_dir .. "/lonejson-lua-array-rewrite-out.json"
  local f = assert(io.open(input_path, "wb"))
  f:write('{"items":[{"id":1},{"id":2}]}')
  f:close()
  local ok, err = lj.array_rewrite_path("items", input_path, output_path, {
    item = function(item)
      if item.id == 1 then
        return "drop"
      end
      return "keep"
    end,
  })
  assert_true(ok, err and err.message)
  f = assert(io.open(output_path, "rb"))
  assert_eq(f:read("*a"), '{"items":[{"id":2}]}')
  f:close()
  os.remove(input_path)
  os.remove(output_path)
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1,2]}', {
    item = function()
      error("forced item failure")
    end,
  })
  assert_true(out == nil)
  assert_true(err ~= nil)
  assert_eq(err.status, "callback_failed")
  assert_true(err.message:find("forced item failure", 1, true) ~= nil)
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      return "nonsense"
    end,
  })
  assert_true(out == nil)
  assert_true(err ~= nil)
  assert_eq(err.status, "callback_failed")
  assert_true(err.message:find("unsupported array rewrite action", 1, true) ~= nil)
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      return { action = "replace", value = function()
      end }
    end,
  })
  assert_rewrite_callback_failed(out, err, "unsupported Lua type for JSON value")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      return { action = "replace", value = lj.json_object({ [2] = "x" }) }
    end,
  })
  assert_rewrite_callback_failed(out, err, "JSON object keys must be strings")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      return { action = "insert_after", insert = lj.json_object({ [2] = "x" }) }
    end,
  })
  assert_rewrite_callback_failed(out, err, "JSON object keys must be strings")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      local cycle = {}
      cycle.self = cycle
      return { action = "insert_after", insert = cycle }
    end,
  })
  assert_rewrite_callback_failed(out, err, "cyclic Lua tables cannot be encoded as JSON")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1]}', {
    item = function()
      return { action = "replace_and_insert_after", replacement = { n = 0 / 0 }, insert = { ok = true } }
    end,
  })
  assert_rewrite_callback_failed(out, err, "JSON numbers must be finite")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[]}', {
    append = function(ctx, emit)
      local ok, emit_err = emit(lj.json_object({ [2] = "x" }))
      assert_true(ok == nil)
      assert_true(emit_err ~= nil)
      error(emit_err.message)
    end,
  })
  assert_rewrite_callback_failed(out, err, "JSON object keys must be strings")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[]}', {
    append = function(ctx, emit)
      local ok, emit_err = emit({ bad = function()
      end })
      assert_true(ok == nil)
      assert_true(emit_err ~= nil)
      error(emit_err.message)
    end,
  })
  assert_rewrite_callback_failed(out, err, "unsupported Lua type for JSON value")
end

do
  local out, err = lj.array_rewrite_string("items", '{"items":[1,]}', {
    item = function()
      return "keep"
    end,
  })
  assert_true(out == nil)
  assert_true(err ~= nil)
  assert_true(err.status ~= "ok")
end

do
  local ok = pcall(function()
    lj.array_rewrite_string("boards[].items", '{"boards":[{"items":[1]}]}', {
      parents = {
        { segment = "boards" },
      },
      item = function()
        return "keep"
      end,
    })
  end)
  assert_true(not ok)
end

print("lua tests passed")
