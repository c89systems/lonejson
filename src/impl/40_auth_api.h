#ifdef LONEJSON_WITH_JWT
#if defined(LONEJSON_INTERNAL_OPENSSL_IMPLEMENTATION)
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#endif

static const lonejson_field lonejson__jwk_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(lonejson_jwk, kty, "kty"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, kid, "kid"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, alg, "alg"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, use, "use"),
    LONEJSON_FIELD_STRING_ARRAY(lonejson_jwk, key_ops, "key_ops",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, crv, "crv"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, n, "n"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, e, "e"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, x, "x"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, y, "y"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, k, "k"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, x5t, "x5t"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, x5t_s256, "x5t#S256"),
    LONEJSON_FIELD_STRING_ARRAY(lonejson_jwk, x5c, "x5c",
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(lonejson__jwk_map, lonejson_jwk, lonejson__jwk_fields);

static const lonejson_field lonejson__jwks_fields[] = {
    LONEJSON_FIELD_OBJECT_ARRAY(lonejson_jwks, keys, "keys", lonejson_jwk,
                                &lonejson__jwk_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(lonejson__jwks_map, lonejson_jwks, lonejson__jwks_fields);

#ifdef LONEJSON_WITH_OIDC
static const lonejson_field lonejson__oidc_discovery_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(lonejson_oidc_discovery, issuer, "issuer"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oidc_discovery, authorization_endpoint,
                                "authorization_endpoint"),
    LONEJSON_FIELD_STRING_ALLOC_REQ(lonejson_oidc_discovery, token_endpoint,
                                    "token_endpoint"),
    LONEJSON_FIELD_STRING_ALLOC_REQ(lonejson_oidc_discovery, jwks_uri,
                                    "jwks_uri"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oidc_discovery, introspection_endpoint,
                                "introspection_endpoint"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oidc_discovery, revocation_endpoint,
                                "revocation_endpoint"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oidc_discovery, userinfo_endpoint,
                                "userinfo_endpoint")};
LONEJSON_MAP_DEFINE(lonejson__oidc_discovery_map, lonejson_oidc_discovery,
                    lonejson__oidc_discovery_fields);

static const lonejson_field lonejson__oauth2_token_response_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, access_token,
                                "access_token"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, token_type,
                                "token_type"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, refresh_token,
                                "refresh_token"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, scope, "scope"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, id_token,
                                "id_token"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, error, "error"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response,
                                error_description, "error_description"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, error_uri,
                                "error_uri"),
    LONEJSON_FIELD_I64_PRESENT(lonejson_oauth2_token_response, expires_in,
                               has_expires_in, "expires_in")};
LONEJSON_MAP_DEFINE(lonejson__oauth2_token_response_map,
                    lonejson_oauth2_token_response,
                    lonejson__oauth2_token_response_fields);

static const lonejson_field lonejson__oauth2_introspection_response_fields[] = {
    LONEJSON_FIELD_BOOL_PRESENT(lonejson_oauth2_introspection_response, active,
                                has_active, "active"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_introspection_response, scope,
                                "scope"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_introspection_response,
                                client_id, "client_id"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_introspection_response,
                                username, "username"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_introspection_response,
                                token_type, "token_type"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_introspection_response, sub,
                                "sub"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_introspection_response, iss,
                                "iss"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_introspection_response, jti,
                                "jti"),
    LONEJSON_FIELD_I64_PRESENT(lonejson_oauth2_introspection_response, exp,
                               has_exp, "exp"),
    LONEJSON_FIELD_I64_PRESENT(lonejson_oauth2_introspection_response, iat,
                               has_iat, "iat"),
    LONEJSON_FIELD_I64_PRESENT(lonejson_oauth2_introspection_response, nbf,
                               has_nbf, "nbf")};
LONEJSON_MAP_DEFINE(lonejson__oauth2_introspection_response_map,
                    lonejson_oauth2_introspection_response,
                    lonejson__oauth2_introspection_response_fields);

static const lonejson_field lonejson__oidc_userinfo_known_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oidc_userinfo_response, sub, "sub"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oidc_userinfo_response, name, "name"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oidc_userinfo_response,
                                preferred_username, "preferred_username"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oidc_userinfo_response, email,
                                "email"),
    LONEJSON_FIELD_BOOL_PRESENT(lonejson_oidc_userinfo_response, email_verified,
                                has_email_verified, "email_verified")};
LONEJSON_MAP_DEFINE(lonejson__oidc_userinfo_known_map,
                    lonejson_oidc_userinfo_response,
                    lonejson__oidc_userinfo_known_fields);
#endif

#define LONEJSON__JWT_DEFAULT_MAX_HEADER_BYTES (16u * 1024u)
#define LONEJSON__JWT_DEFAULT_MAX_CLAIMS_BYTES (256u * 1024u)
#define LONEJSON__JWT_MAX_RSA_COMPONENT_BYTES (8u * 1024u)
#define LONEJSON__OIDC_DEFAULT_MAX_DISCOVERY_BYTES (256u * 1024u)
#define LONEJSON__OIDC_DEFAULT_MAX_JWKS_BYTES (1024u * 1024u)
#define LONEJSON__OAUTH2_DEFAULT_MAX_FORM_BODY_BYTES (64u * 1024u)
#define LONEJSON__OAUTH2_DEFAULT_MAX_TOKEN_RESPONSE_BYTES (1024u * 1024u)
#define LONEJSON__OAUTH2_DEFAULT_MAX_INTROSPECTION_RESPONSE_BYTES              \
  (1024u * 1024u)
#define LONEJSON__OAUTH2_DEFAULT_TOKEN_FLOW_REFRESH_SKEW_SECONDS 60
#define LONEJSON__OAUTH2_DEFAULT_TOKEN_FLOW_RETRIES 2u
#define LONEJSON__OIDC_DEFAULT_MAX_USERINFO_RESPONSE_BYTES (1024u * 1024u)
#define LONEJSON__OIDC_DEFAULT_MAX_AUTH_URL_BYTES (16u * 1024u)
#define LONEJSON__OIDC_DEFAULT_MAX_CALLBACK_QUERY_BYTES (16u * 1024u)
#define LONEJSON__OIDC_PKCE_DEFAULT_VERIFIER_BYTES 32u
#define LONEJSON__OIDC_PKCE_MIN_VERIFIER_CHARS 43u
#define LONEJSON__OIDC_PKCE_MAX_VERIFIER_CHARS 128u

#ifdef LONEJSON_WITH_OIDC
void lonejson_http_response_init(lonejson_http_response *response) {
  if (response != NULL) {
    memset(response, 0, sizeof(*response));
    lonejson_owned_buffer_init(&response->body);
  }
}

void lonejson_http_response_cleanup(lonejson_http_response *response) {
  if (response == NULL) {
    return;
  }
  lonejson__owned_free(response->content_type);
  lonejson_owned_buffer_free(&response->body);
  lonejson_http_response_init(response);
}

lonejson_status
lonejson_http_provider_init(lonejson_http_provider *provider,
                            const lonejson_http_provider_config *config,
                            lonejson_error *error) {
  lonejson__clear_error(error);
  if (provider == NULL || config == NULL || config->request == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "HTTP provider config and request callback "
                               "are required");
  }
  memset(provider, 0, sizeof(*provider));
  provider->user_data = config->user_data;
  provider->user_agent = config->user_agent;
  provider->request = config->request;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_http_provider_init_simple(
    lonejson_http_provider *provider, void *user_data, const char *user_agent,
    lonejson_status (*request)(void *user_data,
                               const lonejson_http_request *request,
                               lonejson_http_response *response,
                               lonejson_error *error),
    lonejson_error *error) {
  lonejson_http_provider_config config;

  memset(&config, 0, sizeof(config));
  config.user_data = user_data;
  config.user_agent = user_agent;
  config.request = request;
  return lonejson_http_provider_init(provider, &config, error);
}

static lonejson_status
lonejson__http_request_validate(const lonejson_http_request *request,
                                lonejson_error *error) {
  if (request == NULL || request->method == NULL || request->url == NULL ||
      request->method[0] == '\0' || request->url[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "HTTP method and URL are required");
  }
  if (request->body == NULL && request->body_len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "HTTP request body is required");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__http_provider_request(
    lonejson *runtime, const lonejson_http_request *request,
    lonejson_http_response *response, lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson_http_request local_request;
  lonejson_status status;

  if (response == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "HTTP response output is required");
  }
  lonejson_http_response_cleanup(response);
  status = lonejson__http_request_validate(request, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (!runtime_state->has_http_provider ||
      runtime_state->http_provider.request == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "runtime HTTP provider is not configured");
  }
  local_request = *request;
  if (local_request.user_agent == NULL) {
    local_request.user_agent = runtime_state->http_provider.user_agent;
  }
  status = runtime_state->http_provider.request(
      runtime_state->http_provider.user_data, &local_request, response, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

static lonejson_status
lonejson__http_require_success(const lonejson_http_response *response,
                               const char *what, lonejson_error *error) {
  if (response == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "HTTP response is required");
  }
  if (response->status_code < 200 || response->status_code >= 300) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "%s returned HTTP status %ld", what,
                               response->status_code);
  }
  return LONEJSON_STATUS_OK;
}
#endif

typedef enum lonejson__jwt_claim_field {
  LONEJSON__JWT_FIELD_NONE = 0,
  LONEJSON__JWT_FIELD_HEADER_ALG,
  LONEJSON__JWT_FIELD_HEADER_KID,
  LONEJSON__JWT_FIELD_HEADER_TYP,
  LONEJSON__JWT_FIELD_HEADER_CRIT_ITEM,
  LONEJSON__JWT_FIELD_HEADER_X5T,
  LONEJSON__JWT_FIELD_HEADER_X5T_S256,
  LONEJSON__JWT_FIELD_HEADER_X5C_ITEM,
  LONEJSON__JWT_FIELD_ISS,
  LONEJSON__JWT_FIELD_SUB,
  LONEJSON__JWT_FIELD_NONCE,
  LONEJSON__JWT_FIELD_AZP,
  LONEJSON__JWT_FIELD_SCOPE,
  LONEJSON__JWT_FIELD_SCP_STRING,
  LONEJSON__JWT_FIELD_SCP_ARRAY_ITEM,
  LONEJSON__JWT_FIELD_AUD_STRING,
  LONEJSON__JWT_FIELD_AUD_ARRAY_ITEM,
  LONEJSON__JWT_FIELD_EXP,
  LONEJSON__JWT_FIELD_NBF,
  LONEJSON__JWT_FIELD_IAT
} lonejson__jwt_claim_field;

typedef struct lonejson__jwt_claim_visit {
  int header_mode;
  lonejson_jwt_header *header;
  lonejson_jwt_claims *claims;
  lonejson__jwt_claim_field active;
  char *buffer;
  size_t len;
  size_t capacity;
  int root_object_seen;
  int seen_alg;
  int seen_kid;
  int seen_typ;
  int seen_crit;
  int seen_x5t;
  int seen_x5t_s256;
  int seen_x5c;
  int seen_iss;
  int seen_sub;
  int seen_nonce;
  int seen_azp;
  int seen_scope;
  int seen_scp;
  int seen_aud;
  int seen_exp;
  int seen_nbf;
  int seen_iat;
} lonejson__jwt_claim_visit;

static lonejson_status lonejson__jwt_check_segment(const char *token,
                                                   size_t begin, size_t end,
                                                   int allow_empty,
                                                   lonejson_error *error) {
  size_t decoded_len;
  lonejson_status status;

  if (end < begin) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, begin, 0u,
                               0u, "invalid JWT segment bounds");
  }
  if (!allow_empty && end == begin) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, begin, 0u,
                               0u, "JWT compact segment must not be empty");
  }
  status = lonejson_base64_decoded_len(
      token + begin, end - begin, LONEJSON_BASE64_URL_RAW, &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL) {
      error->offset += begin;
    }
    return status;
  }
  (void)decoded_len;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_jwt_parse_compact(const char *token, size_t len,
                                           lonejson_jwt_compact *out,
                                           lonejson_error *error) {
  size_t i;
  size_t first_dot;
  size_t second_dot;
  int seen_first;
  int seen_second;
  lonejson_status status;

  lonejson__clear_error(error);
  if (token == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWT compact token and output are required");
  }
  memset(out, 0, sizeof(*out));
  first_dot = 0u;
  second_dot = 0u;
  seen_first = 0;
  seen_second = 0;
  for (i = 0u; i < len; ++i) {
    if (token[i] == '.') {
      if (!seen_first) {
        first_dot = i;
        seen_first = 1;
      } else if (!seen_second) {
        second_dot = i;
        seen_second = 1;
      } else {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                   0u,
                                   "JWT compact token has too many segments");
      }
    }
  }
  if (!seen_first || !seen_second) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, len, 0u, 0u,
                               "JWT compact token must contain three segments");
  }
  status = lonejson__jwt_check_segment(token, 0u, first_dot, 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      lonejson__jwt_check_segment(token, first_dot + 1u, second_dot, 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwt_check_segment(token, second_dot + 1u, len, 1, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  out->header.data = token;
  out->header.len = first_dot;
  out->payload.data = token + first_dot + 1u;
  out->payload.len = second_dot - first_dot - 1u;
  out->signature.data = token + second_dot + 1u;
  out->signature.len = len - second_dot - 1u;
  out->signing_input.data = token;
  out->signing_input.len = second_dot;
  return LONEJSON_STATUS_OK;
}

static int lonejson__auth_streq(const char *a, const char *b) {
  if (a == NULL || b == NULL) {
    return 0;
  }
  return strcmp(a, b) == 0;
}

static lonejson_status lonejson__jwk_require_member(const lonejson_jwk *jwk,
                                                    const char *value,
                                                    const char *member,
                                                    lonejson_error *error) {
  (void)jwk;
  if (value == NULL || value[0] == '\0') {
    char message[160];
    (void)snprintf(message, sizeof(message), "JWK %s member is required",
                   member);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               message);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwk_check_base64url_member(const char *value, const char *member,
                                     int required, lonejson_error *error) {
  size_t decoded_len;
  lonejson_status status;

  if (value == NULL) {
    if (!required) {
      return LONEJSON_STATUS_OK;
    }
    return lonejson__jwk_require_member(NULL, value, member, error);
  }
  if (required && value[0] == '\0') {
    return lonejson__jwk_require_member(NULL, value, member, error);
  }
  status = lonejson_base64_decoded_len(
      value, strlen(value), LONEJSON_BASE64_URL_RAW, &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL) {
      (void)snprintf(error->message, sizeof(error->message),
                     "invalid base64url in JWK %s member", member);
    }
    return status;
  }
  (void)decoded_len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwk_check_base64_member(const char *value, const char *member,
                                  lonejson_error *error) {
  size_t decoded_len;
  lonejson_status status;

  if (value == NULL) {
    return LONEJSON_STATUS_OK;
  }
  status = lonejson_base64_decoded_len(
      value, strlen(value), LONEJSON_BASE64_STANDARD, &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL) {
      (void)snprintf(error->message, sizeof(error->message),
                     "invalid base64 in JWK %s member", member);
    }
    return status;
  }
  (void)decoded_len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__jwk_validate(lonejson_jwk *jwk,
                                              lonejson_error *error) {
  lonejson_status status;
  size_t i;

  if (jwk == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWK object is required");
  }
  if (jwk->kty == NULL || jwk->kty[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "JWK kty member is required");
  }
  if (lonejson__auth_streq(jwk->kty, "RSA")) {
    status = lonejson__jwk_check_base64url_member(jwk->n, "n", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__jwk_check_base64url_member(jwk->e, "e", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  } else if (lonejson__auth_streq(jwk->kty, "EC")) {
    status = lonejson__jwk_require_member(jwk, jwk->crv, "crv", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__jwk_check_base64url_member(jwk->x, "x", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__jwk_check_base64url_member(jwk->y, "y", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  } else if (lonejson__auth_streq(jwk->kty, "oct")) {
    status = lonejson__jwk_check_base64url_member(jwk->k, "k", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  } else if (lonejson__auth_streq(jwk->kty, "OKP")) {
    status = lonejson__jwk_require_member(jwk, jwk->crv, "crv", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__jwk_check_base64url_member(jwk->x, "x", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  status = lonejson__jwk_check_base64url_member(jwk->n, "n", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwk_check_base64url_member(jwk->e, "e", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwk_check_base64url_member(jwk->x, "x", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwk_check_base64url_member(jwk->y, "y", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwk_check_base64url_member(jwk->k, "k", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwk_check_base64url_member(jwk->x5t, "x5t", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      lonejson__jwk_check_base64url_member(jwk->x5t_s256, "x5t#S256", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (i = 0u; i < jwk->x5c.count; ++i) {
    status = lonejson__jwk_check_base64_member(jwk->x5c.items[i], "x5c", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

void lonejson_jwk_init(lonejson_jwk *jwk) {
  if (jwk != NULL) {
    memset(jwk, 0, sizeof(*jwk));
  }
}

void lonejson_jwk_cleanup(lonejson_jwk *jwk) {
  if (jwk != NULL) {
    lonejson_cleanup(&lonejson__jwk_map, jwk, NULL);
    memset(jwk, 0, sizeof(*jwk));
  }
}

void lonejson_jwks_init(lonejson_jwks *jwks) {
  if (jwks != NULL) {
    memset(jwks, 0, sizeof(*jwks));
  }
}

void lonejson_jwks_cleanup(lonejson_jwks *jwks) {
  if (jwks != NULL) {
    lonejson_cleanup(&lonejson__jwks_map, jwks, NULL);
    memset(jwks, 0, sizeof(*jwks));
  }
}

lonejson_status lonejson_jwk_parse_json(lonejson *runtime, const char *json,
                                        size_t len, lonejson_jwk *out,
                                        lonejson_error *error) {
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWK output is required");
  }
  if (json == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWK JSON input is required");
  }
  lonejson_jwk_cleanup(out);
  status =
      lonejson_parse_buffer(runtime, &lonejson__jwk_map, out, json, len, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwk_cleanup(out);
    return status;
  }
  status = lonejson__jwk_validate(out, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwk_cleanup(out);
  }
  return status;
}

lonejson_status lonejson_jwks_parse_json(lonejson *runtime, const char *json,
                                         size_t len, lonejson_jwks *out,
                                         lonejson_error *error) {
  lonejson_status status;
  size_t i;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWKS output is required");
  }
  if (json == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWKS JSON input is required");
  }
  lonejson_jwks_cleanup(out);
  status = lonejson_parse_buffer(runtime, &lonejson__jwks_map, out, json, len,
                                 error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwks_cleanup(out);
    return status;
  }
  if (out->keys.count == 0u) {
    lonejson_jwks_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "JWKS keys array must not be empty");
  }
  for (i = 0u; i < out->keys.count; ++i) {
    lonejson_jwk *jwk = &((lonejson_jwk *)out->keys.items)[i];
    status = lonejson__jwk_validate(jwk, error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson_jwks_cleanup(out);
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static int lonejson__jwk_match_filter(const char *value, const char *filter) {
  if (filter == NULL) {
    return 1;
  }
  return value != NULL && strcmp(value, filter) == 0;
}

lonejson_status lonejson_jwks_select(const lonejson_jwks *jwks,
                                     const lonejson_jwk_select_options *options,
                                     const lonejson_jwk **out,
                                     lonejson_error *error) {
  size_t i;

  lonejson__clear_error(error);
  if (jwks == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWKS and output pointer are required");
  }
  *out = NULL;
  for (i = 0u; i < jwks->keys.count; ++i) {
    const lonejson_jwk *jwk = &((const lonejson_jwk *)jwks->keys.items)[i];
    if (options != NULL &&
        (!lonejson__jwk_match_filter(jwk->kid, options->kid) ||
         !lonejson__jwk_match_filter(jwk->kty, options->kty) ||
         !lonejson__jwk_match_filter(jwk->alg, options->alg) ||
         !lonejson__jwk_match_filter(jwk->use, options->use))) {
      continue;
    }
    *out = jwk;
    return LONEJSON_STATUS_OK;
  }
  return LONEJSON_STATUS_OK;
}

#ifdef LONEJSON_WITH_OIDC
static int lonejson__oidc_find_https_authority_end(const char *url, size_t len,
                                                   int allow_query_fragment,
                                                   size_t *authority_end) {
  size_t i;
  size_t authority_start = 8u;

  if (url == NULL || len <= authority_start ||
      memcmp(url, "https://", authority_start) != 0) {
    return 0;
  }
  if (url[authority_start] == '/' || url[authority_start] == '?' ||
      url[authority_start] == '#') {
    return 0;
  }
  for (i = authority_start; i < len; ++i) {
    if (url[i] == '/' || url[i] == '?' || url[i] == '#') {
      break;
    }
  }
  if (i == authority_start) {
    return 0;
  }
  if (!allow_query_fragment) {
    size_t j;
    for (j = i; j < len; ++j) {
      if (url[j] == '?' || url[j] == '#') {
        return 0;
      }
    }
  }
  if (authority_end != NULL) {
    *authority_end = i;
  }
  return 1;
}

static lonejson_status
lonejson__oidc_require_https_url(const char *value, const char *member,
                                 int allow_query_fragment,
                                 lonejson_error *error) {
  if (value == NULL || value[0] == '\0' ||
      !lonejson__oidc_find_https_authority_end(value, strlen(value),
                                               allow_query_fragment, NULL)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "OIDC %s must be an https URL with a host",
                               member);
  }
  return LONEJSON_STATUS_OK;
}

void lonejson_oidc_discovery_init(lonejson_oidc_discovery *discovery) {
  if (discovery != NULL) {
    memset(discovery, 0, sizeof(*discovery));
  }
}

void lonejson_oidc_discovery_cleanup(lonejson_oidc_discovery *discovery) {
  if (discovery != NULL) {
    lonejson_cleanup(&lonejson__oidc_discovery_map, discovery, NULL);
    memset(discovery, 0, sizeof(*discovery));
  }
}

lonejson_status lonejson_oidc_discovery_url(const char *issuer,
                                            lonejson_owned_buffer *out,
                                            lonejson_error *error) {
  static const char well_known[] = "/.well-known/openid-configuration";
  size_t issuer_len;
  size_t authority_end;
  size_t trim_len;
  size_t total_len;
  lonejson_allocator allocator;
  char *data;

  lonejson__clear_error(error);
  if (issuer == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC issuer and output are required");
  }
  issuer_len = strlen(issuer);
  if (!lonejson__oidc_find_https_authority_end(issuer, issuer_len, 0,
                                               &authority_end)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "OIDC issuer must be an https URL with a host");
  }
  trim_len = issuer_len;
  while (trim_len > authority_end && issuer[trim_len - 1u] == '/') {
    --trim_len;
  }
  if (trim_len > SIZE_MAX - (sizeof(well_known) - 1u) - 1u) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "OIDC discovery URL is too large");
  }
  total_len = trim_len + (sizeof(well_known) - 1u);
  allocator = lonejson_default_allocator();
  data = (char *)lonejson__buffer_alloc(&allocator, total_len + 1u);
  if (data == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate OIDC discovery URL");
  }
  memcpy(data, issuer, trim_len);
  memcpy(data + trim_len, well_known, sizeof(well_known) - 1u);
  data[total_len] = '\0';
  lonejson_owned_buffer_free(out);
  out->data = data;
  out->len = total_len;
  out->alloc_size = total_len + 1u;
  out->allocator = allocator;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oidc_discovery_parse_json(lonejson *runtime,
                                                   const char *json, size_t len,
                                                   lonejson_oidc_discovery *out,
                                                   lonejson_error *error) {
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC discovery output is required");
  }
  if (json == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC discovery JSON input is required");
  }
  lonejson_oidc_discovery_cleanup(out);
  status = lonejson_parse_buffer(runtime, &lonejson__oidc_discovery_map, out,
                                 json, len, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_discovery_cleanup(out);
    return status;
  }
  status = lonejson__oidc_require_https_url(out->issuer, "issuer", 0, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_require_https_url(out->token_endpoint,
                                              "token_endpoint", 1, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status =
        lonejson__oidc_require_https_url(out->jwks_uri, "jwks_uri", 1, error);
  }
  if (status == LONEJSON_STATUS_OK && out->authorization_endpoint != NULL) {
    status = lonejson__oidc_require_https_url(
        out->authorization_endpoint, "authorization_endpoint", 1, error);
  }
  if (status == LONEJSON_STATUS_OK && out->introspection_endpoint != NULL) {
    status = lonejson__oidc_require_https_url(
        out->introspection_endpoint, "introspection_endpoint", 1, error);
  }
  if (status == LONEJSON_STATUS_OK && out->revocation_endpoint != NULL) {
    status = lonejson__oidc_require_https_url(out->revocation_endpoint,
                                              "revocation_endpoint", 1, error);
  }
  if (status == LONEJSON_STATUS_OK && out->userinfo_endpoint != NULL) {
    status = lonejson__oidc_require_https_url(out->userinfo_endpoint,
                                              "userinfo_endpoint", 1, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_discovery_cleanup(out);
  }
  return status;
}

lonejson_status lonejson_oidc_discovery_validate_issuer(
    const lonejson_oidc_discovery *discovery, const char *expected_issuer,
    lonejson_error *error) {
  lonejson__clear_error(error);
  if (discovery == NULL || expected_issuer == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OIDC discovery and expected issuer are required");
  }
  if (!lonejson__oidc_find_https_authority_end(
          expected_issuer, strlen(expected_issuer), 0, NULL)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
        "OIDC expected issuer must be an https URL with a host");
  }
  if (discovery->issuer == NULL ||
      strcmp(discovery->issuer, expected_issuer) != 0) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
        "OIDC discovery issuer does not match expected issuer");
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oidc_fetch_discovery(lonejson *runtime,
                                              const char *issuer,
                                              size_t max_response_bytes,
                                              lonejson_oidc_discovery *out,
                                              lonejson_error *error) {
  lonejson_owned_buffer url;
  lonejson_http_request request;
  lonejson_http_response response;
  size_t max_bytes;
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC discovery output is required");
  }
  max_bytes = max_response_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_DISCOVERY_BYTES
                  : max_response_bytes;
  lonejson_owned_buffer_init(&url);
  lonejson_http_response_init(&response);
  status = lonejson_oidc_discovery_url(issuer, &url, error);
  if (status == LONEJSON_STATUS_OK) {
    memset(&request, 0, sizeof(request));
    request.method = "GET";
    request.url = url.data;
    request.max_response_bytes = max_bytes;
    status =
        lonejson__http_provider_request(runtime, &request, &response, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__http_require_success(&response, "OIDC discovery", error);
  }
  if (status == LONEJSON_STATUS_OK && response.body.len > max_bytes) {
    status = lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, response.body.len, 0u, 0u,
        "OIDC discovery response exceeds configured limit");
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oidc_discovery_parse_json(runtime, response.body.data,
                                                response.body.len, out, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oidc_discovery_validate_issuer(out, issuer, error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson_oidc_discovery_cleanup(out);
    }
  }
  lonejson_http_response_cleanup(&response);
  lonejson_owned_buffer_free(&url);
  return status;
}

static lonejson_status lonejson__oidc_jwks_cache_validate_policy(
    const lonejson_oidc_jwks_cache_policy *policy, lonejson_error *error) {
  lonejson_int64 max_i64 = (lonejson_int64)(LONEJSON_UINT64_MAX >> 1);

  if (policy == NULL || policy->issuer == NULL || policy->jwks_uri == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OIDC JWKS cache policy, issuer, and jwks_uri are required");
  }
  if (policy->ttl_seconds <= 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OIDC JWKS cache ttl_seconds must be positive");
  }
  if (policy->now < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC JWKS cache now must be non-negative");
  }
  if (policy->now > 0 && policy->ttl_seconds > max_i64 - policy->now) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "OIDC JWKS cache expiry overflows");
  }
  if (lonejson__oidc_require_https_url(policy->issuer, "issuer", 0, error) !=
      LONEJSON_STATUS_OK) {
    return error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
  }
  return lonejson__oidc_require_https_url(policy->jwks_uri, "jwks_uri", 1,
                                          error);
}

static int lonejson__oidc_jwks_cache_matches_policy(
    const lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy) {
  return cache != NULL && policy != NULL && cache->has_jwks &&
         cache->issuer != NULL && cache->jwks_uri != NULL &&
         strcmp(cache->issuer, policy->issuer) == 0 &&
         strcmp(cache->jwks_uri, policy->jwks_uri) == 0 &&
         policy->now < cache->expires_at;
}

void lonejson_oidc_jwks_cache_init(lonejson_oidc_jwks_cache *cache) {
  if (cache != NULL) {
    memset(cache, 0, sizeof(*cache));
  }
}

void lonejson_oidc_jwks_cache_cleanup(lonejson_oidc_jwks_cache *cache) {
  if (cache != NULL) {
    lonejson__owned_free(cache->issuer);
    lonejson__owned_free(cache->jwks_uri);
    lonejson_jwks_cleanup(&cache->jwks);
    memset(cache, 0, sizeof(*cache));
  }
}

static int lonejson__oauth2_form_is_unreserved(unsigned char ch) {
  return (ch >= (unsigned char)'A' && ch <= (unsigned char)'Z') ||
         (ch >= (unsigned char)'a' && ch <= (unsigned char)'z') ||
         (ch >= (unsigned char)'0' && ch <= (unsigned char)'9') ||
         ch == (unsigned char)'-' || ch == (unsigned char)'.' ||
         ch == (unsigned char)'_' || ch == (unsigned char)'~';
}

static lonejson_status
lonejson__oauth2_form_append_raw(lonejson_owned_buffer *out, const char *data,
                                 size_t len, size_t max_bytes,
                                 lonejson_error *error) {
  if (out->len > max_bytes || len > max_bytes - out->len) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "OAuth2 form body exceeds configured limit");
  }
  return lonejson_owned_buffer_sink(out, data, len, error);
}

static lonejson_status
lonejson__oauth2_form_append_component(lonejson_owned_buffer *out,
                                       const char *value, size_t max_bytes,
                                       lonejson_error *error) {
  static const char hex[] = "0123456789ABCDEF";
  unsigned char ch;
  char encoded[3];
  lonejson_status status;

  while (*value != '\0') {
    ch = (unsigned char)*value++;
    if (lonejson__oauth2_form_is_unreserved(ch)) {
      status = lonejson__oauth2_form_append_raw(out, (const char *)&ch, 1u,
                                                max_bytes, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    } else if (ch == (unsigned char)' ') {
      status = lonejson__oauth2_form_append_raw(out, "+", 1u, max_bytes, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    } else {
      encoded[0] = '%';
      encoded[1] = hex[(ch >> 4u) & 0x0fu];
      encoded[2] = hex[ch & 0x0fu];
      status = lonejson__oauth2_form_append_raw(out, encoded, sizeof(encoded),
                                                max_bytes, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__oauth2_form_append_pair(lonejson_owned_buffer *out, const char *key,
                                  const char *value, int *has_pair,
                                  size_t max_bytes, lonejson_error *error) {
  lonejson_status status;

  if (value == NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (*has_pair) {
    status = lonejson__oauth2_form_append_raw(out, "&", 1u, max_bytes, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  if (key == NULL || key[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OAuth2 form parameter name is required");
  }
  status = lonejson__oauth2_form_append_component(out, key, max_bytes, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_raw(out, "=", 1u, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status =
        lonejson__oauth2_form_append_component(out, value, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    *has_pair = 1;
  }
  return status;
}

static int lonejson__ascii_case_equal(const char *a, const char *b) {
  unsigned char ca;
  unsigned char cb;

  if (a == NULL || b == NULL) {
    return 0;
  }
  while (*a != '\0' && *b != '\0') {
    ca = (unsigned char)*a++;
    cb = (unsigned char)*b++;
    if (ca >= (unsigned char)'A' && ca <= (unsigned char)'Z') {
      ca = (unsigned char)(ca - (unsigned char)'A' + (unsigned char)'a');
    }
    if (cb >= (unsigned char)'A' && cb <= (unsigned char)'Z') {
      cb = (unsigned char)(cb - (unsigned char)'A' + (unsigned char)'a');
    }
    if (ca != cb) {
      return 0;
    }
  }
  return *a == '\0' && *b == '\0';
}

lonejson_status lonejson_oauth2_client_credentials_body(
    const lonejson_oauth2_client_credentials *request,
    lonejson_owned_buffer *out, lonejson_error *error) {
  size_t max_bytes;
  lonejson_status status;
  int has_pair = 0;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OAuth2 client credentials request is required");
  }
  if (request->client_id == NULL || request->client_id[0] == '\0' ||
      request->client_secret == NULL || request->client_secret[0] == '\0') {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OAuth2 client_id and client_secret are required");
  }
  lonejson_owned_buffer_free(out);
  max_bytes = request->max_body_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_FORM_BODY_BYTES
                  : request->max_body_bytes;
  status = lonejson__oauth2_form_append_pair(
      out, "grant_type", "client_credentials", &has_pair, max_bytes, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "client_id", request->client_id, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "client_secret",
                                               request->client_secret,
                                               &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "scope", request->scope,
                                               &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "audience", request->audience, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "resource", request->resource, &has_pair, max_bytes, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(out);
  }
  return status;
}

lonejson_status
lonejson_oauth2_refresh_token_body(const lonejson_oauth2_refresh_token *request,
                                   lonejson_owned_buffer *out,
                                   lonejson_error *error) {
  size_t max_bytes;
  lonejson_status status;
  int has_pair = 0;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OAuth2 refresh token request is required");
  }
  if (request->refresh_token == NULL || request->refresh_token[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OAuth2 refresh_token is required");
  }
  if (request->client_secret != NULL &&
      (request->client_id == NULL || request->client_id[0] == '\0')) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OAuth2 client_id is required with "
                               "client_secret");
  }
  lonejson_owned_buffer_free(out);
  max_bytes = request->max_body_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_FORM_BODY_BYTES
                  : request->max_body_bytes;
  status = lonejson__oauth2_form_append_pair(out, "grant_type", "refresh_token",
                                             &has_pair, max_bytes, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "refresh_token",
                                               request->refresh_token,
                                               &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "client_id", request->client_id, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "client_secret",
                                               request->client_secret,
                                               &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "scope", request->scope,
                                               &has_pair, max_bytes, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(out);
  }
  return status;
}

static lonejson_status lonejson__oauth2_token_admin_body(
    const char *kind, const char *token, const char *token_type_hint,
    const char *client_id, const char *client_secret, size_t max_body_bytes,
    lonejson_owned_buffer *out, lonejson_error *error) {
  size_t max_bytes;
  lonejson_status status;
  int has_pair = 0;

  if (token == NULL || token[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, kind);
  }
  if (client_secret != NULL && (client_id == NULL || client_id[0] == '\0')) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OAuth2 client_id is required with "
                               "client_secret");
  }
  lonejson_owned_buffer_free(out);
  max_bytes = max_body_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_FORM_BODY_BYTES
                  : max_body_bytes;
  status = lonejson__oauth2_form_append_pair(out, "token", token, &has_pair,
                                             max_bytes, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "token_type_hint", token_type_hint, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "client_id", client_id,
                                               &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "client_secret", client_secret, &has_pair, max_bytes, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(out);
  }
  return status;
}

lonejson_status lonejson_oauth2_token_introspection_body(
    const lonejson_oauth2_token_introspection *request,
    lonejson_owned_buffer *out, lonejson_error *error) {
  lonejson__clear_error(error);
  if (request == NULL || out == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OAuth2 token introspection request is required");
  }
  return lonejson__oauth2_token_admin_body(
      "OAuth2 introspection token is required", request->token,
      request->token_type_hint,
      request->use_basic_auth ? NULL : request->client_id,
      request->use_basic_auth ? NULL : request->client_secret,
      request->max_body_bytes, out, error);
}

lonejson_status lonejson_oauth2_token_revocation_body(
    const lonejson_oauth2_token_revocation *request, lonejson_owned_buffer *out,
    lonejson_error *error) {
  lonejson__clear_error(error);
  if (request == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OAuth2 token revocation request is required");
  }
  return lonejson__oauth2_token_admin_body(
      "OAuth2 revocation token is required", request->token,
      request->token_type_hint,
      request->use_basic_auth ? NULL : request->client_id,
      request->use_basic_auth ? NULL : request->client_secret,
      request->max_body_bytes, out, error);
}

lonejson_status lonejson_oidc_authorization_code_token_body(
    const lonejson_oidc_authorization_code_token *request,
    lonejson_owned_buffer *out, lonejson_error *error) {
  size_t max_bytes;
  lonejson_status status;
  int has_pair = 0;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OIDC authorization code token request is "
                               "required");
  }
  if (request->client_id == NULL || request->client_id[0] == '\0' ||
      request->code == NULL || request->code[0] == '\0' ||
      request->redirect_uri == NULL || request->redirect_uri[0] == '\0' ||
      request->code_verifier == NULL || request->code_verifier[0] == '\0') {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "client_id, code, redirect_uri, and code_verifier are required");
  }
  lonejson_owned_buffer_free(out);
  max_bytes = request->max_body_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_FORM_BODY_BYTES
                  : request->max_body_bytes;
  status = lonejson__oauth2_form_append_pair(
      out, "grant_type", "authorization_code", &has_pair, max_bytes, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "client_id", request->client_id, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "code", request->code,
                                               &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "redirect_uri",
                                               request->redirect_uri, &has_pair,
                                               max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "code_verifier",
                                               request->code_verifier,
                                               &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "client_secret",
                                               request->client_secret,
                                               &has_pair, max_bytes, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(out);
  }
  return status;
}

void lonejson_oauth2_token_response_init(
    lonejson_oauth2_token_response *response) {
  if (response != NULL) {
    memset(response, 0, sizeof(*response));
  }
}

void lonejson_oauth2_token_response_cleanup(
    lonejson_oauth2_token_response *response) {
  if (response != NULL) {
    lonejson_cleanup(&lonejson__oauth2_token_response_map, response, NULL);
    memset(response, 0, sizeof(*response));
  }
}

static char *lonejson__base64url_encode_alloc(const unsigned char *data,
                                              size_t len,
                                              lonejson_error *error) {
  size_t out_len;
  char *out;
  size_t needed;
  lonejson_status status;

  status = lonejson_base64_encoded_len(len, LONEJSON_BASE64_URL_RAW, &out_len,
                                       error);
  if (status != LONEJSON_STATUS_OK) {
    return NULL;
  }
  out = (char *)lonejson__owned_malloc(NULL, out_len + 1u);
  if (out == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate base64url output");
    return NULL;
  }
  status = lonejson_base64_encode(data, len, LONEJSON_BASE64_URL_RAW, out,
                                  out_len, &needed, error);
  if (status != LONEJSON_STATUS_OK || needed != out_len) {
    lonejson__owned_free(out);
    return NULL;
  }
  out[out_len] = '\0';
  return out;
}

static int lonejson__oidc_pkce_verifier_char(unsigned char ch) {
  return lonejson__oauth2_form_is_unreserved(ch);
}

static lonejson_status
lonejson__oidc_pkce_validate_verifier(const char *code_verifier,
                                      lonejson_error *error) {
  size_t len;
  const unsigned char *p;

  if (code_verifier == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "PKCE code_verifier is required");
  }
  len = strlen(code_verifier);
  if (len < LONEJSON__OIDC_PKCE_MIN_VERIFIER_CHARS ||
      len > LONEJSON__OIDC_PKCE_MAX_VERIFIER_CHARS) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "PKCE code_verifier length is invalid");
  }
  for (p = (const unsigned char *)code_verifier; *p != '\0'; ++p) {
    if (!lonejson__oidc_pkce_verifier_char(*p)) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                 0u, 0u,
                                 "PKCE code_verifier contains invalid chars");
    }
  }
  return LONEJSON_STATUS_OK;
}

void lonejson_oidc_pkce_init(lonejson_oidc_pkce *pkce) {
  if (pkce != NULL) {
    memset(pkce, 0, sizeof(*pkce));
  }
}

void lonejson_oidc_pkce_cleanup(lonejson_oidc_pkce *pkce) {
  if (pkce != NULL) {
    lonejson__owned_free(pkce->code_verifier);
    lonejson__owned_free(pkce->code_challenge);
    memset(pkce, 0, sizeof(*pkce));
  }
}

static lonejson_status lonejson__oidc_pkce_provider(
    lonejson *runtime, int need_random, lonejson__runtime_borrow *borrow,
    lonejson_auth_provider *fallback, const lonejson_auth_provider **provider,
    lonejson_error *error) {
  const lonejson_runtime *runtime_state;
  const lonejson_auth_provider *candidate;

  borrow->runtime = NULL;
  borrow->handle = NULL;
  *provider = NULL;
  candidate = NULL;
  if (runtime != NULL) {
    runtime_state = lonejson__require_runtime_borrow(runtime, borrow, error);
    if (runtime_state == NULL) {
      return LONEJSON_STATUS_INVALID_ARGUMENT;
    }
    if (runtime_state->has_auth_provider) {
      candidate = &runtime_state->auth_provider;
    }
  }
#if defined(LONEJSON_INTERNAL_OPENSSL_IMPLEMENTATION)
  if (candidate == NULL) {
    if (lonejson_auth_provider_init_openssl(fallback, NULL, error) !=
        LONEJSON_STATUS_OK) {
      lonejson__runtime_borrow_release(borrow);
      return error != NULL ? error->code : LONEJSON_STATUS_INTERNAL_ERROR;
    }
    candidate = fallback;
  }
#else
  (void)fallback;
#endif
  if (candidate == NULL) {
    lonejson__runtime_borrow_release(borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "PKCE requires an auth crypto provider");
  }
  if (candidate->sha256 == NULL) {
    lonejson__runtime_borrow_release(borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "PKCE challenge requires auth provider sha256");
  }
  if (need_random && candidate->random_bytes == NULL) {
    lonejson__runtime_borrow_release(borrow);
    return lonejson__set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
        "PKCE verifier generation requires auth provider random_bytes");
  }
  *provider = candidate;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__oidc_pkce_challenge_with_provider(
    const lonejson_auth_provider *provider, const char *code_verifier,
    lonejson_owned_buffer *out, lonejson_error *error) {
  unsigned char digest[32];
  char *encoded;
  lonejson_status status;

  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "PKCE challenge output is required");
  }
  status = lonejson__oidc_pkce_validate_verifier(code_verifier, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  lonejson_owned_buffer_free(out);
  if (provider == NULL || provider->sha256 == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "PKCE challenge requires auth provider sha256");
  }
  status = provider->sha256(provider->user_data, code_verifier,
                            strlen(code_verifier), digest, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  encoded = lonejson__base64url_encode_alloc(digest, sizeof(digest), error);
  if (encoded == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
  }
  status = lonejson_owned_buffer_sink(out, encoded, strlen(encoded), error);
  lonejson__owned_free(encoded);
  return status;
}

lonejson_status lonejson_oidc_pkce_challenge_with_runtime(
    lonejson *runtime, const char *code_verifier, lonejson_owned_buffer *out,
    lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  lonejson_auth_provider fallback;
  const lonejson_auth_provider *provider;
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "PKCE challenge output is required");
  }
  status = lonejson__oidc_pkce_validate_verifier(code_verifier, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__oidc_pkce_provider(runtime, 0, &borrow, &fallback,
                                        &provider, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_pkce_challenge_with_provider(
        provider, code_verifier, out, error);
  }
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

lonejson_status lonejson_oidc_pkce_challenge(const char *code_verifier,
                                             lonejson_owned_buffer *out,
                                             lonejson_error *error) {
  return lonejson_oidc_pkce_challenge_with_runtime(NULL, code_verifier, out,
                                                   error);
}

lonejson_status lonejson_oidc_pkce_generate_with_runtime(
    lonejson *runtime, size_t verifier_bytes, lonejson_oidc_pkce *out,
    lonejson_error *error) {
  unsigned char random_bytes[96];
  lonejson_owned_buffer challenge;
  lonejson__runtime_borrow borrow;
  lonejson_auth_provider fallback;
  const lonejson_auth_provider *provider;
  size_t bytes;
  char *verifier;
  char *challenge_copy;
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "PKCE output is required");
  }
  bytes = verifier_bytes == 0u ? LONEJSON__OIDC_PKCE_DEFAULT_VERIFIER_BYTES
                               : verifier_bytes;
  if (bytes < 32u || bytes > sizeof(random_bytes)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "PKCE verifier entropy bytes must be 32..96");
  }
  status = lonejson__oidc_pkce_provider(runtime, 1, &borrow, &fallback,
                                        &provider, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      provider->random_bytes(provider->user_data, random_bytes, bytes, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__runtime_borrow_release(&borrow);
    return status;
  }
  verifier = lonejson__base64url_encode_alloc(random_bytes, bytes, error);
  if (verifier == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
  }
  lonejson_owned_buffer_init(&challenge);
  status = lonejson__oidc_pkce_challenge_with_provider(provider, verifier,
                                                       &challenge, error);
  lonejson__runtime_borrow_release(&borrow);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__owned_free(verifier);
    return status;
  }
  challenge_copy = lonejson__owned_strdup(NULL, challenge.data);
  lonejson_owned_buffer_free(&challenge);
  if (challenge_copy == NULL) {
    lonejson__owned_free(verifier);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate PKCE challenge");
  }
  lonejson_oidc_pkce_cleanup(out);
  out->code_verifier = verifier;
  out->code_challenge = challenge_copy;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oidc_pkce_generate(size_t verifier_bytes,
                                            lonejson_oidc_pkce *out,
                                            lonejson_error *error) {
  return lonejson_oidc_pkce_generate_with_runtime(NULL, verifier_bytes, out,
                                                  error);
}

lonejson_status lonejson_oauth2_token_response_parse_json(
    lonejson *runtime, const char *json, size_t len, size_t max_response_bytes,
    lonejson_oauth2_token_response *out, lonejson_error *error) {
  lonejson_status status;
  size_t max_bytes;

  lonejson__clear_error(error);
  if (out == NULL || (json == NULL && len != 0u)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OAuth2 token response JSON and output are required");
  }
  max_bytes = max_response_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_TOKEN_RESPONSE_BYTES
                  : max_response_bytes;
  if (len > max_bytes) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, len, 0u, 0u,
        "OAuth2 token response exceeds configured limit");
  }
  lonejson_oauth2_token_response_cleanup(out);
  status = lonejson_parse_buffer(runtime, &lonejson__oauth2_token_response_map,
                                 out, json, len, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oauth2_token_response_cleanup(out);
    return status;
  }
  if (out->error != NULL) {
    lonejson_oauth2_token_response_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "OAuth2 token endpoint returned an error");
  }
  if (out->access_token == NULL || out->access_token[0] == '\0' ||
      out->token_type == NULL || out->token_type[0] == '\0') {
    lonejson_oauth2_token_response_cleanup(out);
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
        "OAuth2 token response requires access_token and token_type");
  }
  if (!lonejson__ascii_case_equal(out->token_type, "Bearer")) {
    lonejson_oauth2_token_response_cleanup(out);
    return lonejson__set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
        "OAuth2 token response token_type is not Bearer");
  }
  if (out->has_expires_in && out->expires_in < 0) {
    lonejson_oauth2_token_response_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "OAuth2 token response expires_in is negative");
  }
  return LONEJSON_STATUS_OK;
}

void lonejson_oauth2_introspection_response_init(
    lonejson_oauth2_introspection_response *response) {
  if (response != NULL) {
    memset(response, 0, sizeof(*response));
  }
}

void lonejson_oauth2_introspection_response_cleanup(
    lonejson_oauth2_introspection_response *response) {
  if (response != NULL) {
    lonejson__owned_free(response->aud);
    response->aud = NULL;
    lonejson_cleanup(&lonejson__oauth2_introspection_response_map, response,
                     NULL);
    memset(response, 0, sizeof(*response));
  }
}

typedef struct lonejson__oauth2_introspection_aud_visit {
  char *buffer;
  size_t len;
  size_t capacity;
  int active;
  int seen;
  char **aud;
} lonejson__oauth2_introspection_aud_visit;

static int lonejson__oauth2_path_is_aud(const lonejson_value_path *path) {
  static const char name[] = "aud";
  return path != NULL && path->segment_count == 1u &&
         path->segments[0].len == sizeof(name) - 1u &&
         memcmp(path->segments[0].data, name, sizeof(name) - 1u) == 0;
}

static lonejson_status
lonejson__oauth2_aud_append(lonejson__oauth2_introspection_aud_visit *state,
                            const char *data, size_t len,
                            lonejson_error *error) {
  char *next;
  size_t next_cap;

  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (state->len > SIZE_MAX - len - 1u) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "OAuth2 introspection aud is too large");
  }
  if (state->len + len + 1u > state->capacity) {
    next_cap = state->capacity == 0u ? 32u : state->capacity;
    while (next_cap < state->len + len + 1u) {
      if (next_cap > SIZE_MAX / 2u) {
        next_cap = state->len + len + 1u;
        break;
      }
      next_cap *= 2u;
    }
    next = (char *)lonejson__owned_realloc(NULL, state->buffer, next_cap);
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u,
                                 "failed to allocate OAuth2 introspection aud");
    }
    state->buffer = next;
    state->capacity = next_cap;
  }
  memcpy(state->buffer + state->len, data, len);
  state->len += len;
  state->buffer[state->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__oauth2_aud_string_begin(void *user, const lonejson_value_path *path,
                                  lonejson_error *error) {
  lonejson__oauth2_introspection_aud_visit *state =
      (lonejson__oauth2_introspection_aud_visit *)user;

  if (!lonejson__oauth2_path_is_aud(path)) {
    return LONEJSON_STATUS_OK;
  }
  if (state->seen) {
    return lonejson__set_error(error, LONEJSON_STATUS_DUPLICATE_FIELD, 0u, 0u,
                               0u, "duplicate OAuth2 introspection aud");
  }
  state->active = 1;
  state->len = 0u;
  if (state->buffer != NULL) {
    state->buffer[0] = '\0';
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__oauth2_aud_string_chunk(void *user, const lonejson_value_path *path,
                                  const char *data, size_t len,
                                  lonejson_error *error) {
  lonejson__oauth2_introspection_aud_visit *state =
      (lonejson__oauth2_introspection_aud_visit *)user;
  (void)path;
  if (!state->active) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__oauth2_aud_append(state, data, len, error);
}

static lonejson_status
lonejson__oauth2_aud_string_end(void *user, const lonejson_value_path *path,
                                lonejson_error *error) {
  lonejson__oauth2_introspection_aud_visit *state =
      (lonejson__oauth2_introspection_aud_visit *)user;
  char *copy;
  (void)path;

  if (!state->active) {
    return LONEJSON_STATUS_OK;
  }
  state->active = 0;
  state->seen = 1;
  if (state->buffer == NULL) {
    state->buffer = (char *)lonejson__owned_malloc(NULL, 1u);
    if (state->buffer == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u,
                                 "failed to allocate OAuth2 introspection aud");
    }
    state->buffer[0] = '\0';
    state->capacity = 1u;
  }
  copy = state->buffer;
  state->buffer = NULL;
  state->len = 0u;
  state->capacity = 0u;
  lonejson__owned_free(*state->aud);
  *state->aud = copy;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__oauth2_introspection_capture_aud(
    lonejson *runtime, const char *json, size_t len,
    lonejson_oauth2_introspection_response *out, lonejson_error *error) {
  lonejson__oauth2_introspection_aud_visit state;
  lonejson_path_value_visitor visitor;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  memset(&visitor, 0, sizeof(visitor));
  state.aud = &out->aud;
  visitor.string_begin = lonejson__oauth2_aud_string_begin;
  visitor.string_chunk = lonejson__oauth2_aud_string_chunk;
  visitor.string_end = lonejson__oauth2_aud_string_end;
  status = lonejson_visit_path_value_buffer(runtime, json, len, &visitor,
                                            &state, error);
  lonejson__owned_free(state.buffer);
  return status;
}

lonejson_status lonejson_oauth2_introspection_response_parse_json(
    lonejson *runtime, const char *json, size_t len, size_t max_response_bytes,
    lonejson_oauth2_introspection_response *out, lonejson_error *error) {
  lonejson_status status;
  size_t max_bytes;

  lonejson__clear_error(error);
  if (out == NULL || (json == NULL && len != 0u)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OAuth2 introspection response JSON and output are required");
  }
  max_bytes = max_response_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_INTROSPECTION_RESPONSE_BYTES
                  : max_response_bytes;
  if (len > max_bytes) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, len, 0u, 0u,
        "OAuth2 introspection response exceeds configured limit");
  }
  lonejson_oauth2_introspection_response_cleanup(out);
  status = lonejson_parse_buffer(runtime,
                                 &lonejson__oauth2_introspection_response_map,
                                 out, json, len, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oauth2_introspection_response_cleanup(out);
    return status;
  }
  if (!out->has_active) {
    lonejson_oauth2_introspection_response_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "OAuth2 introspection response requires active");
  }
  status = lonejson__oauth2_introspection_capture_aud(runtime, json, len, out,
                                                      error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oauth2_introspection_response_cleanup(out);
    return status;
  }
  return LONEJSON_STATUS_OK;
}

void lonejson_oidc_userinfo_response_init(
    lonejson_oidc_userinfo_response *out) {
  if (out != NULL) {
    memset(out, 0, sizeof(*out));
  }
}

void lonejson_oidc_userinfo_response_cleanup(
    lonejson_oidc_userinfo_response *out) {
  if (out != NULL) {
    lonejson__owned_free(out->json);
    out->json = NULL;
    lonejson_cleanup(&lonejson__oidc_userinfo_known_map, out, NULL);
    memset(out, 0, sizeof(*out));
  }
}

lonejson_status lonejson_oidc_userinfo_response_parse_json(
    lonejson *runtime, const char *json, size_t len, size_t max_response_bytes,
    lonejson_oidc_userinfo_response *out, lonejson_error *error) {
  lonejson_status status;
  size_t max_bytes;
  char *copy;

  lonejson__clear_error(error);
  if (out == NULL || (json == NULL && len != 0u)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OIDC UserInfo response JSON and output are required");
  }
  max_bytes = max_response_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_USERINFO_RESPONSE_BYTES
                  : max_response_bytes;
  if (len > max_bytes) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, len, 0u, 0u,
        "OIDC UserInfo response exceeds configured limit");
  }
  lonejson_oidc_userinfo_response_cleanup(out);
  status = lonejson_validate_buffer(runtime, json, len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_parse_buffer(runtime, &lonejson__oidc_userinfo_known_map,
                                 out, json, len, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_userinfo_response_cleanup(out);
    return status;
  }
  copy = (char *)lonejson__owned_malloc(NULL, len + 1u);
  if (copy == NULL) {
    lonejson_oidc_userinfo_response_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to copy OIDC UserInfo JSON");
  }
  if (len != 0u) {
    memcpy(copy, json, len);
  }
  copy[len] = '\0';
  out->json = copy;
  out->len = len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__oauth2_token_endpoint_request(
    lonejson *runtime, const char *token_endpoint, const void *request_options,
    lonejson_status (*body_fn)(const void *, lonejson_owned_buffer *,
                               lonejson_error *),
    size_t max_response_bytes, lonejson_http_response *response,
    lonejson_error *error) {
  lonejson_owned_buffer body;
  lonejson_http_request request;
  size_t max_bytes;
  lonejson_status status;

  lonejson__clear_error(error);
  if (response == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OAuth2 HTTP response output is required");
  }
  if (body_fn == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OAuth2 token request body builder is "
                               "required");
  }
  status = lonejson__oidc_require_https_url(token_endpoint, "token_endpoint", 1,
                                            error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  max_bytes = max_response_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_TOKEN_RESPONSE_BYTES
                  : max_response_bytes;
  lonejson_owned_buffer_init(&body);
  status = body_fn(request_options, &body, error);
  if (status == LONEJSON_STATUS_OK) {
    memset(&request, 0, sizeof(request));
    request.method = "POST";
    request.url = token_endpoint;
    request.content_type = "application/x-www-form-urlencoded";
    request.body = body.data;
    request.body_len = body.len;
    request.max_response_bytes = max_bytes;
    status =
        lonejson__http_provider_request(runtime, &request, response, error);
  }
  lonejson_owned_buffer_free(&body);
  return status;
}

static lonejson_status lonejson__oauth2_token_endpoint_post(
    lonejson *runtime, const char *token_endpoint, const void *request_options,
    lonejson_status (*body_fn)(const void *, lonejson_owned_buffer *,
                               lonejson_error *),
    size_t max_response_bytes, lonejson_oauth2_token_response *out,
    lonejson_error *error) {
  lonejson_http_response response;
  size_t max_bytes;
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OAuth2 token response output is required");
  }
  max_bytes = max_response_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_TOKEN_RESPONSE_BYTES
                  : max_response_bytes;
  lonejson_http_response_init(&response);
  status = lonejson__oauth2_token_endpoint_request(
      runtime, token_endpoint, request_options, body_fn, max_response_bytes,
      &response, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__http_require_success(&response, "OAuth2 token endpoint",
                                            error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oauth2_token_response_parse_json(
        runtime, response.body.data, response.body.len, max_bytes, out, error);
  }
  lonejson_http_response_cleanup(&response);
  return status;
}

static lonejson_status lonejson__oauth2_client_credentials_body_adapter(
    const void *request, lonejson_owned_buffer *out, lonejson_error *error) {
  return lonejson_oauth2_client_credentials_body(
      (const lonejson_oauth2_client_credentials *)request, out, error);
}

static lonejson_status lonejson__oauth2_refresh_token_body_adapter(
    const void *request, lonejson_owned_buffer *out, lonejson_error *error) {
  return lonejson_oauth2_refresh_token_body(
      (const lonejson_oauth2_refresh_token *)request, out, error);
}

static lonejson_status lonejson__oauth2_token_introspection_body_adapter(
    const void *request, lonejson_owned_buffer *out, lonejson_error *error) {
  return lonejson_oauth2_token_introspection_body(
      (const lonejson_oauth2_token_introspection *)request, out, error);
}

static lonejson_status lonejson__oauth2_token_revocation_body_adapter(
    const void *request, lonejson_owned_buffer *out, lonejson_error *error) {
  return lonejson_oauth2_token_revocation_body(
      (const lonejson_oauth2_token_revocation *)request, out, error);
}

static lonejson_status lonejson__oidc_authorization_code_token_body_adapter(
    const void *request, lonejson_owned_buffer *out, lonejson_error *error) {
  return lonejson_oidc_authorization_code_token_body(
      (const lonejson_oidc_authorization_code_token *)request, out, error);
}

lonejson_status lonejson_oauth2_client_credentials_request(
    lonejson *runtime, const char *token_endpoint,
    const lonejson_oauth2_client_credentials *request_options,
    size_t max_response_bytes, lonejson_oauth2_token_response *out,
    lonejson_error *error) {
  return lonejson__oauth2_token_endpoint_post(
      runtime, token_endpoint, request_options,
      lonejson__oauth2_client_credentials_body_adapter, max_response_bytes, out,
      error);
}

lonejson_status lonejson_oauth2_refresh_token_request(
    lonejson *runtime, const char *token_endpoint,
    const lonejson_oauth2_refresh_token *request_options,
    size_t max_response_bytes, lonejson_oauth2_token_response *out,
    lonejson_error *error) {
  return lonejson__oauth2_token_endpoint_post(
      runtime, token_endpoint, request_options,
      lonejson__oauth2_refresh_token_body_adapter, max_response_bytes, out,
      error);
}

void lonejson_oauth2_token_flow_init(lonejson_oauth2_token_flow *flow) {
  if (flow != NULL) {
    memset(flow, 0, sizeof(*flow));
  }
}

void lonejson_oauth2_token_flow_cleanup(lonejson_oauth2_token_flow *flow) {
  if (flow != NULL) {
    lonejson__owned_free(flow->access_token);
    lonejson__owned_free(flow->token_type);
    lonejson__owned_free(flow->refresh_token);
    lonejson__owned_free(flow->scope);
    lonejson__owned_free(flow->id_token);
    memset(flow, 0, sizeof(*flow));
  }
}

lonejson_status
lonejson_oauth2_token_flow_assign(lonejson_oauth2_token_flow *flow,
                                  const lonejson_oauth2_token_flow *source,
                                  lonejson_error *error) {
  lonejson_oauth2_token_flow next;

  lonejson__clear_error(error);
  if (flow == NULL || source == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OAuth2 token flow and source are required");
  }
  lonejson_oauth2_token_flow_init(&next);
  if (source->access_token != NULL) {
    next.access_token = lonejson__owned_strdup(NULL, source->access_token);
  }
  if (source->token_type != NULL) {
    next.token_type = lonejson__owned_strdup(NULL, source->token_type);
  }
  if (source->refresh_token != NULL) {
    next.refresh_token = lonejson__owned_strdup(NULL, source->refresh_token);
  }
  if (source->scope != NULL) {
    next.scope = lonejson__owned_strdup(NULL, source->scope);
  }
  if (source->id_token != NULL) {
    next.id_token = lonejson__owned_strdup(NULL, source->id_token);
  }
  if ((source->access_token != NULL && next.access_token == NULL) ||
      (source->token_type != NULL && next.token_type == NULL) ||
      (source->refresh_token != NULL && next.refresh_token == NULL) ||
      (source->scope != NULL && next.scope == NULL) ||
      (source->id_token != NULL && next.id_token == NULL)) {
    lonejson_oauth2_token_flow_cleanup(&next);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to copy OAuth2 token flow field");
  }
  next.expires_at = source->expires_at;
  next.has_expires_at = source->has_expires_at;
  lonejson_oauth2_token_flow_cleanup(flow);
  *flow = next;
  return LONEJSON_STATUS_OK;
}

int lonejson_oauth2_token_flow_is_expired(
    const lonejson_oauth2_token_flow *flow, lonejson_int64 now,
    lonejson_int64 skew_seconds) {
  lonejson_int64 skew;

  if (flow == NULL || flow->access_token == NULL ||
      flow->access_token[0] == '\0') {
    return 1;
  }
  if (!flow->has_expires_at) {
    return 0;
  }
  skew = skew_seconds == 0
             ? LONEJSON__OAUTH2_DEFAULT_TOKEN_FLOW_REFRESH_SKEW_SECONDS
             : skew_seconds;
  if (skew < 0) {
    skew = 0;
  }
  if (now > (lonejson_int64)(LONEJSON_UINT64_MAX >> 1) - skew) {
    return 1;
  }
  return flow->expires_at <= now + skew;
}

lonejson_status lonejson_oauth2_token_flow_update_response(
    lonejson_oauth2_token_flow *flow,
    const lonejson_oauth2_token_response *response, lonejson_int64 now,
    lonejson_error *error) {
  lonejson_oauth2_token_flow next;

  lonejson__clear_error(error);
  if (flow == NULL || response == NULL || response->access_token == NULL ||
      response->token_type == NULL || now < 0) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OAuth2 token flow, successful token response, and non-negative "
        "current time are required");
  }
  if (response->has_expires_in) {
    if (response->expires_in >
        (lonejson_int64)(LONEJSON_UINT64_MAX >> 1) - now) {
      return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                                 "OAuth2 token flow expiry overflows");
    }
  }
  lonejson_oauth2_token_flow_init(&next);
  next.access_token = lonejson__owned_strdup(NULL, response->access_token);
  next.token_type = lonejson__owned_strdup(NULL, response->token_type);
  if (response->refresh_token != NULL) {
    next.refresh_token = lonejson__owned_strdup(NULL, response->refresh_token);
  } else if (flow->refresh_token != NULL) {
    next.refresh_token = lonejson__owned_strdup(NULL, flow->refresh_token);
  }
  if (response->scope != NULL) {
    next.scope = lonejson__owned_strdup(NULL, response->scope);
  }
  if (response->id_token != NULL) {
    next.id_token = lonejson__owned_strdup(NULL, response->id_token);
  }
  if (next.access_token == NULL || next.token_type == NULL ||
      (response->refresh_token != NULL && next.refresh_token == NULL) ||
      (response->refresh_token == NULL && flow->refresh_token != NULL &&
       next.refresh_token == NULL) ||
      (response->scope != NULL && next.scope == NULL) ||
      (response->id_token != NULL && next.id_token == NULL)) {
    lonejson_oauth2_token_flow_cleanup(&next);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to copy OAuth2 token flow field");
  }
  if (response->has_expires_in) {
    next.expires_at = now + response->expires_in;
    next.has_expires_at = 1;
  }
  lonejson_oauth2_token_flow_cleanup(flow);
  *flow = next;
  return LONEJSON_STATUS_OK;
}

static int
lonejson__oauth2_token_flow_retryable_status(lonejson_status status) {
  return status == LONEJSON_STATUS_CALLBACK_FAILED ||
         status == LONEJSON_STATUS_IO_ERROR ||
         status == LONEJSON_STATUS_TRUNCATED;
}

static int lonejson__oauth2_token_flow_retryable_http(
    const lonejson_http_response *response) {
  return response != NULL &&
         (response->status_code == 429 ||
          (response->status_code >= 500 && response->status_code <= 599));
}

lonejson_status lonejson_oauth2_token_flow_ensure(
    lonejson *runtime, lonejson_oauth2_token_flow *flow,
    const lonejson_oauth2_token_flow_policy *policy,
    lonejson_oauth2_token_flow_result *result, lonejson_error *error) {
  lonejson_oauth2_refresh_token request;
  lonejson_oauth2_token_response token_response;
  lonejson_http_response response;
  lonejson_status status;
  unsigned attempts;
  unsigned max_retries;
  int retry;

  lonejson__clear_error(error);
  if (result != NULL) {
    memset(result, 0, sizeof(*result));
    result->state = LONEJSON_OAUTH2_TOKEN_FLOW_FAILED;
  }
  if (flow == NULL || policy == NULL || policy->now < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OAuth2 token flow, policy, and non-negative "
                               "current time are required");
  }
  if (!lonejson_oauth2_token_flow_is_expired(flow, policy->now,
                                             policy->refresh_skew_seconds)) {
    if (result != NULL) {
      result->state = LONEJSON_OAUTH2_TOKEN_FLOW_READY;
    }
    return LONEJSON_STATUS_OK;
  }
  if (policy->disable_refresh || flow->refresh_token == NULL ||
      flow->refresh_token[0] == '\0') {
    if (result != NULL) {
      result->state = LONEJSON_OAUTH2_TOKEN_FLOW_NEEDS_INTERACTION;
    }
    return LONEJSON_STATUS_OK;
  }
  if (policy->token_endpoint == NULL || policy->client_id == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OAuth2 token flow refresh requires token_endpoint and client_id");
  }

  memset(&request, 0, sizeof(request));
  request.refresh_token = flow->refresh_token;
  request.client_id = policy->client_id;
  request.client_secret = policy->client_secret;
  request.scope = policy->scope;
  lonejson_oauth2_token_response_init(&token_response);
  lonejson_http_response_init(&response);
  max_retries = policy->disable_retry
                    ? 0u
                    : (policy->max_retries == 0u
                           ? LONEJSON__OAUTH2_DEFAULT_TOKEN_FLOW_RETRIES
                           : policy->max_retries);
  attempts = 0u;
  do {
    ++attempts;
    lonejson_http_response_cleanup(&response);
    status = lonejson__oauth2_token_endpoint_request(
        runtime, policy->token_endpoint, &request,
        lonejson__oauth2_refresh_token_body_adapter, policy->max_response_bytes,
        &response, error);
    retry = 0;
    if (status == LONEJSON_STATUS_OK &&
        lonejson__oauth2_token_flow_retryable_http(&response)) {
      retry = attempts <= max_retries;
    } else if (status != LONEJSON_STATUS_OK &&
               lonejson__oauth2_token_flow_retryable_status(status)) {
      retry = attempts <= max_retries;
    }
  } while (retry);

  if (result != NULL) {
    result->attempts = attempts;
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__http_require_success(&response, "OAuth2 token endpoint",
                                            error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oauth2_token_response_parse_json(
        runtime, response.body.data, response.body.len,
        policy->max_response_bytes, &token_response, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oauth2_token_flow_update_response(flow, &token_response,
                                                        policy->now, error);
  }
  if (status == LONEJSON_STATUS_OK && result != NULL) {
    result->state = LONEJSON_OAUTH2_TOKEN_FLOW_REFRESHED;
    result->refreshed = 1;
  }
  lonejson_oauth2_token_response_cleanup(&token_response);
  lonejson_http_response_cleanup(&response);
  return status;
}

static lonejson_status lonejson__oauth2_form_endpoint_post(
    lonejson *runtime, const char *endpoint, const char *endpoint_name,
    const void *request_options,
    lonejson_status (*body_fn)(const void *, lonejson_owned_buffer *,
                               lonejson_error *),
    const char *authorization, size_t max_response_bytes,
    lonejson_http_response *response, lonejson_error *error) {
  lonejson_owned_buffer body;
  lonejson_http_request request;
  size_t max_bytes;
  lonejson_status status;

  status = lonejson__oidc_require_https_url(endpoint, endpoint_name, 1, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  max_bytes = max_response_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_TOKEN_RESPONSE_BYTES
                  : max_response_bytes;
  lonejson_owned_buffer_init(&body);
  status = body_fn(request_options, &body, error);
  if (status == LONEJSON_STATUS_OK) {
    memset(&request, 0, sizeof(request));
    request.method = "POST";
    request.url = endpoint;
    request.content_type = "application/x-www-form-urlencoded";
    request.authorization = authorization;
    request.body = body.data;
    request.body_len = body.len;
    request.max_response_bytes = max_bytes;
    status =
        lonejson__http_provider_request(runtime, &request, response, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__http_require_success(response, endpoint_name, error);
  }
  lonejson_owned_buffer_free(&body);
  return status;
}

static lonejson_status lonejson__oauth2_basic_authorization(
    const char *client_id, const char *client_secret,
    lonejson_owned_buffer *out, lonejson_error *error) {
  lonejson_owned_buffer credentials;
  char *encoded;
  size_t id_len;
  size_t secret_len;
  size_t encoded_len;
  size_t needed;
  lonejson_status status;

  encoded = NULL;
  if (client_id == NULL || client_id[0] == '\0' || client_secret == NULL ||
      client_secret[0] == '\0' || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OAuth2 client_id and client_secret are "
                               "required for client_secret_basic");
  }
  id_len = strlen(client_id);
  secret_len = strlen(client_secret);
  if (id_len > SIZE_MAX - secret_len - 1u) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "OAuth2 client credentials are too large");
  }
  lonejson_owned_buffer_init(&credentials);
  lonejson_owned_buffer_free(out);
  status = lonejson__oauth2_form_append_component(&credentials, client_id,
                                                  SIZE_MAX, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_owned_buffer_sink(&credentials, ":", 1u, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_component(&credentials, client_secret,
                                                    SIZE_MAX, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_base64_encoded_len(
        credentials.len, LONEJSON_BASE64_STANDARD, &encoded_len, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_owned_buffer_sink(out, "Basic ", 6u, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    if (encoded_len > SIZE_MAX - out->len - 1u) {
      status = lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                                   "OAuth2 Basic authorization header is too "
                                   "large");
    } else {
      encoded = (char *)LONEJSON_MALLOC(encoded_len + 1u);
      if (encoded == NULL) {
        status = lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                     0u, 0u, 0u,
                                     "failed to allocate OAuth2 Basic "
                                     "authorization header");
      }
    }
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_base64_encode(credentials.data, credentials.len,
                                    LONEJSON_BASE64_STANDARD, encoded,
                                    encoded_len + 1u, &needed, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_owned_buffer_sink(out, encoded, needed, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    out->data[out->len] = '\0';
  } else {
    lonejson_owned_buffer_free(out);
  }
  LONEJSON_FREE(encoded);
  lonejson_owned_buffer_free(&credentials);
  return status;
}

lonejson_status lonejson_oauth2_introspect_token_request(
    lonejson *runtime, const char *introspection_endpoint,
    const lonejson_oauth2_token_introspection *request_options,
    size_t max_response_bytes, lonejson_oauth2_introspection_response *out,
    lonejson_error *error) {
  lonejson_http_response response;
  lonejson_owned_buffer authorization;
  const char *authorization_header;
  size_t max_bytes;
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OAuth2 introspection response output is "
                               "required");
  }
  max_bytes = max_response_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_INTROSPECTION_RESPONSE_BYTES
                  : max_response_bytes;
  authorization_header = NULL;
  lonejson_owned_buffer_init(&authorization);
  if (request_options != NULL && request_options->use_basic_auth) {
    status = lonejson__oauth2_basic_authorization(
        request_options->client_id, request_options->client_secret,
        &authorization, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    authorization_header = authorization.data;
  }
  lonejson_http_response_init(&response);
  status = lonejson__oauth2_form_endpoint_post(
      runtime, introspection_endpoint, "OAuth2 introspection endpoint",
      request_options, lonejson__oauth2_token_introspection_body_adapter,
      authorization_header, max_bytes, &response, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oauth2_introspection_response_parse_json(
        runtime, response.body.data, response.body.len, max_bytes, out, error);
  }
  lonejson_http_response_cleanup(&response);
  lonejson_owned_buffer_free(&authorization);
  return status;
}

lonejson_status lonejson_oauth2_revoke_token_request(
    lonejson *runtime, const char *revocation_endpoint,
    const lonejson_oauth2_token_revocation *request_options,
    lonejson_error *error) {
  lonejson_http_response response;
  lonejson_owned_buffer authorization;
  const char *authorization_header;
  lonejson_status status;

  lonejson__clear_error(error);
  authorization_header = NULL;
  lonejson_owned_buffer_init(&authorization);
  if (request_options != NULL && request_options->use_basic_auth) {
    status = lonejson__oauth2_basic_authorization(
        request_options->client_id, request_options->client_secret,
        &authorization, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    authorization_header = authorization.data;
  }
  lonejson_http_response_init(&response);
  status = lonejson__oauth2_form_endpoint_post(
      runtime, revocation_endpoint, "OAuth2 revocation endpoint",
      request_options, lonejson__oauth2_token_revocation_body_adapter,
      authorization_header, 4096u, &response, error);
  lonejson_http_response_cleanup(&response);
  lonejson_owned_buffer_free(&authorization);
  return status;
}

lonejson_status lonejson_oidc_fetch_userinfo(
    lonejson *runtime, const char *userinfo_endpoint,
    const lonejson_oidc_userinfo_request *request_options,
    lonejson_oidc_userinfo_response *out, lonejson_error *error) {
  lonejson_http_request request;
  lonejson_http_response response;
  size_t max_bytes;
  lonejson_status status;
  size_t token_len;
  char *authorization;

  lonejson__clear_error(error);
  if (request_options == NULL || out == NULL ||
      request_options->access_token == NULL ||
      request_options->access_token[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OIDC UserInfo access token and output are "
                               "required");
  }
  status = lonejson__oidc_require_https_url(userinfo_endpoint,
                                            "userinfo_endpoint", 1, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  token_len = strlen(request_options->access_token);
  if (token_len > SIZE_MAX - sizeof("Bearer ")) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "OIDC UserInfo access token is too large");
  }
  authorization =
      (char *)lonejson__owned_malloc(NULL, sizeof("Bearer ") + token_len);
  if (authorization == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u,
                               "failed to allocate OIDC UserInfo authorization "
                               "header");
  }
  memcpy(authorization, "Bearer ", sizeof("Bearer ") - 1u);
  memcpy(authorization + sizeof("Bearer ") - 1u, request_options->access_token,
         token_len + 1u);
  max_bytes = request_options->max_response_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_USERINFO_RESPONSE_BYTES
                  : request_options->max_response_bytes;
  lonejson_http_response_init(&response);
  memset(&request, 0, sizeof(request));
  request.method = "GET";
  request.url = userinfo_endpoint;
  request.authorization = authorization;
  request.max_response_bytes = max_bytes;
  status = lonejson__http_provider_request(runtime, &request, &response, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__http_require_success(&response, "OIDC UserInfo endpoint",
                                            error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oidc_userinfo_response_parse_json(
        runtime, response.body.data, response.body.len, max_bytes, out, error);
  }
  lonejson_http_response_cleanup(&response);
  lonejson__owned_free(authorization);
  return status;
}

lonejson_status lonejson_oidc_authorization_code_token_request(
    lonejson *runtime, const char *token_endpoint,
    const lonejson_oidc_authorization_code_token *request_options,
    size_t max_response_bytes, lonejson_oauth2_token_response *out,
    lonejson_error *error) {
  return lonejson__oauth2_token_endpoint_post(
      runtime, token_endpoint, request_options,
      lonejson__oidc_authorization_code_token_body_adapter, max_response_bytes,
      out, error);
}

static lonejson_status lonejson__oidc_authorization_append_pair(
    lonejson_owned_buffer *out, const char *key, const char *value,
    int *has_pair, size_t max_bytes, lonejson_error *error) {
  return lonejson__oauth2_form_append_pair(out, key, value, has_pair, max_bytes,
                                           error);
}

lonejson_status lonejson_oidc_authorization_url(
    const lonejson_oidc_authorization_request *request,
    lonejson_owned_buffer *out, lonejson_error *error) {
  size_t max_bytes;
  lonejson_status status;
  int has_query;
  int has_pair = 0;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL ||
      request->authorization_endpoint == NULL || request->client_id == NULL ||
      request->redirect_uri == NULL || request->state == NULL ||
      request->nonce == NULL || request->code_challenge == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "authorization endpoint, client_id, redirect_uri, state, nonce, and "
        "code_challenge are required");
  }
  status = lonejson__oidc_require_https_url(request->authorization_endpoint,
                                            "authorization_endpoint", 1, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (strchr(request->authorization_endpoint, '#') != NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
        "OIDC authorization_endpoint must not contain a fragment");
  }
  max_bytes = request->max_url_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_AUTH_URL_BYTES
                  : request->max_url_bytes;
  lonejson_owned_buffer_free(out);
  status = lonejson__oauth2_form_append_raw(
      out, request->authorization_endpoint,
      strlen(request->authorization_endpoint), max_bytes, error);
  has_query = strchr(request->authorization_endpoint, '?') != NULL;
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_raw(out, has_query ? "&" : "?", 1u,
                                              max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "response_type", "code", &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "client_id", request->client_id, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "redirect_uri", request->redirect_uri, &has_pair, max_bytes,
        error);
  }
  if (status == LONEJSON_STATUS_OK && request->scope != NULL) {
    status = lonejson__oidc_authorization_append_pair(
        out, "scope", request->scope, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "state", request->state, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "nonce", request->nonce, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "code_challenge", request->code_challenge, &has_pair, max_bytes,
        error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "code_challenge_method", "S256", &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK && request->audience != NULL) {
    status = lonejson__oidc_authorization_append_pair(
        out, "audience", request->audience, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK && request->resource != NULL) {
    status = lonejson__oidc_authorization_append_pair(
        out, "resource", request->resource, &has_pair, max_bytes, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(out);
  }
  return status;
}

void lonejson_oidc_authorization_callback_init(
    lonejson_oidc_authorization_callback *callback) {
  if (callback != NULL) {
    memset(callback, 0, sizeof(*callback));
  }
}

void lonejson_oidc_authorization_callback_cleanup(
    lonejson_oidc_authorization_callback *callback) {
  if (callback != NULL) {
    lonejson__owned_free(callback->code);
    lonejson__owned_free(callback->state);
    lonejson__owned_free(callback->error);
    lonejson__owned_free(callback->error_description);
    lonejson__owned_free(callback->error_uri);
    memset(callback, 0, sizeof(*callback));
  }
}

static int lonejson__oauth2_hex_value(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  return -1;
}

static lonejson_status
lonejson__oauth2_percent_decode(const char *data, size_t len,
                                lonejson_owned_buffer *out,
                                lonejson_error *error) {
  size_t i;
  int hi;
  int lo;
  char ch;

  lonejson_owned_buffer_free(out);
  for (i = 0u; i < len; ++i) {
    ch = data[i];
    if (ch == '+') {
      ch = ' ';
    } else if (ch == '%') {
      if (i + 2u >= len) {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                   0u, "callback query has bad percent escape");
      }
      hi = lonejson__oauth2_hex_value(data[i + 1u]);
      lo = lonejson__oauth2_hex_value(data[i + 2u]);
      if (hi < 0 || lo < 0) {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                   0u, "callback query has bad percent escape");
      }
      ch = (char)((hi << 4) | lo);
      i += 2u;
    }
    if (lonejson_owned_buffer_sink(out, &ch, 1u, error) != LONEJSON_STATUS_OK) {
      return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__oidc_callback_reject_nul(const char *value, size_t len,
                                   const char *kind, lonejson_error *error) {
  if (len != 0u && memchr(value, '\0', len) != NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "%s contains a NUL byte", kind);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__oidc_callback_assign(char **dst,
                                                      const char *value,
                                                      size_t len,
                                                      lonejson_error *error) {
  if (*dst != NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_DUPLICATE_FIELD, 0u, 0u,
                               0u, "duplicate callback query parameter");
  }
  *dst = (char *)lonejson__owned_malloc(NULL, len + 1u);
  if (*dst == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate callback parameter");
  }
  if (len != 0u) {
    memcpy(*dst, value, len);
  }
  (*dst)[len] = '\0';
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oidc_authorization_callback_parse_query(
    const char *query, size_t len, const char *expected_state,
    size_t max_query_bytes, lonejson_oidc_authorization_callback *out,
    lonejson_error *error) {
  size_t max_bytes;
  size_t pos = 0u;
  size_t key_begin;
  size_t key_len;
  size_t value_begin;
  size_t value_len;
  lonejson_owned_buffer key;
  lonejson_owned_buffer value;
  lonejson_status status = LONEJSON_STATUS_OK;

  lonejson__clear_error(error);
  if (out == NULL || expected_state == NULL || expected_state[0] == '\0' ||
      (query == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "callback query, expected_state, and output are "
                               "required");
  }
  max_bytes = max_query_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_CALLBACK_QUERY_BYTES
                  : max_query_bytes;
  if (len > max_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, len, 0u, 0u,
                               "callback query exceeds configured limit");
  }
  lonejson_oidc_authorization_callback_cleanup(out);
  lonejson_owned_buffer_init(&key);
  lonejson_owned_buffer_init(&value);
  if (len != 0u && query[0] == '?') {
    pos = 1u;
  }
  while (status == LONEJSON_STATUS_OK && pos <= len) {
    key_begin = pos;
    while (pos < len && query[pos] != '=' && query[pos] != '&') {
      ++pos;
    }
    key_len = pos - key_begin;
    value_begin = pos;
    value_len = 0u;
    if (pos < len && query[pos] == '=') {
      ++pos;
      value_begin = pos;
      while (pos < len && query[pos] != '&') {
        ++pos;
      }
      value_len = pos - value_begin;
    }
    if (pos < len && query[pos] == '&') {
      ++pos;
    } else if (pos == len) {
      ++pos;
    }
    if (key_len == 0u) {
      continue;
    }
    status = lonejson__oauth2_percent_decode(query + key_begin, key_len, &key,
                                             error);
    if (status == LONEJSON_STATUS_OK) {
      status = lonejson__oauth2_percent_decode(query + value_begin, value_len,
                                               &value, error);
    }
    if (status != LONEJSON_STATUS_OK) {
      break;
    }
    status = lonejson__oidc_callback_reject_nul(
        key.data != NULL ? key.data : "", key.len, "callback query parameter",
        error);
    if (status == LONEJSON_STATUS_OK) {
      status = lonejson__oidc_callback_reject_nul(
          value.data != NULL ? value.data : "", value.len,
          "callback query parameter value", error);
    }
    if (status != LONEJSON_STATUS_OK) {
      break;
    }
    if (strcmp(key.data, "code") == 0) {
      status = lonejson__oidc_callback_assign(
          &out->code, value.data != NULL ? value.data : "", value.len, error);
    } else if (strcmp(key.data, "state") == 0) {
      status = lonejson__oidc_callback_assign(
          &out->state, value.data != NULL ? value.data : "", value.len, error);
    } else if (strcmp(key.data, "error") == 0) {
      status = lonejson__oidc_callback_assign(
          &out->error, value.data != NULL ? value.data : "", value.len, error);
    } else if (strcmp(key.data, "error_description") == 0) {
      status = lonejson__oidc_callback_assign(
          &out->error_description, value.data != NULL ? value.data : "",
          value.len, error);
    } else if (strcmp(key.data, "error_uri") == 0) {
      status = lonejson__oidc_callback_assign(
          &out->error_uri, value.data != NULL ? value.data : "", value.len,
          error);
    }
  }
  lonejson_owned_buffer_free(&key);
  lonejson_owned_buffer_free(&value);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_authorization_callback_cleanup(out);
    return status;
  }
  if (out->error != NULL) {
    lonejson_oidc_authorization_callback_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "authorization callback returned an error");
  }
  if (out->state == NULL || strcmp(out->state, expected_state) != 0) {
    lonejson_oidc_authorization_callback_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "authorization callback state mismatch");
  }
  if (out->code == NULL || out->code[0] == '\0') {
    lonejson_oidc_authorization_callback_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "authorization callback code is required");
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oidc_jwks_cache_update_json(
    lonejson *runtime, lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy, const char *json, size_t len,
    lonejson_error *error) {
  lonejson_jwks next_jwks;
  char *issuer = NULL;
  char *jwks_uri = NULL;
  size_t max_bytes;
  lonejson_status status;

  lonejson__clear_error(error);
  if (cache == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC JWKS cache is required");
  }
  if (json == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC JWKS JSON input is required");
  }
  status = lonejson__oidc_jwks_cache_validate_policy(policy, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  max_bytes = policy->max_jwks_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_JWKS_BYTES
                  : policy->max_jwks_bytes;
  if (len > max_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, len, 0u, 0u,
                               "OIDC JWKS response exceeds configured limit");
  }
  lonejson_jwks_init(&next_jwks);
  status = lonejson_jwks_parse_json(runtime, json, len, &next_jwks, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  issuer = lonejson__owned_strdup(NULL, policy->issuer);
  jwks_uri = lonejson__owned_strdup(NULL, policy->jwks_uri);
  if (issuer == NULL || jwks_uri == NULL) {
    lonejson__owned_free(issuer);
    lonejson__owned_free(jwks_uri);
    lonejson_jwks_cleanup(&next_jwks);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate OIDC JWKS metadata");
  }
  lonejson_oidc_jwks_cache_cleanup(cache);
  cache->issuer = issuer;
  cache->jwks_uri = jwks_uri;
  cache->fetched_at = policy->now;
  cache->expires_at = policy->now + policy->ttl_seconds;
  cache->max_jwks_bytes = max_bytes;
  cache->has_jwks = 1;
  cache->jwks = next_jwks;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oidc_jwks_cache_refresh(
    lonejson *runtime, lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy, lonejson_error *error) {
  lonejson_http_request request;
  lonejson_http_response response;
  size_t max_bytes;
  lonejson_status status;

  lonejson__clear_error(error);
  status = lonejson__oidc_jwks_cache_validate_policy(policy, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  max_bytes = policy->max_jwks_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_JWKS_BYTES
                  : policy->max_jwks_bytes;
  lonejson_http_response_init(&response);
  memset(&request, 0, sizeof(request));
  request.method = "GET";
  request.url = policy->jwks_uri;
  request.max_response_bytes = max_bytes;
  status = lonejson__http_provider_request(runtime, &request, &response, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__http_require_success(&response, "OIDC JWKS", error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oidc_jwks_cache_update_json(
        runtime, cache, policy, response.body.data, response.body.len, error);
  }
  lonejson_http_response_cleanup(&response);
  return status;
}

int lonejson_oidc_jwks_cache_is_fresh(
    const lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy) {
  if (lonejson__oidc_jwks_cache_validate_policy(policy, NULL) !=
      LONEJSON_STATUS_OK) {
    return 0;
  }
  return lonejson__oidc_jwks_cache_matches_policy(cache, policy);
}

lonejson_status
lonejson_oidc_jwks_cache_select(const lonejson_oidc_jwks_cache *cache,
                                const lonejson_oidc_jwks_cache_policy *policy,
                                const lonejson_jwk_select_options *options,
                                const lonejson_jwk **out,
                                lonejson_error *error) {
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC JWKS selected key output is required");
  }
  *out = NULL;
  status = lonejson__oidc_jwks_cache_validate_policy(policy, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (!lonejson__oidc_jwks_cache_matches_policy(cache, policy)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "OIDC JWKS cache is missing or stale");
  }
  return lonejson_jwks_select(&cache->jwks, options, out, error);
}

static int lonejson__jwk_match_optional_declared(const char *value,
                                                 const char *required) {
  return required == NULL || value == NULL || strcmp(value, required) == 0;
}

static lonejson_status lonejson__oidc_jwks_cache_select_bearer_key(
    const lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy, const char *kid,
    const char *kty, const char *alg, const lonejson_jwk **out,
    lonejson_error *error) {
  lonejson_status status;
  size_t i;

  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC JWKS selected key output is required");
  }
  *out = NULL;
  status = lonejson__oidc_jwks_cache_validate_policy(policy, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (!lonejson__oidc_jwks_cache_matches_policy(cache, policy)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "OIDC JWKS cache is missing or stale");
  }
  for (i = 0u; i < cache->jwks.keys.count; ++i) {
    const lonejson_jwk *jwk =
        &((const lonejson_jwk *)cache->jwks.keys.items)[i];
    if (!lonejson__jwk_match_filter(jwk->kid, kid) ||
        !lonejson__jwk_match_filter(jwk->kty, kty) ||
        !lonejson__jwk_match_optional_declared(jwk->alg, alg) ||
        !lonejson__jwk_match_optional_declared(jwk->use, "sig")) {
      continue;
    }
    *out = jwk;
    return LONEJSON_STATUS_OK;
  }
  return LONEJSON_STATUS_OK;
}

#ifdef LONEJSON_WITH_CURL
lonejson_status lonejson_oidc_jwks_cache_parse_init(
    lonejson_oidc_jwks_cache_parse *ctx, lonejson *runtime,
    lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy) {
  lonejson_status status;
  char *issuer;
  char *jwks_uri;

  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (lonejson__curl_state_is_live(ctx->_reserved_state)) {
    lonejson_oidc_jwks_cache_parse_cleanup(ctx);
  }
  memset(ctx, 0, sizeof(*ctx));
  lonejson__oidc_jwks_cache_assign_methods(ctx);
  lonejson__curl_state_mark_live(ctx->_reserved_state);
  lonejson_owned_buffer_init(&ctx->response);
  if (runtime == NULL || cache == NULL || policy == NULL) {
    return lonejson__set_error(
        &ctx->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "runtime, JWKS cache, and cache policy are required");
  }
  status = lonejson__oidc_jwks_cache_validate_policy(policy, &ctx->error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  issuer = lonejson__owned_strdup(NULL, policy->issuer);
  jwks_uri = lonejson__owned_strdup(NULL, policy->jwks_uri);
  if (issuer == NULL || jwks_uri == NULL) {
    lonejson__owned_free(issuer);
    lonejson__owned_free(jwks_uri);
    return lonejson__set_error(&ctx->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                               0u, 0u, 0u,
                               "failed to allocate OIDC JWKS cache policy");
  }
  ctx->runtime = runtime;
  ctx->cache = cache;
  ctx->policy = *policy;
  ctx->policy.issuer = issuer;
  ctx->policy.jwks_uri = jwks_uri;
  return LONEJSON_STATUS_OK;
}

size_t lonejson_oidc_jwks_cache_write_callback(char *ptr, size_t size,
                                               size_t nmemb, void *userdata) {
  lonejson_oidc_jwks_cache_parse *ctx =
      (lonejson_oidc_jwks_cache_parse *)userdata;
  size_t bytes;
  size_t max_bytes;

  if (ctx == NULL || !lonejson__curl_state_is_live(ctx->_reserved_state)) {
    return 0u;
  }
  if (size != 0u && nmemb > SIZE_MAX / size) {
    (void)lonejson__set_error(&ctx->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                              "OIDC JWKS response chunk size overflows");
    return 0u;
  }
  bytes = size * nmemb;
  if (bytes == 0u) {
    return 0u;
  }
  if (ptr == NULL) {
    (void)lonejson__set_error(&ctx->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                              0u, 0u, "OIDC JWKS response chunk is required");
    return 0u;
  }
  max_bytes = ctx->policy.max_jwks_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_JWKS_BYTES
                  : ctx->policy.max_jwks_bytes;
  if (ctx->response.len > max_bytes || bytes > max_bytes - ctx->response.len) {
    (void)lonejson__set_error(&ctx->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                              "OIDC JWKS response exceeds configured limit");
    return 0u;
  }
  return lonejson_owned_buffer_sink(&ctx->response, ptr, bytes, &ctx->error) ==
                 LONEJSON_STATUS_OK
             ? bytes
             : 0u;
}

lonejson_status
lonejson_oidc_jwks_cache_parse_finish(lonejson_oidc_jwks_cache_parse *ctx) {
  if (ctx == NULL || !lonejson__curl_state_is_live(ctx->_reserved_state)) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (ctx->error.code != LONEJSON_STATUS_OK) {
    return ctx->error.code;
  }
  return lonejson_oidc_jwks_cache_update_json(ctx->runtime, ctx->cache,
                                              &ctx->policy, ctx->response.data,
                                              ctx->response.len, &ctx->error);
}

void lonejson_oidc_jwks_cache_parse_cleanup(
    lonejson_oidc_jwks_cache_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  if (!lonejson__curl_state_is_live(ctx->_reserved_state)) {
    return;
  }
  lonejson__owned_free((void *)ctx->policy.issuer);
  lonejson__owned_free((void *)ctx->policy.jwks_uri);
  lonejson_owned_buffer_free(&ctx->response);
  memset(ctx, 0, sizeof(*ctx));
  lonejson__oidc_jwks_cache_assign_methods(ctx);
}
#endif
#endif

static int lonejson__jwt_path_is(const lonejson_value_path *path,
                                 const char *name) {
  return path != NULL && path->segment_count == 1u &&
         strlen(name) == path->segments[0].len &&
         memcmp(path->segments[0].data, name, path->segments[0].len) == 0;
}

static int lonejson__jwt_path_is_array_item(const lonejson_value_path *path,
                                            const char *name) {
  size_t i;
  size_t name_len;

  name_len = strlen(name);
  if (path == NULL || path->segment_count != 2u ||
      path->segments[0].len != name_len ||
      memcmp(path->segments[0].data, name, name_len) != 0) {
    return 0;
  }
  if (path->segments[1].len == 0u) {
    return 0;
  }
  for (i = 0u; i < path->segments[1].len; ++i) {
    unsigned char ch = (unsigned char)path->segments[1].data[i];
    if (ch < (unsigned char)'0' || ch > (unsigned char)'9') {
      return 0;
    }
  }
  return 1;
}

static int lonejson__jwt_path_is_aud_item(const lonejson_value_path *path) {
  return lonejson__jwt_path_is_array_item(path, "aud");
}

static int
lonejson__jwt_path_is_registered_header(const lonejson_value_path *path) {
  return lonejson__jwt_path_is(path, "alg") ||
         lonejson__jwt_path_is(path, "kid") ||
         lonejson__jwt_path_is(path, "typ") ||
         lonejson__jwt_path_is(path, "crit") ||
         lonejson__jwt_path_is_array_item(path, "crit") ||
         lonejson__jwt_path_is(path, "x5t") ||
         lonejson__jwt_path_is(path, "x5t#S256") ||
         lonejson__jwt_path_is(path, "x5c") ||
         lonejson__jwt_path_is_array_item(path, "x5c");
}

static int
lonejson__jwt_path_is_registered_claim(const lonejson_value_path *path) {
  return lonejson__jwt_path_is(path, "iss") ||
         lonejson__jwt_path_is(path, "sub") ||
         lonejson__jwt_path_is(path, "nonce") ||
         lonejson__jwt_path_is(path, "azp") ||
         lonejson__jwt_path_is(path, "scope") ||
         lonejson__jwt_path_is(path, "scp") ||
         lonejson__jwt_path_is_array_item(path, "scp") ||
         lonejson__jwt_path_is(path, "aud") ||
         lonejson__jwt_path_is_aud_item(path) ||
         lonejson__jwt_path_is(path, "exp") ||
         lonejson__jwt_path_is(path, "nbf") ||
         lonejson__jwt_path_is(path, "iat");
}

static lonejson_status lonejson__jwt_claim_type_error(lonejson_error *error,
                                                      const char *claim) {
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "JWT claim has invalid type: %s", claim);
}

static lonejson_status
lonejson__jwt_claim_duplicate_error(lonejson_error *error, const char *claim) {
  return lonejson__set_error(error, LONEJSON_STATUS_DUPLICATE_FIELD, 0u, 0u, 0u,
                             "duplicate JWT registered claim: %s", claim);
}

static lonejson_status
lonejson__jwt_visit_buffer_append(lonejson__jwt_claim_visit *state,
                                  const char *data, size_t len,
                                  lonejson_error *error) {
  char *next;
  size_t next_cap;

  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (memchr(data, '\0', len) != NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "JWT string claim contains NUL byte");
  }
  if (state->len > SIZE_MAX - len - 1u) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JWT claim value is too large");
  }
  if (state->len + len + 1u > state->capacity) {
    next_cap = state->capacity == 0u ? 32u : state->capacity;
    while (next_cap < state->len + len + 1u) {
      if (next_cap > SIZE_MAX / 2u) {
        next_cap = state->len + len + 1u;
        break;
      }
      next_cap *= 2u;
    }
    next = (char *)lonejson__owned_realloc(NULL, state->buffer, next_cap);
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    state->buffer = next;
    state->capacity = next_cap;
  }
  memcpy(state->buffer + state->len, data, len);
  state->len += len;
  state->buffer[state->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static void lonejson__jwt_visit_buffer_reset(lonejson__jwt_claim_visit *state) {
  state->len = 0u;
  if (state->buffer != NULL) {
    state->buffer[0] = '\0';
  }
}

static char *lonejson__jwt_visit_buffer_take(lonejson__jwt_claim_visit *state) {
  char *out = state->buffer;
  if (out == NULL) {
    out = (char *)lonejson__owned_malloc(NULL, 1u);
    if (out == NULL) {
      return NULL;
    }
    out[0] = '\0';
  }
  state->buffer = NULL;
  state->len = 0u;
  state->capacity = 0u;
  return out;
}

static lonejson_status
lonejson__jwt_string_array_append(lonejson_string_array *array, char *value,
                                  lonejson_error *error) {
  char **next;
  size_t next_capacity;

  if (array->count == array->capacity) {
    next_capacity = array->capacity == 0u ? 4u : array->capacity * 2u;
    if (next_capacity < array->capacity) {
      return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                                 "JWT audience array is too large");
    }
    next = (char **)lonejson__owned_realloc(
        NULL, array->items, next_capacity * sizeof(array->items[0]));
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u,
                                 "failed to allocate JWT audience array");
    }
    array->items = next;
    array->capacity = next_capacity;
    array->flags |= LONEJSON_ARRAY_OWNS_ITEMS;
  }
  array->items[array->count++] = value;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_begin_string_field(lonejson__jwt_claim_visit *state,
                                 lonejson__jwt_claim_field field, int *seen,
                                 const char *name, lonejson_error *error) {
  if (*seen) {
    return lonejson__jwt_claim_duplicate_error(error, name);
  }
  *seen = 1;
  state->active = field;
  lonejson__jwt_visit_buffer_reset(state);
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_string_begin(void *user, const lonejson_value_path *path,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;

  if (state->header_mode) {
    if (lonejson__jwt_path_is(path, "alg")) {
      return lonejson__jwt_begin_string_field(state,
                                              LONEJSON__JWT_FIELD_HEADER_ALG,
                                              &state->seen_alg, "alg", error);
    }
    if (lonejson__jwt_path_is(path, "kid")) {
      return lonejson__jwt_begin_string_field(state,
                                              LONEJSON__JWT_FIELD_HEADER_KID,
                                              &state->seen_kid, "kid", error);
    }
    if (lonejson__jwt_path_is(path, "typ")) {
      return lonejson__jwt_begin_string_field(state,
                                              LONEJSON__JWT_FIELD_HEADER_TYP,
                                              &state->seen_typ, "typ", error);
    }
    if (lonejson__jwt_path_is(path, "crit") ||
        lonejson__jwt_path_is(path, "x5c")) {
      return lonejson__jwt_claim_type_error(error, "registered");
    }
    if (lonejson__jwt_path_is_array_item(path, "crit")) {
      state->active = LONEJSON__JWT_FIELD_HEADER_CRIT_ITEM;
      lonejson__jwt_visit_buffer_reset(state);
      return LONEJSON_STATUS_OK;
    }
    if (lonejson__jwt_path_is(path, "x5t")) {
      return lonejson__jwt_begin_string_field(state,
                                              LONEJSON__JWT_FIELD_HEADER_X5T,
                                              &state->seen_x5t, "x5t", error);
    }
    if (lonejson__jwt_path_is(path, "x5t#S256")) {
      return lonejson__jwt_begin_string_field(
          state, LONEJSON__JWT_FIELD_HEADER_X5T_S256, &state->seen_x5t_s256,
          "x5t#S256", error);
    }
    if (lonejson__jwt_path_is_array_item(path, "x5c")) {
      state->active = LONEJSON__JWT_FIELD_HEADER_X5C_ITEM;
      lonejson__jwt_visit_buffer_reset(state);
      return LONEJSON_STATUS_OK;
    }
    return LONEJSON_STATUS_OK;
  }
  if (lonejson__jwt_path_is(path, "iss")) {
    return lonejson__jwt_begin_string_field(state, LONEJSON__JWT_FIELD_ISS,
                                            &state->seen_iss, "iss", error);
  }
  if (lonejson__jwt_path_is(path, "sub")) {
    return lonejson__jwt_begin_string_field(state, LONEJSON__JWT_FIELD_SUB,
                                            &state->seen_sub, "sub", error);
  }
  if (lonejson__jwt_path_is(path, "nonce")) {
    return lonejson__jwt_begin_string_field(state, LONEJSON__JWT_FIELD_NONCE,
                                            &state->seen_nonce, "nonce", error);
  }
  if (lonejson__jwt_path_is(path, "azp")) {
    return lonejson__jwt_begin_string_field(state, LONEJSON__JWT_FIELD_AZP,
                                            &state->seen_azp, "azp", error);
  }
  if (lonejson__jwt_path_is(path, "scope")) {
    return lonejson__jwt_begin_string_field(state, LONEJSON__JWT_FIELD_SCOPE,
                                            &state->seen_scope, "scope", error);
  }
  if (lonejson__jwt_path_is(path, "scp")) {
    return lonejson__jwt_begin_string_field(
        state, LONEJSON__JWT_FIELD_SCP_STRING, &state->seen_scp, "scp", error);
  }
  if (lonejson__jwt_path_is_array_item(path, "scp")) {
    state->active = LONEJSON__JWT_FIELD_SCP_ARRAY_ITEM;
    lonejson__jwt_visit_buffer_reset(state);
    return LONEJSON_STATUS_OK;
  }
  if (lonejson__jwt_path_is(path, "aud")) {
    return lonejson__jwt_begin_string_field(
        state, LONEJSON__JWT_FIELD_AUD_STRING, &state->seen_aud, "aud", error);
  }
  if (lonejson__jwt_path_is_aud_item(path)) {
    state->active = LONEJSON__JWT_FIELD_AUD_ARRAY_ITEM;
    lonejson__jwt_visit_buffer_reset(state);
    return LONEJSON_STATUS_OK;
  }
  if (lonejson__jwt_path_is(path, "exp") ||
      lonejson__jwt_path_is(path, "nbf") ||
      lonejson__jwt_path_is(path, "iat")) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_string_chunk(void *user, const lonejson_value_path *path,
                                 const char *data, size_t len,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  (void)path;
  if (state->active == LONEJSON__JWT_FIELD_NONE) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__jwt_visit_buffer_append(state, data, len, error);
}

static lonejson_status
lonejson__jwt_claim_string_end(void *user, const lonejson_value_path *path,
                               lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  char *value;
  (void)path;

  switch (state->active) {
  case LONEJSON__JWT_FIELD_HEADER_ALG:
    state->header->alg = lonejson__jwt_visit_buffer_take(state);
    if (state->header->alg == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_HEADER_KID:
    state->header->kid = lonejson__jwt_visit_buffer_take(state);
    if (state->header->kid == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_HEADER_TYP:
    state->header->typ = lonejson__jwt_visit_buffer_take(state);
    if (state->header->typ == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_HEADER_X5T:
    state->header->x5t = lonejson__jwt_visit_buffer_take(state);
    if (state->header->x5t == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_HEADER_X5T_S256:
    state->header->x5t_s256 = lonejson__jwt_visit_buffer_take(state);
    if (state->header->x5t_s256 == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_HEADER_CRIT_ITEM:
    value = lonejson__jwt_visit_buffer_take(state);
    if (value == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    if (lonejson__jwt_string_array_append(&state->header->crit, value, error) !=
        LONEJSON_STATUS_OK) {
      lonejson__owned_free(value);
      return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
    }
    break;
  case LONEJSON__JWT_FIELD_HEADER_X5C_ITEM:
    value = lonejson__jwt_visit_buffer_take(state);
    if (value == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    if (lonejson__jwt_string_array_append(&state->header->x5c, value, error) !=
        LONEJSON_STATUS_OK) {
      lonejson__owned_free(value);
      return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
    }
    break;
  case LONEJSON__JWT_FIELD_ISS:
    state->claims->iss = lonejson__jwt_visit_buffer_take(state);
    if (state->claims->iss == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_SUB:
    state->claims->sub = lonejson__jwt_visit_buffer_take(state);
    if (state->claims->sub == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_NONCE:
    state->claims->nonce = lonejson__jwt_visit_buffer_take(state);
    if (state->claims->nonce == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_AZP:
    state->claims->azp = lonejson__jwt_visit_buffer_take(state);
    if (state->claims->azp == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_SCOPE:
    state->claims->scope = lonejson__jwt_visit_buffer_take(state);
    if (state->claims->scope == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_SCP_STRING:
    value = lonejson__jwt_visit_buffer_take(state);
    if (value == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    if (lonejson__jwt_string_array_append(&state->claims->scp, value, error) !=
        LONEJSON_STATUS_OK) {
      lonejson__owned_free(value);
      return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
    }
    break;
  case LONEJSON__JWT_FIELD_AUD_STRING:
    state->claims->aud = lonejson__jwt_visit_buffer_take(state);
    if (state->claims->aud == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_AUD_ARRAY_ITEM:
  case LONEJSON__JWT_FIELD_SCP_ARRAY_ITEM:
    value = lonejson__jwt_visit_buffer_take(state);
    if (value == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    if (lonejson__jwt_string_array_append(
            state->active == LONEJSON__JWT_FIELD_AUD_ARRAY_ITEM
                ? &state->claims->aud_array
                : &state->claims->scp,
            value, error) != LONEJSON_STATUS_OK) {
      lonejson__owned_free(value);
      return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
    }
    break;
  default:
    break;
  }
  state->active = LONEJSON__JWT_FIELD_NONE;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_begin_number_field(lonejson__jwt_claim_visit *state,
                                 lonejson__jwt_claim_field field, int *seen,
                                 const char *name, lonejson_error *error) {
  if (*seen) {
    return lonejson__jwt_claim_duplicate_error(error, name);
  }
  *seen = 1;
  state->active = field;
  lonejson__jwt_visit_buffer_reset(state);
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_number_begin(void *user, const lonejson_value_path *path,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;

  if (state->header_mode) {
    if (lonejson__jwt_path_is_registered_header(path)) {
      return lonejson__jwt_claim_type_error(error, "registered");
    }
    return LONEJSON_STATUS_OK;
  }
  if (lonejson__jwt_path_is(path, "exp")) {
    return lonejson__jwt_begin_number_field(state, LONEJSON__JWT_FIELD_EXP,
                                            &state->seen_exp, "exp", error);
  }
  if (lonejson__jwt_path_is(path, "nbf")) {
    return lonejson__jwt_begin_number_field(state, LONEJSON__JWT_FIELD_NBF,
                                            &state->seen_nbf, "nbf", error);
  }
  if (lonejson__jwt_path_is(path, "iat")) {
    return lonejson__jwt_begin_number_field(state, LONEJSON__JWT_FIELD_IAT,
                                            &state->seen_iat, "iat", error);
  }
  if (lonejson__jwt_path_is(path, "iss") ||
      lonejson__jwt_path_is_registered_claim(path)) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_number_chunk(void *user, const lonejson_value_path *path,
                                 const char *data, size_t len,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  (void)path;
  if (state->active != LONEJSON__JWT_FIELD_EXP &&
      state->active != LONEJSON__JWT_FIELD_NBF &&
      state->active != LONEJSON__JWT_FIELD_IAT) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__jwt_visit_buffer_append(state, data, len, error);
}

static lonejson_status
lonejson__jwt_claim_number_end(void *user, const lonejson_value_path *path,
                               lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  lonejson_int64 value;
  (void)path;

  if (state->active != LONEJSON__JWT_FIELD_EXP &&
      state->active != LONEJSON__JWT_FIELD_NBF &&
      state->active != LONEJSON__JWT_FIELD_IAT) {
    state->active = LONEJSON__JWT_FIELD_NONE;
    return LONEJSON_STATUS_OK;
  }
  if (state->buffer == NULL ||
      !lonejson__parse_i64_token(state->buffer, &value)) {
    state->active = LONEJSON__JWT_FIELD_NONE;
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT date claim must be an integer");
  }
  if (value < 0) {
    state->active = LONEJSON__JWT_FIELD_NONE;
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT date claim must be non-negative");
  }
  if (state->active == LONEJSON__JWT_FIELD_EXP) {
    state->claims->exp = value;
    state->claims->has_exp = 1;
  } else if (state->active == LONEJSON__JWT_FIELD_NBF) {
    state->claims->nbf = value;
    state->claims->has_nbf = 1;
  } else {
    state->claims->iat = value;
    state->claims->has_iat = 1;
  }
  state->active = LONEJSON__JWT_FIELD_NONE;
  lonejson__jwt_visit_buffer_reset(state);
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_object_begin(void *user, const lonejson_value_path *path,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  if (path != NULL && path->segment_count == 0u) {
    if (state->root_object_seen) {
      return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                                 0u, "invalid JWT claim visitor state");
    }
    state->root_object_seen = 1;
    return LONEJSON_STATUS_OK;
  }
  if (state->header_mode && lonejson__jwt_path_is_registered_header(path)) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  if (!state->header_mode && lonejson__jwt_path_is_registered_claim(path)) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_array_begin(void *user, const lonejson_value_path *path,
                                lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  if (state->header_mode && lonejson__jwt_path_is(path, "crit")) {
    if (state->seen_crit) {
      return lonejson__jwt_claim_duplicate_error(error, "crit");
    }
    state->seen_crit = 1;
    return LONEJSON_STATUS_OK;
  }
  if (state->header_mode && lonejson__jwt_path_is(path, "x5c")) {
    if (state->seen_x5c) {
      return lonejson__jwt_claim_duplicate_error(error, "x5c");
    }
    state->seen_x5c = 1;
    return LONEJSON_STATUS_OK;
  }
  if (!state->header_mode && lonejson__jwt_path_is(path, "aud")) {
    if (state->seen_aud) {
      return lonejson__jwt_claim_duplicate_error(error, "aud");
    }
    state->seen_aud = 1;
    return LONEJSON_STATUS_OK;
  }
  if (!state->header_mode && lonejson__jwt_path_is(path, "scp")) {
    if (state->seen_scp) {
      return lonejson__jwt_claim_duplicate_error(error, "scp");
    }
    state->seen_scp = 1;
    return LONEJSON_STATUS_OK;
  }
  if (state->header_mode && lonejson__jwt_path_is_registered_header(path)) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  if (!state->header_mode && lonejson__jwt_path_is_registered_claim(path)) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_literal_type(void *user, const lonejson_value_path *path,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  if (state->header_mode && lonejson__jwt_path_is_registered_header(path)) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  if (!state->header_mode && lonejson__jwt_path_is_registered_claim(path)) {
    return lonejson__jwt_claim_type_error(
        error, path->segment_count == 0u ? "root" : "registered");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__jwt_claim_bool(void *user,
                                                const lonejson_value_path *path,
                                                int value,
                                                lonejson_error *error) {
  (void)value;
  return lonejson__jwt_claim_literal_type(user, path, error);
}

static lonejson_path_value_visitor lonejson__jwt_claim_visitor(void) {
  lonejson_path_value_visitor visitor = lonejson_default_path_value_visitor();
  visitor.object_begin = lonejson__jwt_claim_object_begin;
  visitor.array_begin = lonejson__jwt_claim_array_begin;
  visitor.string_begin = lonejson__jwt_claim_string_begin;
  visitor.string_chunk = lonejson__jwt_claim_string_chunk;
  visitor.string_end = lonejson__jwt_claim_string_end;
  visitor.number_begin = lonejson__jwt_claim_number_begin;
  visitor.number_chunk = lonejson__jwt_claim_number_chunk;
  visitor.number_end = lonejson__jwt_claim_number_end;
  visitor.boolean_value = lonejson__jwt_claim_bool;
  visitor.null_value = lonejson__jwt_claim_literal_type;
  return visitor;
}

static void
lonejson__jwt_owned_string_array_cleanup(lonejson_string_array *array) {
  size_t i;
  if (array == NULL) {
    return;
  }
  for (i = 0u; i < array->count; ++i) {
    lonejson__owned_free(array->items[i]);
  }
  lonejson__owned_free(array->items);
  memset(array, 0, sizeof(*array));
}

void lonejson_jwt_header_init(lonejson_jwt_header *header) {
  if (header != NULL) {
    memset(header, 0, sizeof(*header));
  }
}

void lonejson_jwt_header_cleanup(lonejson_jwt_header *header) {
  if (header != NULL) {
    lonejson__owned_free(header->alg);
    lonejson__owned_free(header->kid);
    lonejson__owned_free(header->typ);
    lonejson__jwt_owned_string_array_cleanup(&header->crit);
    lonejson__owned_free(header->x5t);
    lonejson__owned_free(header->x5t_s256);
    lonejson__jwt_owned_string_array_cleanup(&header->x5c);
    memset(header, 0, sizeof(*header));
  }
}

void lonejson_jwt_claims_init(lonejson_jwt_claims *claims) {
  if (claims != NULL) {
    memset(claims, 0, sizeof(*claims));
  }
}

void lonejson_jwt_claims_cleanup(lonejson_jwt_claims *claims) {
  size_t i;
  if (claims != NULL) {
    lonejson__owned_free(claims->iss);
    lonejson__owned_free(claims->sub);
    lonejson__owned_free(claims->nonce);
    lonejson__owned_free(claims->aud);
    lonejson__owned_free(claims->azp);
    lonejson__owned_free(claims->scope);
    lonejson__jwt_owned_string_array_cleanup(&claims->scp);
    for (i = 0u; i < claims->aud_array.count; ++i) {
      lonejson__owned_free(claims->aud_array.items[i]);
    }
    lonejson__owned_free(claims->aud_array.items);
    memset(claims, 0, sizeof(*claims));
  }
}

static lonejson_status lonejson__jwt_parse_decoded_json(
    lonejson *runtime, const char *json, size_t len, int header_mode,
    lonejson_jwt_header *header, lonejson_jwt_claims *claims,
    lonejson_error *error) {
  lonejson__jwt_claim_visit state;
  lonejson_path_value_visitor visitor;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  state.header_mode = header_mode;
  state.header = header;
  state.claims = claims;
  visitor = lonejson__jwt_claim_visitor();
  status = lonejson_visit_path_value_buffer(runtime, json, len, &visitor,
                                            &state, error);
  lonejson__owned_free(state.buffer);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (!state.root_object_seen) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               header_mode ? "JWT header must be an object"
                                           : "JWT claims must be an object");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_header_validate(const lonejson_jwt_header *header,
                              lonejson_error *error) {
  size_t i;
  lonejson_status status;

  if (header == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWT header is required");
  }
  status = lonejson__jwk_check_base64url_member(header->x5t, "x5t", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwk_check_base64url_member(header->x5t_s256, "x5t#S256", 0,
                                                error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (i = 0u; i < header->x5c.count; ++i) {
    status =
        lonejson__jwk_check_base64_member(header->x5c.items[i], "x5c", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static size_t lonejson__jwt_limit_or_default(size_t configured,
                                             size_t fallback) {
  return configured == 0u ? fallback : configured;
}

lonejson_status lonejson_jwt_decode_compact(
    lonejson *runtime, const char *token, size_t len,
    const lonejson_jwt_claim_policy *limits, lonejson_jwt_header *header,
    lonejson_jwt_claims *claims, lonejson_error *error) {
  lonejson_jwt_compact compact;
  size_t header_len;
  size_t claims_len;
  size_t header_limit;
  size_t claims_limit;
  char *header_json;
  char *claims_json;
  size_t needed;
  lonejson_status status;

  lonejson__clear_error(error);
  if (header == NULL || claims == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JWT header and claims outputs are required");
  }
  if (limits != NULL && limits->max_token_bytes != 0u &&
      len > limits->max_token_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, len, 0u, 0u,
                               "JWT compact token exceeds configured limit");
  }
  lonejson_jwt_header_cleanup(header);
  lonejson_jwt_claims_cleanup(claims);
  status = lonejson_jwt_parse_compact(token, len, &compact, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      lonejson_base64_decoded_len(compact.header.data, compact.header.len,
                                  LONEJSON_BASE64_URL_RAW, &header_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      lonejson_base64_decoded_len(compact.payload.data, compact.payload.len,
                                  LONEJSON_BASE64_URL_RAW, &claims_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  header_limit = lonejson__jwt_limit_or_default(
      limits == NULL ? 0u : limits->max_decoded_header_bytes,
      LONEJSON__JWT_DEFAULT_MAX_HEADER_BYTES);
  claims_limit = lonejson__jwt_limit_or_default(
      limits == NULL ? 0u : limits->max_decoded_claims_bytes,
      LONEJSON__JWT_DEFAULT_MAX_CLAIMS_BYTES);
  if (header_len > header_limit || claims_len > claims_limit) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JWT decoded JSON exceeds configured limit");
  }
  header_json = (char *)lonejson__owned_malloc(NULL, header_len + 1u);
  claims_json = (char *)lonejson__owned_malloc(NULL, claims_len + 1u);
  if (header_json == NULL || claims_json == NULL) {
    lonejson__owned_free(header_json);
    lonejson__owned_free(claims_json);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate decoded JWT JSON");
  }
  status = lonejson_base64_decode(
      compact.header.data, compact.header.len, LONEJSON_BASE64_URL_RAW,
      (unsigned char *)header_json, header_len, &needed, error);
  if (status == LONEJSON_STATUS_OK) {
    header_json[header_len] = '\0';
    status = lonejson_base64_decode(
        compact.payload.data, compact.payload.len, LONEJSON_BASE64_URL_RAW,
        (unsigned char *)claims_json, claims_len, &needed, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    claims_json[claims_len] = '\0';
    status = lonejson__jwt_parse_decoded_json(runtime, header_json, header_len,
                                              1, header, claims, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__jwt_header_validate(header, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__jwt_parse_decoded_json(runtime, claims_json, claims_len,
                                              0, header, claims, error);
  }
  lonejson__owned_free(header_json);
  lonejson__owned_free(claims_json);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwt_header_cleanup(header);
    lonejson_jwt_claims_cleanup(claims);
  }
  return status;
}

static int lonejson__jwt_string_in_list(const char *value,
                                        const char *const *items,
                                        size_t count) {
  size_t i;
  if (value == NULL) {
    return 0;
  }
  for (i = 0u; i < count; ++i) {
    if (items[i] != NULL && strcmp(value, items[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static int
lonejson__jwt_string_array_contains(const lonejson_string_array *array,
                                    const char *value) {
  size_t i;
  if (array == NULL || value == NULL) {
    return 0;
  }
  for (i = 0u; i < array->count; ++i) {
    if (array->items[i] != NULL && strcmp(array->items[i], value) == 0) {
      return 1;
    }
  }
  return 0;
}

static int lonejson__jwt_claim_has_audience(const lonejson_jwt_claims *claims,
                                            const char *audience) {
  size_t i;
  if (claims->aud != NULL && strcmp(claims->aud, audience) == 0) {
    return 1;
  }
  for (i = 0u; i < claims->aud_array.count; ++i) {
    if (claims->aud_array.items[i] != NULL &&
        strcmp(claims->aud_array.items[i], audience) == 0) {
      return 1;
    }
  }
  return 0;
}

static size_t
lonejson__jwt_claim_audience_count(const lonejson_jwt_claims *claims) {
  if (claims == NULL) {
    return 0u;
  }
  if (claims->aud_array.count != 0u) {
    return claims->aud_array.count;
  }
  return claims->aud == NULL ? 0u : 1u;
}

static int
lonejson__jwt_any_audience_accepted(const lonejson_jwt_claims *claims,
                                    const lonejson_jwt_claim_policy *policy) {
  size_t i;
  for (i = 0u; i < policy->accepted_audience_count; ++i) {
    if (policy->accepted_audiences[i] != NULL &&
        lonejson__jwt_claim_has_audience(claims,
                                         policy->accepted_audiences[i])) {
      return 1;
    }
  }
  return 0;
}

static int
lonejson__jwt_all_audiences_accepted(const lonejson_jwt_claims *claims,
                                     const lonejson_jwt_claim_policy *policy) {
  size_t i;
  if (claims->aud != NULL &&
      !lonejson__jwt_string_in_list(claims->aud, policy->accepted_audiences,
                                    policy->accepted_audience_count)) {
    return 0;
  }
  for (i = 0u; i < claims->aud_array.count; ++i) {
    if (!lonejson__jwt_string_in_list(claims->aud_array.items[i],
                                      policy->accepted_audiences,
                                      policy->accepted_audience_count)) {
      return 0;
    }
  }
  return lonejson__jwt_claim_audience_count(claims) != 0u;
}

static int lonejson__jwt_numeric_date_at_least_delta(lonejson_int64 later,
                                                     lonejson_int64 earlier,
                                                     lonejson_int64 delta) {
  lonejson_int64 max_i64 = (lonejson_int64)(LONEJSON_UINT64_MAX >> 1);
  lonejson_int64 min_i64 = -max_i64 - 1;

  if (later < earlier || delta < 0) {
    return 0;
  }
  if (earlier < 0 && later > max_i64 + earlier) {
    return 1;
  }
  if (earlier > 0 && later < min_i64 + earlier) {
    return 0;
  }
  return later - earlier >= delta;
}

static int lonejson__jwt_scope_list_has(const char *scope_list,
                                        const char *required) {
  const char *cursor;
  const char *end;
  size_t required_len;

  if (scope_list == NULL || required == NULL || required[0] == '\0') {
    return 0;
  }
  required_len = strlen(required);
  cursor = scope_list;
  while (*cursor != '\0') {
    while (*cursor == ' ') {
      ++cursor;
    }
    end = cursor;
    while (*end != '\0' && *end != ' ') {
      ++end;
    }
    if ((size_t)(end - cursor) == required_len &&
        memcmp(cursor, required, required_len) == 0) {
      return 1;
    }
    cursor = end;
  }
  return 0;
}

static int lonejson__jwt_claim_has_scope(const lonejson_jwt_claims *claims,
                                         const char *required) {
  size_t i;
  if (claims == NULL || required == NULL) {
    return 0;
  }
  if (lonejson__jwt_scope_list_has(claims->scope, required)) {
    return 1;
  }
  for (i = 0u; i < claims->scp.count; ++i) {
    if (claims->scp.items[i] != NULL &&
        (strcmp(claims->scp.items[i], required) == 0 ||
         lonejson__jwt_scope_list_has(claims->scp.items[i], required))) {
      return 1;
    }
  }
  return 0;
}

static lonejson_status
lonejson__jwt_validate_jwk_signature_use(const lonejson_jwk *jwk,
                                         lonejson_error *error) {
  if (jwk->use != NULL && strcmp(jwk->use, "sig") != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK use is not valid for signatures");
  }
  if (jwk->key_ops.count != 0u &&
      !lonejson__jwt_string_array_contains(&jwk->key_ops, "verify")) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK key_ops does not allow verify");
  }
  return LONEJSON_STATUS_OK;
}

static int lonejson__jwt_has_required_claim(const lonejson_jwt_claims *claims,
                                            const char *name) {
  if (strcmp(name, "iss") == 0) {
    return claims->iss != NULL;
  }
  if (strcmp(name, "sub") == 0) {
    return claims->sub != NULL;
  }
  if (strcmp(name, "nonce") == 0) {
    return claims->nonce != NULL;
  }
  if (strcmp(name, "azp") == 0) {
    return claims->azp != NULL;
  }
  if (strcmp(name, "scope") == 0) {
    return claims->scope != NULL;
  }
  if (strcmp(name, "scp") == 0) {
    return claims->scp.count != 0u;
  }
  if (strcmp(name, "aud") == 0) {
    return claims->aud != NULL || claims->aud_array.count != 0u;
  }
  if (strcmp(name, "exp") == 0) {
    return claims->has_exp;
  }
  if (strcmp(name, "nbf") == 0) {
    return claims->has_nbf;
  }
  if (strcmp(name, "iat") == 0) {
    return claims->has_iat;
  }
  return 0;
}

lonejson_status lonejson_jwt_validate_claims(
    const lonejson_jwt_header *header, const lonejson_jwt_claims *claims,
    const lonejson_jwt_claim_policy *policy, lonejson_error *error) {
  size_t i;
  lonejson_int64 skew;

  lonejson__clear_error(error);
  if (header == NULL || claims == NULL || policy == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JWT header, claims, and policy are required");
  }
  if (policy->accepted_algs == NULL || policy->accepted_alg_count == 0u ||
      policy->accepted_issuers == NULL || policy->accepted_issuer_count == 0u ||
      policy->accepted_audiences == NULL ||
      policy->accepted_audience_count == 0u || policy->now < 0 ||
      policy->allowed_clock_skew < 0 ||
      (policy->accepted_crit_count != 0u && policy->accepted_crit == NULL) ||
      (policy->required_scope_count != 0u && policy->required_scopes == NULL)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWT claim policy is incomplete");
  }
  if (header->alg == NULL || strcmp(header->alg, "none") == 0 ||
      !lonejson__jwt_string_in_list(header->alg, policy->accepted_algs,
                                    policy->accepted_alg_count)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT algorithm is not accepted");
  }
  if (!lonejson__jwt_string_in_list(claims->iss, policy->accepted_issuers,
                                    policy->accepted_issuer_count)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT issuer is not accepted");
  }
  if (!lonejson__jwt_any_audience_accepted(claims, policy)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT audience is not accepted");
  }
  if (policy->require_all_audiences_accepted &&
      !lonejson__jwt_all_audiences_accepted(claims, policy)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT audience is not accepted");
  }
  if (policy->expected_nonce != NULL &&
      (claims->nonce == NULL ||
       strcmp(claims->nonce, policy->expected_nonce) != 0)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT nonce is not accepted");
  }
  if (policy->expected_azp != NULL &&
      (claims->azp == NULL || strcmp(claims->azp, policy->expected_azp) != 0)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT authorized party is not accepted");
  }
  if (policy->require_azp_when_multiple_audiences &&
      lonejson__jwt_claim_audience_count(claims) > 1u &&
      (claims->azp == NULL || claims->azp[0] == '\0')) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT authorized party is required");
  }
  for (i = 0u; i < header->crit.count; ++i) {
    if (header->crit.items[i] == NULL || header->crit.items[i][0] == '\0' ||
        !lonejson__jwt_string_in_list(header->crit.items[i],
                                      policy->accepted_crit,
                                      policy->accepted_crit_count)) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
          "JWT critical header parameter is unsupported");
    }
  }
  for (i = 0u; i < policy->required_scope_count; ++i) {
    if (policy->required_scopes[i] == NULL ||
        !lonejson__jwt_claim_has_scope(claims, policy->required_scopes[i])) {
      return lonejson__set_error(error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                                 0u, 0u, 0u, "JWT required scope is missing");
    }
  }
  for (i = 0u; i < policy->required_claim_count; ++i) {
    if (policy->required_claims == NULL || policy->required_claims[i] == NULL ||
        !lonejson__jwt_has_required_claim(claims, policy->required_claims[i])) {
      return lonejson__set_error(error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                                 0u, 0u, 0u, "JWT required claim is missing");
    }
  }
  skew = policy->allowed_clock_skew;
  if (claims->has_exp && lonejson__jwt_numeric_date_at_least_delta(
                             policy->now, claims->exp, skew)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT is expired");
  }
  if (claims->has_nbf && policy->now < claims->nbf &&
      claims->nbf - policy->now > skew) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT is not yet valid");
  }
  if (claims->has_iat && policy->now < claims->iat &&
      claims->iat - policy->now > skew) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT issued-at time is in the future");
  }
  return LONEJSON_STATUS_OK;
}

#if defined(LONEJSON_INTERNAL_OPENSSL_IMPLEMENTATION)
static unsigned char *
lonejson__jwt_decode_base64url_alloc(const char *data, size_t len,
                                     size_t *out_len, const char *what,
                                     lonejson_error *error) {
  unsigned char *out;
  size_t decoded_len;
  size_t needed;
  lonejson_status status;

  status = lonejson_base64_decoded_len(data, len, LONEJSON_BASE64_URL_RAW,
                                       &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return NULL;
  }
  out = (unsigned char *)lonejson__owned_malloc(
      NULL, decoded_len == 0u ? 1u : decoded_len);
  if (out == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate decoded JWT %s", what);
    return NULL;
  }
  status = lonejson_base64_decode(data, len, LONEJSON_BASE64_URL_RAW, out,
                                  decoded_len, &needed, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__owned_free(out);
    return NULL;
  }
  *out_len = decoded_len;
  return out;
}

static unsigned char *
lonejson__jwt_decode_base64_alloc(const char *data, size_t len, size_t *out_len,
                                  const char *what, lonejson_error *error) {
  unsigned char *out;
  size_t decoded_len;
  size_t needed;
  lonejson_status status;

  status = lonejson_base64_decoded_len(data, len, LONEJSON_BASE64_STANDARD,
                                       &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return NULL;
  }
  out = (unsigned char *)lonejson__owned_malloc(
      NULL, decoded_len == 0u ? 1u : decoded_len);
  if (out == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate decoded JWT %s", what);
    return NULL;
  }
  status = lonejson_base64_decode(data, len, LONEJSON_BASE64_STANDARD, out,
                                  decoded_len, &needed, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__owned_free(out);
    return NULL;
  }
  *out_len = decoded_len;
  return out;
}

static int lonejson__jwt_digest_matches_b64url(const char *encoded,
                                               const unsigned char *digest,
                                               size_t digest_len) {
  unsigned char *decoded;
  size_t decoded_len = 0u;
  lonejson_error error;
  int ok;

  if (encoded == NULL) {
    return 1;
  }
  lonejson_error_init(&error);
  decoded = lonejson__jwt_decode_base64url_alloc(
      encoded, strlen(encoded), &decoded_len, "thumbprint", &error);
  if (decoded == NULL) {
    return 0;
  }
  ok = decoded_len == digest_len && memcmp(decoded, digest, digest_len) == 0;
  lonejson__owned_free(decoded);
  return ok;
}

static X509 *lonejson__jwt_x509_from_x5c_item(const char *item,
                                              lonejson_error *error) {
  unsigned char *der;
  const unsigned char *cursor;
  size_t der_len = 0u;
  X509 *cert;

  if (item == NULL || item[0] == '\0') {
    (void)lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                              "JWK x5c certificate must not be empty");
    return NULL;
  }
  der = lonejson__jwt_decode_base64_alloc(item, strlen(item), &der_len,
                                          "x5c certificate", error);
  if (der == NULL) {
    return NULL;
  }
  if (der_len > (size_t)LONG_MAX) {
    lonejson__owned_free(der);
    (void)lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                              "JWK x5c certificate is too large");
    return NULL;
  }
  cursor = der;
  cert = d2i_X509(NULL, &cursor, (long)der_len);
  lonejson__owned_free(der);
  if (cert == NULL || cursor == NULL) {
    X509_free(cert);
    (void)lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                              "JWK x5c certificate is invalid");
    return NULL;
  }
  return cert;
}

static EVP_PKEY *lonejson__jwt_rsa_public_key_from_jwk(const lonejson_jwk *jwk,
                                                       lonejson_error *error) {
  EVP_PKEY_CTX *ctx = NULL;
  EVP_PKEY *pkey = NULL;
  OSSL_PARAM_BLD *builder = NULL;
  OSSL_PARAM *params = NULL;
  BIGNUM *n_bn = NULL;
  BIGNUM *e_bn = NULL;
  unsigned char *n = NULL;
  unsigned char *e = NULL;
  size_t n_len = 0u;
  size_t e_len = 0u;
  int ok = 0;

  if (jwk->n == NULL || jwk->e == NULL || jwk->n[0] == '\0' ||
      jwk->e[0] == '\0') {
    (void)lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                              "RSA JWK n and e members are required");
    return NULL;
  }
  n = lonejson__jwt_decode_base64url_alloc(jwk->n, strlen(jwk->n), &n_len,
                                           "JWK modulus", error);
  e = lonejson__jwt_decode_base64url_alloc(jwk->e, strlen(jwk->e), &e_len,
                                           "JWK exponent", error);
  if (n == NULL || e == NULL || n_len == 0u || e_len == 0u) {
    if (n != NULL && n_len == 0u) {
      (void)lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                                "RSA JWK modulus must not be empty");
    } else if (e != NULL && e_len == 0u) {
      (void)lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                                "RSA JWK exponent must not be empty");
    }
    goto done;
  }
  if (n_len > LONEJSON__JWT_MAX_RSA_COMPONENT_BYTES ||
      e_len > LONEJSON__JWT_MAX_RSA_COMPONENT_BYTES) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                              "RSA JWK key material exceeds configured limit");
    goto done;
  }
  if (n_len > (size_t)INT_MAX || e_len > (size_t)INT_MAX) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                              "RSA JWK key material exceeds supported size");
    goto done;
  }
  n_bn = BN_bin2bn(n, (int)n_len, NULL);
  e_bn = BN_bin2bn(e, (int)e_len, NULL);
  if (n_bn == NULL || e_bn == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate RSA JWK key material");
    goto done;
  }
  builder = OSSL_PARAM_BLD_new();
  ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
  if (builder == NULL || ctx == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate RSA public key context");
    goto done;
  }
  if (!OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_N, n_bn) ||
      !OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_E, e_bn)) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate RSA public key params");
    goto done;
  }
  params = OSSL_PARAM_BLD_to_param(builder);
  if (params == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate RSA public key params");
    goto done;
  }
  if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
      EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0 ||
      pkey == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "RSA JWK public key is invalid");
    goto done;
  }
  ok = 1;

done:
  lonejson__owned_free(n);
  lonejson__owned_free(e);
  BN_free(n_bn);
  BN_free(e_bn);
  OSSL_PARAM_free(params);
  OSSL_PARAM_BLD_free(builder);
  EVP_PKEY_CTX_free(ctx);
  if (!ok) {
    EVP_PKEY_free(pkey);
    pkey = NULL;
  }
  return pkey;
}

static lonejson_status
lonejson__jwt_validate_rs256_signature(const lonejson_jwt_compact *jwt,
                                       const lonejson_jwk *jwk, int pss,
                                       lonejson_error *error) {
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *md = NULL;
  EVP_PKEY_CTX *pkey_ctx = NULL;
  unsigned char *signature = NULL;
  size_t signature_len = 0u;
  int verify_result;

  signature = lonejson__jwt_decode_base64url_alloc(
      jwt->signature.data, jwt->signature.len, &signature_len, "signature",
      error);
  if (signature == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
  }
  if (signature_len == 0u) {
    lonejson__owned_free(signature);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT signature must not be empty");
  }
  pkey = lonejson__jwt_rsa_public_key_from_jwk(jwk, error);
  if (pkey == NULL) {
    lonejson__owned_free(signature);
    return error != NULL ? error->code : LONEJSON_STATUS_TYPE_MISMATCH;
  }
  md = EVP_MD_CTX_new();
  if (md == NULL) {
    lonejson__owned_free(signature);
    EVP_PKEY_free(pkey);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate JWT verifier");
  }
  if (EVP_DigestVerifyInit(md, &pkey_ctx, EVP_sha256(), NULL, pkey) <= 0 ||
      EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, pss ? RSA_PKCS1_PSS_PADDING
                                                 : RSA_PKCS1_PADDING) <= 0 ||
      (pss && EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, 32) <= 0)) {
    lonejson__owned_free(signature);
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(pkey);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "failed to initialize JWT verifier");
  }
  verify_result = EVP_DigestVerify(
      md, signature, signature_len,
      (const unsigned char *)jwt->signing_input.data, jwt->signing_input.len);
  lonejson__owned_free(signature);
  EVP_MD_CTX_free(md);
  EVP_PKEY_free(pkey);
  if (verify_result == 1) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "JWT signature validation failed");
}

static EVP_PKEY *lonejson__jwt_ec_public_key_from_jwk(const lonejson_jwk *jwk,
                                                      lonejson_error *error) {
  EVP_PKEY_CTX *ctx = NULL;
  EVP_PKEY *pkey = NULL;
  OSSL_PARAM_BLD *builder = NULL;
  OSSL_PARAM *params = NULL;
  unsigned char *x = NULL;
  unsigned char *y = NULL;
  unsigned char pub[65];
  size_t x_len = 0u;
  size_t y_len = 0u;
  int ok = 0;

  if (!lonejson__auth_streq(jwk->crv, "P-256")) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "ES256 requires a P-256 JWK");
    return NULL;
  }
  if (jwk->x == NULL || jwk->x[0] == '\0' || jwk->y == NULL ||
      jwk->y[0] == '\0') {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "ES256 JWK coordinates are required");
    return NULL;
  }
  x = lonejson__jwt_decode_base64url_alloc(jwk->x, strlen(jwk->x), &x_len,
                                           "JWK x coordinate", error);
  y = lonejson__jwt_decode_base64url_alloc(jwk->y, strlen(jwk->y), &y_len,
                                           "JWK y coordinate", error);
  if (x == NULL || y == NULL) {
    goto done;
  }
  if (x_len != 32u || y_len != 32u) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "ES256 JWK coordinates must be 32 bytes");
    goto done;
  }
  pub[0] = 0x04u;
  memcpy(pub + 1u, x, 32u);
  memcpy(pub + 33u, y, 32u);
  builder = OSSL_PARAM_BLD_new();
  ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
  if (builder == NULL || ctx == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate EC public key context");
    goto done;
  }
  if (!OSSL_PARAM_BLD_push_utf8_string(builder, OSSL_PKEY_PARAM_GROUP_NAME,
                                       "prime256v1", 0u) ||
      !OSSL_PARAM_BLD_push_octet_string(builder, OSSL_PKEY_PARAM_PUB_KEY, pub,
                                        sizeof(pub))) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate EC public key params");
    goto done;
  }
  params = OSSL_PARAM_BLD_to_param(builder);
  if (params == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate EC public key params");
    goto done;
  }
  if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
      EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0 ||
      pkey == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "EC JWK public key is invalid");
    goto done;
  }
  ok = 1;

done:
  lonejson__owned_free(x);
  lonejson__owned_free(y);
  OSSL_PARAM_free(params);
  OSSL_PARAM_BLD_free(builder);
  EVP_PKEY_CTX_free(ctx);
  if (!ok) {
    EVP_PKEY_free(pkey);
    pkey = NULL;
  }
  return pkey;
}

static unsigned char *
lonejson__jwt_es256_der_signature_alloc(const unsigned char *signature,
                                        size_t signature_len, size_t *out_len,
                                        lonejson_error *error) {
  ECDSA_SIG *sig = NULL;
  BIGNUM *r = NULL;
  BIGNUM *s = NULL;
  unsigned char *der = NULL;
  unsigned char *cursor;
  int der_len;

  if (signature_len != 64u) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "ES256 signature must be 64 bytes");
    return NULL;
  }
  sig = ECDSA_SIG_new();
  r = BN_bin2bn(signature, 32, NULL);
  s = BN_bin2bn(signature + 32u, 32, NULL);
  if (sig == NULL || r == NULL || s == NULL) {
    ECDSA_SIG_free(sig);
    BN_free(r);
    BN_free(s);
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate ES256 signature");
    return NULL;
  }
  if (ECDSA_SIG_set0(sig, r, s) != 1) {
    ECDSA_SIG_free(sig);
    BN_free(r);
    BN_free(s);
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to prepare ES256 signature");
    return NULL;
  }
  r = NULL;
  s = NULL;
  der_len = i2d_ECDSA_SIG(sig, NULL);
  if (der_len <= 0) {
    ECDSA_SIG_free(sig);
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "ES256 signature is invalid");
    return NULL;
  }
  der = (unsigned char *)lonejson__owned_malloc(NULL, (size_t)der_len);
  if (der == NULL) {
    ECDSA_SIG_free(sig);
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate ES256 signature");
    return NULL;
  }
  cursor = der;
  der_len = i2d_ECDSA_SIG(sig, &cursor);
  ECDSA_SIG_free(sig);
  if (der_len <= 0) {
    lonejson__owned_free(der);
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "ES256 signature is invalid");
    return NULL;
  }
  *out_len = (size_t)der_len;
  return der;
}

static lonejson_status
lonejson__jwt_validate_es256_signature(const lonejson_jwt_compact *jwt,
                                       const lonejson_jwk *jwk,
                                       lonejson_error *error) {
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *md = NULL;
  unsigned char *signature = NULL;
  unsigned char *der_signature = NULL;
  size_t signature_len = 0u;
  size_t der_signature_len = 0u;
  int verify_result;

  signature = lonejson__jwt_decode_base64url_alloc(
      jwt->signature.data, jwt->signature.len, &signature_len, "signature",
      error);
  if (signature == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
  }
  der_signature = lonejson__jwt_es256_der_signature_alloc(
      signature, signature_len, &der_signature_len, error);
  lonejson__owned_free(signature);
  if (der_signature == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_TYPE_MISMATCH;
  }
  pkey = lonejson__jwt_ec_public_key_from_jwk(jwk, error);
  if (pkey == NULL) {
    lonejson__owned_free(der_signature);
    return error != NULL ? error->code : LONEJSON_STATUS_TYPE_MISMATCH;
  }
  md = EVP_MD_CTX_new();
  if (md == NULL) {
    lonejson__owned_free(der_signature);
    EVP_PKEY_free(pkey);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate JWT verifier");
  }
  if (EVP_DigestVerifyInit(md, NULL, EVP_sha256(), NULL, pkey) <= 0) {
    lonejson__owned_free(der_signature);
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(pkey);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "failed to initialize JWT verifier");
  }
  verify_result = EVP_DigestVerify(
      md, der_signature, der_signature_len,
      (const unsigned char *)jwt->signing_input.data, jwt->signing_input.len);
  lonejson__owned_free(der_signature);
  EVP_MD_CTX_free(md);
  EVP_PKEY_free(pkey);
  if (verify_result == 1) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "JWT signature validation failed");
}

static lonejson_status
lonejson__jwt_validate_eddsa_signature(const lonejson_jwt_compact *jwt,
                                       const lonejson_jwk *jwk,
                                       lonejson_error *error) {
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *md = NULL;
  unsigned char *x = NULL;
  unsigned char *signature = NULL;
  size_t x_len = 0u;
  size_t signature_len = 0u;
  int verify_result;

  if (!lonejson__auth_streq(jwk->crv, "Ed25519")) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "EdDSA requires an Ed25519 JWK");
  }
  if (jwk->x == NULL || jwk->x[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "Ed25519 JWK x coordinate is required");
  }
  x = lonejson__jwt_decode_base64url_alloc(jwk->x, strlen(jwk->x), &x_len,
                                           "JWK x coordinate", error);
  signature = lonejson__jwt_decode_base64url_alloc(
      jwt->signature.data, jwt->signature.len, &signature_len, "signature",
      error);
  if (x == NULL || signature == NULL) {
    lonejson__owned_free(x);
    lonejson__owned_free(signature);
    return error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
  }
  if (x_len != 32u || signature_len != 64u) {
    lonejson__owned_free(x);
    lonejson__owned_free(signature);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "Ed25519 JWK or signature length is invalid");
  }
  pkey = EVP_PKEY_new_raw_public_key_ex(NULL, "ED25519", NULL, x, x_len);
  lonejson__owned_free(x);
  if (pkey == NULL) {
    lonejson__owned_free(signature);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "Ed25519 JWK public key is invalid");
  }
  md = EVP_MD_CTX_new();
  if (md == NULL) {
    EVP_PKEY_free(pkey);
    lonejson__owned_free(signature);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate JWT verifier");
  }
  if (EVP_DigestVerifyInit(md, NULL, NULL, NULL, pkey) <= 0) {
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(pkey);
    lonejson__owned_free(signature);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "failed to initialize JWT verifier");
  }
  verify_result = EVP_DigestVerify(
      md, signature, signature_len,
      (const unsigned char *)jwt->signing_input.data, jwt->signing_input.len);
  EVP_MD_CTX_free(md);
  EVP_PKEY_free(pkey);
  lonejson__owned_free(signature);
  if (verify_result == 1) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "JWT signature validation failed");
}

static EVP_PKEY *
lonejson__jwt_eddsa_public_key_from_jwk(const lonejson_jwk *jwk,
                                        lonejson_error *error) {
  EVP_PKEY *pkey;
  unsigned char *x = NULL;
  size_t x_len = 0u;

  if (!lonejson__auth_streq(jwk->crv, "Ed25519")) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "EdDSA requires an Ed25519 JWK");
    return NULL;
  }
  if (jwk->x == NULL || jwk->x[0] == '\0') {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "Ed25519 JWK x coordinate is required");
    return NULL;
  }
  x = lonejson__jwt_decode_base64url_alloc(jwk->x, strlen(jwk->x), &x_len,
                                           "JWK x coordinate", error);
  if (x == NULL) {
    return NULL;
  }
  if (x_len != 32u) {
    lonejson__owned_free(x);
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "Ed25519 JWK length is invalid");
    return NULL;
  }
  pkey = EVP_PKEY_new_raw_public_key_ex(NULL, "ED25519", NULL, x, x_len);
  lonejson__owned_free(x);
  if (pkey == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "Ed25519 JWK public key is invalid");
  }
  return pkey;
}

static EVP_PKEY *
lonejson__jwt_public_key_from_jwk_for_alg(const lonejson_jwt_header *header,
                                          const lonejson_jwk *jwk,
                                          lonejson_error *error) {
  if (strcmp(header->alg, "RS256") == 0 || strcmp(header->alg, "PS256") == 0) {
    return lonejson__jwt_rsa_public_key_from_jwk(jwk, error);
  }
  if (strcmp(header->alg, "ES256") == 0) {
    return lonejson__jwt_ec_public_key_from_jwk(jwk, error);
  }
  if (strcmp(header->alg, "EdDSA") == 0) {
    return lonejson__jwt_eddsa_public_key_from_jwk(jwk, error);
  }
  (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                            "JWT signature algorithm is not supported");
  return NULL;
}

static lonejson_status
lonejson__jwt_validate_jwk_x5c(void *x509_store,
                               const lonejson_jwt_header *header,
                               const lonejson_jwk *jwk, lonejson_error *error) {
  STACK_OF(X509) *chain = NULL;
  X509 *leaf = NULL;
  X509 *cert = NULL;
  X509_STORE *store = NULL;
  X509_STORE_CTX *store_ctx = NULL;
  EVP_PKEY *cert_key = NULL;
  EVP_PKEY *jwk_key = NULL;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0u;
  size_t i;
  int owns_store = 0;
  int ok;
  lonejson_status status = LONEJSON_STATUS_OK;

  if (jwk->x5c.count == 0u) {
    return LONEJSON_STATUS_OK;
  }
  chain = sk_X509_new_null();
  if (chain == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate JWK x5c chain");
  }
  for (i = 0u; i < jwk->x5c.count; ++i) {
    cert = lonejson__jwt_x509_from_x5c_item(jwk->x5c.items[i], error);
    if (cert == NULL) {
      status = error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
      goto done;
    }
    if (i == 0u) {
      leaf = cert;
    } else if (!sk_X509_push(chain, cert)) {
      X509_free(cert);
      status =
          lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to append JWK x5c certificate");
      goto done;
    }
    cert = NULL;
  }
  if (leaf == NULL) {
    status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                                 0u, "JWK x5c chain is empty");
    goto done;
  }
  if (X509_digest(leaf, EVP_sha1(), digest, &digest_len) != 1 ||
      digest_len != 20u) {
    status = lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                                 0u, "failed to compute JWK x5c SHA-1");
    goto done;
  }
  if (!lonejson__jwt_digest_matches_b64url(jwk->x5t, digest, digest_len) ||
      !lonejson__jwt_digest_matches_b64url(header->x5t, digest, digest_len)) {
    status =
        lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                            "JWT x5t thumbprint does not match x5c leaf");
    goto done;
  }
  if (X509_digest(leaf, EVP_sha256(), digest, &digest_len) != 1 ||
      digest_len != 32u) {
    status = lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                                 0u, "failed to compute JWK x5c SHA-256");
    goto done;
  }
  if (!lonejson__jwt_digest_matches_b64url(jwk->x5t_s256, digest, digest_len) ||
      !lonejson__jwt_digest_matches_b64url(header->x5t_s256, digest,
                                           digest_len)) {
    status =
        lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                            "JWT x5t#S256 thumbprint does not match x5c leaf");
    goto done;
  }
  cert_key = X509_get_pubkey(leaf);
  jwk_key = lonejson__jwt_public_key_from_jwk_for_alg(header, jwk, error);
  if (cert_key == NULL || jwk_key == NULL) {
    status =
        error != NULL && error->code != LONEJSON_STATUS_OK
            ? error->code
            : lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                                  0u, "JWK x5c leaf public key is invalid");
    goto done;
  }
  if (EVP_PKEY_eq(cert_key, jwk_key) != 1) {
    status = lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                                 0u, "JWK public key does not match x5c leaf");
    goto done;
  }
  if (x509_store != NULL) {
    store = (X509_STORE *)x509_store;
  } else {
    store = X509_STORE_new();
    owns_store = 1;
    if (store == NULL || X509_STORE_set_default_paths(store) != 1) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to initialize OpenSSL trust store");
      goto done;
    }
  }
  store_ctx = X509_STORE_CTX_new();
  if (store_ctx == NULL ||
      X509_STORE_CTX_init(store_ctx, store, leaf, chain) != 1) {
    status =
        lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                            0u, "failed to initialize JWK x5c verifier");
    goto done;
  }
  ok = X509_verify_cert(store_ctx);
  if (ok != 1) {
    status =
        lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                            "JWK x5c certificate chain is not trusted");
    goto done;
  }

done:
  EVP_PKEY_free(cert_key);
  EVP_PKEY_free(jwk_key);
  X509_STORE_CTX_free(store_ctx);
  if (owns_store) {
    X509_STORE_free(store);
  }
  X509_free(leaf);
  sk_X509_pop_free(chain, X509_free);
  return status;
}

static lonejson_status
lonejson__openssl_auth_verify_jws(void *user_data,
                                  const lonejson_jws_verify_request *request,
                                  lonejson_error *error) {
  const lonejson_jwt_compact *jwt;
  const lonejson_jwt_header *header;
  const lonejson_jwk *jwk;
  lonejson_status status;

  if (request == NULL || request->jwt == NULL || request->header == NULL ||
      request->jwk == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWS verification request is required");
  }
  jwt = request->jwt;
  header = request->header;
  jwk = request->jwk;
  status = lonejson__jwt_validate_jwk_x5c(user_data, header, jwk, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (strcmp(header->alg, "RS256") == 0 || strcmp(header->alg, "PS256") == 0) {
    if (!lonejson__auth_streq(jwk->kty, "RSA")) {
      return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                                 0u,
                                 "JWT JWK key type does not match algorithm");
    }
    return lonejson__jwt_validate_rs256_signature(
        jwt, jwk, strcmp(header->alg, "PS256") == 0, error);
  }
  if (strcmp(header->alg, "ES256") == 0) {
    if (!lonejson__auth_streq(jwk->kty, "EC")) {
      return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                                 0u,
                                 "JWT JWK key type does not match algorithm");
    }
    return lonejson__jwt_validate_es256_signature(jwt, jwk, error);
  }
  if (strcmp(header->alg, "EdDSA") == 0) {
    if (!lonejson__auth_streq(jwk->kty, "OKP")) {
      return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                                 0u,
                                 "JWT JWK key type does not match algorithm");
    }
    return lonejson__jwt_validate_eddsa_signature(jwt, jwk, error);
  }
  {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT signature algorithm is not supported");
  }
}

static lonejson_status
lonejson__openssl_auth_random_bytes(void *user_data, unsigned char *dst,
                                    size_t len, lonejson_error *error) {
  (void)user_data;
  if (dst == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "random byte output is required");
  }
  if (len > (size_t)INT_MAX) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "random byte request exceeds OpenSSL limit");
  }
  if (len != 0u && RAND_bytes(dst, (int)len) != 1) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "failed to generate random bytes");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__openssl_auth_sha256(void *user_data, const void *data, size_t len,
                              unsigned char out[32], lonejson_error *error) {
  unsigned int digest_len = 0u;

  (void)user_data;
  if ((data == NULL && len != 0u) || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "SHA-256 input and output are required");
  }
  if (EVP_Digest(data, len, out, &digest_len, EVP_sha256(), NULL) != 1 ||
      digest_len != 32u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "failed to compute SHA-256 digest");
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_auth_provider_init_openssl(
    lonejson_auth_provider *provider,
    const lonejson_openssl_auth_provider_config *config,
    lonejson_error *error) {
  lonejson__clear_error(error);
  if (provider == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "auth provider output is required");
  }
  if (config != NULL && (config->libctx != NULL || config->propq != NULL)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OpenSSL provider config fields are reserved");
  }
  memset(provider, 0, sizeof(*provider));
  provider->user_data = config != NULL ? config->x509_store : NULL;
  provider->verify_jws = lonejson__openssl_auth_verify_jws;
  provider->random_bytes = lonejson__openssl_auth_random_bytes;
  provider->sha256 = lonejson__openssl_auth_sha256;
  return LONEJSON_STATUS_OK;
}
#endif

lonejson_status lonejson_jwt_validate_signature(
    const lonejson_jwt_compact *jwt, const lonejson_jwt_header *header,
    const lonejson_jwk *jwk, lonejson_error *error) {
  lonejson_jws_verify_request request;
#if defined(LONEJSON_INTERNAL_OPENSSL_IMPLEMENTATION)
  lonejson_auth_provider provider;
#endif

  lonejson__clear_error(error);
  if (jwt == NULL || header == NULL || jwk == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWT, header, and JWK are required");
  }
  if (header->alg == NULL || strcmp(header->alg, "none") == 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT signature algorithm is not accepted");
  }
  if (jwk->alg != NULL && strcmp(jwk->alg, header->alg) != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK algorithm does not match header");
  }
  if (header->kid != NULL && jwk->kid != NULL &&
      strcmp(header->kid, jwk->kid) != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK key id does not match header");
  }
  if (lonejson__jwt_validate_jwk_signature_use(jwk, error) !=
      LONEJSON_STATUS_OK) {
    return error != NULL ? error->code : LONEJSON_STATUS_TYPE_MISMATCH;
  }
  request.jwt = jwt;
  request.header = header;
  request.jwk = jwk;
#if defined(LONEJSON_INTERNAL_OPENSSL_IMPLEMENTATION)
  if (lonejson_auth_provider_init_openssl(&provider, NULL, error) !=
      LONEJSON_STATUS_OK) {
    return error != NULL ? error->code : LONEJSON_STATUS_INTERNAL_ERROR;
  }
  return provider.verify_jws(provider.user_data, &request, error);
#else
  (void)request;
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "JWT signature validation requires an auth "
                             "crypto provider");
#endif
}

lonejson_status lonejson_jwt_validate_signature_with_runtime(
    lonejson *runtime, const lonejson_jwt_compact *jwt,
    const lonejson_jwt_header *header, const lonejson_jwk *jwk,
    lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson_jws_verify_request request;
  lonejson_status status;

  lonejson__clear_error(error);
  if (jwt == NULL || header == NULL || jwk == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWT, header, and JWK are required");
  }
  if (header->alg == NULL || strcmp(header->alg, "none") == 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT signature algorithm is not accepted");
  }
  if (jwk->alg != NULL && strcmp(jwk->alg, header->alg) != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK algorithm does not match header");
  }
  if (header->kid != NULL && jwk->kid != NULL &&
      strcmp(header->kid, jwk->kid) != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK key id does not match header");
  }
  if (lonejson__jwt_validate_jwk_signature_use(jwk, error) !=
      LONEJSON_STATUS_OK) {
    return error != NULL ? error->code : LONEJSON_STATUS_TYPE_MISMATCH;
  }
  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (!runtime_state->has_auth_provider ||
      runtime_state->auth_provider.verify_jws == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "runtime auth provider cannot verify JWS");
  }
  request.jwt = jwt;
  request.header = header;
  request.jwk = jwk;
  status = runtime_state->auth_provider.verify_jws(
      runtime_state->auth_provider.user_data, &request, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

#ifdef LONEJSON_WITH_OIDC
const char *lonejson_auth_failure_string(lonejson_auth_failure failure) {
  switch (failure) {
  case LONEJSON_AUTH_FAILURE_NONE:
    return "none";
  case LONEJSON_AUTH_FAILURE_MISSING_CREDENTIALS:
    return "missing_credentials";
  case LONEJSON_AUTH_FAILURE_MALFORMED_TOKEN:
    return "malformed_token";
  case LONEJSON_AUTH_FAILURE_CACHE_UNAVAILABLE:
    return "cache_unavailable";
  case LONEJSON_AUTH_FAILURE_KEY_NOT_FOUND:
    return "key_not_found";
  case LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE:
    return "invalid_signature";
  case LONEJSON_AUTH_FAILURE_EXPIRED_TOKEN:
    return "expired_token";
  case LONEJSON_AUTH_FAILURE_NOT_YET_VALID:
    return "not_yet_valid";
  case LONEJSON_AUTH_FAILURE_ISSUER_MISMATCH:
    return "issuer_mismatch";
  case LONEJSON_AUTH_FAILURE_AUDIENCE_MISMATCH:
    return "audience_mismatch";
  case LONEJSON_AUTH_FAILURE_CLAIMS_INVALID:
    return "claims_invalid";
  }
  return "unknown";
}

void lonejson_oidc_bearer_validation_init(
    lonejson_oidc_bearer_validation *validation) {
  if (validation == NULL) {
    return;
  }
  memset(validation, 0, sizeof(*validation));
  lonejson_jwt_header_init(&validation->header);
  lonejson_jwt_claims_init(&validation->claims);
}

void lonejson_oidc_bearer_validation_cleanup(
    lonejson_oidc_bearer_validation *validation) {
  if (validation == NULL) {
    return;
  }
  lonejson_jwt_header_cleanup(&validation->header);
  lonejson_jwt_claims_cleanup(&validation->claims);
  validation->failure = LONEJSON_AUTH_FAILURE_NONE;
  validation->jwk = NULL;
}

static int lonejson__oidc_http_ows(char ch) { return ch == ' ' || ch == '\t'; }

static int lonejson__oidc_ascii_prefix_case_equal(const char *a, const char *b,
                                                  size_t len) {
  size_t i;
  unsigned char ca;
  unsigned char cb;

  for (i = 0u; i < len; ++i) {
    ca = (unsigned char)a[i];
    cb = (unsigned char)b[i];
    if (ca >= (unsigned char)'A' && ca <= (unsigned char)'Z') {
      ca = (unsigned char)(ca - (unsigned char)'A' + (unsigned char)'a');
    }
    if (cb >= (unsigned char)'A' && cb <= (unsigned char)'Z') {
      cb = (unsigned char)(cb - (unsigned char)'A' + (unsigned char)'a');
    }
    if (ca != cb) {
      return 0;
    }
  }
  return 1;
}

lonejson_status
lonejson_oidc_authorization_bearer_token(const char *authorization_header,
                                         lonejson_jwt_segment *out,
                                         lonejson_error *error) {
  const char *cursor;
  const char *token_begin;
  const char *token_end;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "bearer token output is required");
  }
  out->data = NULL;
  out->len = 0u;
  if (authorization_header == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                               0u, 0u, 0u,
                               "Authorization bearer credential is required");
  }
  cursor = authorization_header;
  while (lonejson__oidc_http_ows(*cursor)) {
    ++cursor;
  }
  if (*cursor == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                               0u, 0u, 0u,
                               "Authorization bearer credential is required");
  }
  if (strlen(cursor) < 7u ||
      !lonejson__oidc_ascii_prefix_case_equal(cursor, "Bearer", 6u) ||
      !lonejson__oidc_http_ows(cursor[6])) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "Authorization header is not a Bearer token");
  }
  cursor += 7u;
  while (lonejson__oidc_http_ows(*cursor)) {
    ++cursor;
  }
  token_begin = cursor;
  while (*cursor != '\0' && !lonejson__oidc_http_ows(*cursor)) {
    ++cursor;
  }
  token_end = cursor;
  while (lonejson__oidc_http_ows(*cursor)) {
    ++cursor;
  }
  if (token_end == token_begin) {
    return lonejson__set_error(error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                               0u, 0u, 0u,
                               "Authorization bearer token is required");
  }
  if (*cursor != '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "Authorization bearer token has trailing data");
  }
  out->data = token_begin;
  out->len = (size_t)(token_end - token_begin);
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__oidc_bearer_fail(
    lonejson_oidc_bearer_validation *out, lonejson_auth_failure failure,
    lonejson_status status, lonejson_error *error, const char *message) {
  lonejson_oidc_bearer_validation_cleanup(out);
  out->failure = failure;
  if (error != NULL && error->code == LONEJSON_STATUS_OK) {
    (void)lonejson__set_error(error, status, 0u, 0u, 0u, "%s", message);
  }
  return status;
}

static lonejson_auth_failure lonejson__oidc_bearer_classify_claim_failure(
    const lonejson_jwt_header *header, const lonejson_jwt_claims *claims,
    const lonejson_jwt_claim_policy *policy) {
  lonejson_int64 skew;

  if (header == NULL || claims == NULL || policy == NULL ||
      policy->accepted_algs == NULL || policy->accepted_alg_count == 0u ||
      policy->accepted_issuers == NULL || policy->accepted_issuer_count == 0u ||
      policy->accepted_audiences == NULL ||
      policy->accepted_audience_count == 0u || policy->allowed_clock_skew < 0) {
    return LONEJSON_AUTH_FAILURE_CLAIMS_INVALID;
  }
  if (!lonejson__jwt_string_in_list(claims->iss, policy->accepted_issuers,
                                    policy->accepted_issuer_count)) {
    return LONEJSON_AUTH_FAILURE_ISSUER_MISMATCH;
  }
  if (!lonejson__jwt_any_audience_accepted(claims, policy)) {
    return LONEJSON_AUTH_FAILURE_AUDIENCE_MISMATCH;
  }
  skew = policy->allowed_clock_skew;
  if (claims->has_exp && lonejson__jwt_numeric_date_at_least_delta(
                             policy->now, claims->exp, skew)) {
    return LONEJSON_AUTH_FAILURE_EXPIRED_TOKEN;
  }
  if ((claims->has_nbf && policy->now < claims->nbf &&
       claims->nbf - policy->now > skew) ||
      (claims->has_iat && policy->now < claims->iat &&
       claims->iat - policy->now > skew)) {
    return LONEJSON_AUTH_FAILURE_NOT_YET_VALID;
  }
  return LONEJSON_AUTH_FAILURE_CLAIMS_INVALID;
}

lonejson_status lonejson_oidc_validate_bearer_token(
    lonejson *runtime, const lonejson_oidc_bearer_validation_request *request,
    lonejson_oidc_bearer_validation *out, lonejson_error *error) {
  lonejson_jwt_segment token;
  lonejson_jwt_compact compact;
  lonejson_jwk_select_options select_options;
  const lonejson_jwk *selected = NULL;
  lonejson_status status;

  lonejson__clear_error(error);
  if (runtime == NULL || request == NULL || out == NULL ||
      request->jwks_cache == NULL || request->jwks_policy == NULL ||
      request->claim_policy == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "runtime, bearer request, JWKS cache, JWKS policy, and claim policy "
        "are required");
  }
  lonejson_oidc_bearer_validation_cleanup(out);

  status = lonejson_oidc_authorization_bearer_token(
      request->authorization_header, &token, error);
  if (status != LONEJSON_STATUS_OK) {
    return lonejson__oidc_bearer_fail(
        out,
        status == LONEJSON_STATUS_MISSING_REQUIRED_FIELD
            ? LONEJSON_AUTH_FAILURE_MISSING_CREDENTIALS
            : LONEJSON_AUTH_FAILURE_MALFORMED_TOKEN,
        status, error, "Authorization bearer token is invalid");
  }

  status = lonejson_jwt_parse_compact(token.data, token.len, &compact, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_jwt_decode_compact(runtime, token.data, token.len,
                                         request->claim_policy, &out->header,
                                         &out->claims, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    return lonejson__oidc_bearer_fail(
        out, LONEJSON_AUTH_FAILURE_MALFORMED_TOKEN, status, error,
        "Authorization bearer JWT is malformed");
  }
  if (out->header.kid == NULL || out->header.kid[0] == '\0') {
    return lonejson__oidc_bearer_fail(
        out, LONEJSON_AUTH_FAILURE_KEY_NOT_FOUND, LONEJSON_STATUS_TYPE_MISMATCH,
        error, "JWT kid is required for JWKS bearer validation");
  }

  memset(&select_options, 0, sizeof(select_options));
  select_options.kid = out->header.kid;
  select_options.kty =
      (lonejson__auth_streq(out->header.alg, "RS256") ||
       lonejson__auth_streq(out->header.alg, "PS256"))
          ? "RSA"
          : (lonejson__auth_streq(out->header.alg, "ES256")
                 ? "EC"
                 : (lonejson__auth_streq(out->header.alg, "EdDSA") ? "OKP"
                                                                   : NULL));
  status = lonejson__oidc_jwks_cache_select_bearer_key(
      request->jwks_cache, request->jwks_policy, select_options.kid,
      select_options.kty, out->header.alg, &selected, error);
  if (status != LONEJSON_STATUS_OK) {
    return lonejson__oidc_bearer_fail(
        out, LONEJSON_AUTH_FAILURE_CACHE_UNAVAILABLE, status, error,
        "OIDC JWKS cache is unavailable for bearer validation");
  }
  if (selected == NULL) {
    return lonejson__oidc_bearer_fail(out, LONEJSON_AUTH_FAILURE_KEY_NOT_FOUND,
                                      LONEJSON_STATUS_TYPE_MISMATCH, error,
                                      "no JWKS key matches bearer JWT header");
  }

  status = lonejson_jwt_validate_signature_with_runtime(
      runtime, &compact, &out->header, selected, error);
  if (status != LONEJSON_STATUS_OK) {
    return lonejson__oidc_bearer_fail(
        out, LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE, status, error,
        "bearer JWT signature validation failed");
  }

  status = lonejson_jwt_validate_claims(&out->header, &out->claims,
                                        request->claim_policy, error);
  if (status != LONEJSON_STATUS_OK) {
    return lonejson__oidc_bearer_fail(
        out,
        lonejson__oidc_bearer_classify_claim_failure(&out->header, &out->claims,
                                                     request->claim_policy),
        status, error, "bearer JWT claims validation failed");
  }

  out->failure = LONEJSON_AUTH_FAILURE_NONE;
  out->jwk = selected;
  return LONEJSON_STATUS_OK;
}

typedef struct lonejson__m2m_record {
  char *client_id;
  char *secret_salt;
  char *secret_hash;
  char *api_key_salt;
  char *api_key_hash;
  int revoked;
  lonejson_json_value claim;
} lonejson__m2m_record;

typedef struct lonejson__m2m_signup_record {
  char *signup_id;
  char *secret_salt;
  char *secret_hash;
  int revoked;
  lonejson_json_value claim;
} lonejson__m2m_signup_record;

typedef struct lonejson__m2m_store_doc {
  lonejson_object_array credentials;
  lonejson_object_array signups;
} lonejson__m2m_store_doc;

static const lonejson_field lonejson__m2m_record_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(lonejson__m2m_record, client_id, "client_id"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson__m2m_record, secret_salt,
                                "secret_salt"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson__m2m_record, secret_hash,
                                "secret_hash"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson__m2m_record, api_key_salt,
                                "api_key_salt"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson__m2m_record, api_key_hash,
                                "api_key_hash"),
    LONEJSON_FIELD_BOOL(lonejson__m2m_record, revoked, "revoked"),
    {"claim", LONEJSON__KEY_LEN("claim"), LONEJSON__KEY_FIRST("claim"),
     LONEJSON__KEY_LAST("claim"), offsetof(lonejson__m2m_record, claim),
     LONEJSON_FIELD_KIND_JSON_VALUE, LONEJSON_STORAGE_FIXED,
     LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_REQUIRED | LONEJSON_FIELD_JSON_VALUE_DEFAULT_CAPTURE, 0u,
     0u, NULL, NULL, 0u, LONEJSON_SPOOL_CLASS_DEFAULT}};
LONEJSON_MAP_DEFINE(lonejson__m2m_record_map, lonejson__m2m_record,
                    lonejson__m2m_record_fields);

static const lonejson_field lonejson__m2m_signup_record_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(lonejson__m2m_signup_record, signup_id,
                                "signup_id"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson__m2m_signup_record, secret_salt,
                                "secret_salt"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson__m2m_signup_record, secret_hash,
                                "secret_hash"),
    LONEJSON_FIELD_BOOL(lonejson__m2m_signup_record, revoked, "revoked"),
    {"claim", LONEJSON__KEY_LEN("claim"), LONEJSON__KEY_FIRST("claim"),
     LONEJSON__KEY_LAST("claim"), offsetof(lonejson__m2m_signup_record, claim),
     LONEJSON_FIELD_KIND_JSON_VALUE, LONEJSON_STORAGE_FIXED,
     LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_REQUIRED | LONEJSON_FIELD_JSON_VALUE_DEFAULT_CAPTURE, 0u,
     0u, NULL, NULL, 0u, LONEJSON_SPOOL_CLASS_DEFAULT}};
LONEJSON_MAP_DEFINE(lonejson__m2m_signup_record_map,
                    lonejson__m2m_signup_record,
                    lonejson__m2m_signup_record_fields);

static const lonejson_field lonejson__m2m_store_doc_fields[] = {
    LONEJSON_FIELD_OBJECT_ARRAY_OMIT_EMPTY(lonejson__m2m_store_doc, credentials,
                                           "credentials", lonejson__m2m_record,
                                           &lonejson__m2m_record_map,
                                           LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_ARRAY_OMIT_EMPTY(
        lonejson__m2m_store_doc, signups, "signups",
        lonejson__m2m_signup_record, &lonejson__m2m_signup_record_map,
        LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(lonejson__m2m_store_doc_map, lonejson__m2m_store_doc,
                    lonejson__m2m_store_doc_fields);

static unsigned lonejson__m2m_modes_or_default(unsigned modes) {
  return modes == 0u ? LONEJSON_M2M_AUTH_DEFAULT : modes;
}

static void lonejson__m2m_store_cleanup(lonejson__m2m_store_doc *store) {
  if (store != NULL) {
    lonejson_cleanup(&lonejson__m2m_store_doc_map, store, NULL);
  }
}

static lonejson_status lonejson__m2m_require_crypto(
    lonejson *runtime, lonejson__runtime_borrow *borrow,
    const lonejson_runtime **runtime_state, lonejson_error *error) {
  *runtime_state = lonejson__require_runtime_borrow(runtime, borrow, error);
  if (*runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (!(*runtime_state)->has_auth_provider ||
      (*runtime_state)->auth_provider.random_bytes == NULL ||
      (*runtime_state)->auth_provider.sha256 == NULL) {
    lonejson__runtime_borrow_release(borrow);
    *runtime_state = NULL;
    return lonejson__set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
        "runtime auth provider must provide random_bytes and sha256");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__m2m_random_token(const lonejson_runtime *runtime_state, size_t bytes,
                           char **out, lonejson_error *error) {
  unsigned char random_bytes[32];

  if (bytes == 0u || bytes > sizeof(random_bytes) || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "invalid random token request");
  }
  *out = NULL;
  if (runtime_state->auth_provider.random_bytes(
          runtime_state->auth_provider.user_data, random_bytes, bytes, error) !=
      LONEJSON_STATUS_OK) {
    return error != NULL ? error->code : LONEJSON_STATUS_INTERNAL_ERROR;
  }
  *out = lonejson__base64url_encode_alloc(random_bytes, bytes, error);
  memset(random_bytes, 0, sizeof(random_bytes));
  return *out != NULL ? LONEJSON_STATUS_OK
                      : (error != NULL ? error->code
                                       : LONEJSON_STATUS_ALLOCATION_FAILED);
}

static lonejson_status
lonejson__m2m_hash_secret(const lonejson_runtime *runtime_state,
                          const char *salt, const char *secret, char **out_hash,
                          lonejson_error *error) {
  unsigned char salt_bytes[16];
  unsigned char digest[32];
  unsigned char *input = NULL;
  size_t salt_len;
  size_t needed;
  size_t secret_len;
  lonejson_status status;

  if (salt == NULL || secret == NULL || out_hash == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "salt, secret, and hash output required");
  }
  *out_hash = NULL;
  status =
      lonejson_base64_decode(salt, strlen(salt), LONEJSON_BASE64_URL_RAW,
                             salt_bytes, sizeof(salt_bytes), &needed, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  salt_len = needed;
  secret_len = strlen(secret);
  if (secret_len == 0u || secret_len > 4096u ||
      salt_len > SIZE_MAX - secret_len) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "secret length is invalid");
  }
  input = (unsigned char *)lonejson__owned_malloc(NULL, salt_len + secret_len);
  if (input == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate secret hash input");
  }
  memcpy(input, salt_bytes, salt_len);
  memcpy(input + salt_len, secret, secret_len);
  status = runtime_state->auth_provider.sha256(
      runtime_state->auth_provider.user_data, input, salt_len + secret_len,
      digest, error);
  memset(input, 0, salt_len + secret_len);
  lonejson__owned_free(input);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  *out_hash = lonejson__base64url_encode_alloc(digest, sizeof(digest), error);
  memset(digest, 0, sizeof(digest));
  return *out_hash != NULL
             ? LONEJSON_STATUS_OK
             : (error != NULL ? error->code
                              : LONEJSON_STATUS_ALLOCATION_FAILED);
}

static int lonejson__m2m_constant_equal(const char *a, const char *b) {
  size_t a_len;
  size_t b_len;
  size_t i;
  unsigned char diff = 0u;

  if (a == NULL || b == NULL) {
    return 0;
  }
  a_len = strlen(a);
  b_len = strlen(b);
  if (a_len != b_len) {
    return 0;
  }
  for (i = 0u; i < a_len; ++i) {
    diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
  }
  return diff == 0u;
}

static int lonejson__m2m_ascii_scheme_prefix(const char *value,
                                             const char *scheme) {
  unsigned char a;
  unsigned char b;

  if (value == NULL || scheme == NULL) {
    return 0;
  }
  while (*scheme != '\0') {
    a = (unsigned char)*value++;
    b = (unsigned char)*scheme++;
    if (a >= (unsigned char)'A' && a <= (unsigned char)'Z') {
      a = (unsigned char)(a - (unsigned char)'A' + (unsigned char)'a');
    }
    if (b >= (unsigned char)'A' && b <= (unsigned char)'Z') {
      b = (unsigned char)(b - (unsigned char)'A' + (unsigned char)'a');
    }
    if (a != b) {
      return 0;
    }
  }
  return 1;
}

static lonejson_status
lonejson__m2m_verify_hash(const lonejson_runtime *runtime_state,
                          const char *salt, const char *hash,
                          const char *secret, int *ok, lonejson_error *error) {
  char *computed = NULL;
  lonejson_status status;

  *ok = 0;
  status =
      lonejson__m2m_hash_secret(runtime_state, salt, secret, &computed, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__clear_error(error);
    return LONEJSON_STATUS_OK;
  }
  *ok = lonejson__m2m_constant_equal(computed, hash);
  lonejson__owned_free(computed);
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__m2m_append_json_string(lonejson_owned_buffer *out, const char *value,
                                 size_t max_bytes, lonejson_error *error) {
  static const char hex[] = "0123456789abcdef";
  const unsigned char *p = (const unsigned char *)value;
  char esc[6];
  lonejson_status status;

  status = lonejson__oauth2_form_append_raw(out, "\"", 1u, max_bytes, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  while (p != NULL && *p != '\0') {
    unsigned char ch = *p++;
    switch (ch) {
    case '"':
      status =
          lonejson__oauth2_form_append_raw(out, "\\\"", 2u, max_bytes, error);
      break;
    case '\\':
      status =
          lonejson__oauth2_form_append_raw(out, "\\\\", 2u, max_bytes, error);
      break;
    case '\b':
      status =
          lonejson__oauth2_form_append_raw(out, "\\b", 2u, max_bytes, error);
      break;
    case '\f':
      status =
          lonejson__oauth2_form_append_raw(out, "\\f", 2u, max_bytes, error);
      break;
    case '\n':
      status =
          lonejson__oauth2_form_append_raw(out, "\\n", 2u, max_bytes, error);
      break;
    case '\r':
      status =
          lonejson__oauth2_form_append_raw(out, "\\r", 2u, max_bytes, error);
      break;
    case '\t':
      status =
          lonejson__oauth2_form_append_raw(out, "\\t", 2u, max_bytes, error);
      break;
    default:
      if (ch < 0x20u) {
        esc[0] = '\\';
        esc[1] = 'u';
        esc[2] = '0';
        esc[3] = '0';
        esc[4] = hex[(ch >> 4u) & 0x0fu];
        esc[5] = hex[ch & 0x0fu];
        status = lonejson__oauth2_form_append_raw(out, esc, sizeof(esc),
                                                  max_bytes, error);
      } else {
        status = lonejson__oauth2_form_append_raw(out, (const char *)&ch, 1u,
                                                  max_bytes, error);
      }
      break;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return lonejson__oauth2_form_append_raw(out, "\"", 1u, max_bytes, error);
}

static lonejson_status
lonejson__m2m_append_member_string(lonejson_owned_buffer *out, const char *key,
                                   const char *value, int *has_member,
                                   size_t max_bytes, lonejson_error *error) {
  lonejson_status status;

  if (value == NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (*has_member) {
    status = lonejson__oauth2_form_append_raw(out, ",", 1u, max_bytes, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  status = lonejson__m2m_append_json_string(out, key, max_bytes, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_raw(out, ":", 1u, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__m2m_append_json_string(out, value, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    *has_member = 1;
  }
  return status;
}

static lonejson_status lonejson__m2m_build_record_json(
    const char *client_id, const char *secret_salt, const char *secret_hash,
    const char *api_key_salt, const char *api_key_hash, const char *claim_json,
    size_t claim_len, size_t max_bytes, lonejson_owned_buffer *out,
    lonejson_error *error) {
  int has_member = 0;
  lonejson_status status;

  lonejson_owned_buffer_free(out);
  status = lonejson__oauth2_form_append_raw(out, "{", 1u, max_bytes, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__m2m_append_member_string(out, "client_id", client_id,
                                                &has_member, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__m2m_append_member_string(out, "secret_salt", secret_salt,
                                                &has_member, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__m2m_append_member_string(out, "secret_hash", secret_hash,
                                                &has_member, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__m2m_append_member_string(
        out, "api_key_salt", api_key_salt, &has_member, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__m2m_append_member_string(
        out, "api_key_hash", api_key_hash, &has_member, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_raw(out, ",\"claim\":", 9u, max_bytes,
                                              error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_raw(out, claim_json, claim_len,
                                              max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_raw(out, "}", 1u, max_bytes, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(out);
  }
  return status;
}

static lonejson_status
lonejson__m2m_parse_store(lonejson *runtime, const lonejson_m2m_store *input,
                          lonejson__m2m_store_doc *store,
                          lonejson_error *error) {
  size_t max_bytes;
  lonejson_status status;

  if (input == NULL || input->json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "M2M credential store JSON is required");
  }
  max_bytes =
      input->max_store_bytes == 0u ? 1024u * 1024u : input->max_store_bytes;
  if (input->len > max_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "M2M credential store exceeds configured limit");
  }
  memset(store, 0, sizeof(*store));
  status = lonejson_parse_buffer(runtime, &lonejson__m2m_store_doc_map, store,
                                 input->json, input->len, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__m2m_store_cleanup(store);
  }
  return status;
}

void lonejson_m2m_credential_init(lonejson_m2m_credential *credential) {
  if (credential != NULL) {
    memset(credential, 0, sizeof(*credential));
    lonejson_owned_buffer_init(&credential->record_json);
  }
}

void lonejson_m2m_credential_cleanup(lonejson_m2m_credential *credential) {
  if (credential != NULL) {
    lonejson__owned_free(credential->client_id);
    lonejson__owned_free(credential->client_secret);
    lonejson__owned_free(credential->api_key);
    lonejson_owned_buffer_free(&credential->record_json);
    memset(credential, 0, sizeof(*credential));
  }
}

lonejson_status lonejson_m2m_credential_generate(
    lonejson *runtime, const lonejson_m2m_credential_request *request,
    lonejson_m2m_credential *out, lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  const char *claim_json;
  size_t claim_len;
  size_t max_record_bytes;
  unsigned modes;
  char *secret_salt = NULL;
  char *secret_hash = NULL;
  char *api_key_salt = NULL;
  char *api_key_hash = NULL;
  lonejson_status status;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL || request->claim_json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "M2M credential request is required");
  }
  lonejson_m2m_credential_cleanup(out);
  claim_json = request->claim_json;
  claim_len = request->claim_len == 0u ? strlen(request->claim_json)
                                       : request->claim_len;
  status = lonejson_validate_buffer(runtime, claim_json, claim_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  modes = lonejson__m2m_modes_or_default(request->auth_modes);
  if ((modes & ~LONEJSON_M2M_AUTH_DEFAULT) != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "unsupported M2M auth mode");
  }
  status =
      lonejson__m2m_require_crypto(runtime, &borrow, &runtime_state, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      lonejson__m2m_random_token(runtime_state, 16u, &out->client_id, error);
  if (status == LONEJSON_STATUS_OK && (modes & LONEJSON_M2M_AUTH_BASIC) != 0u) {
    status = lonejson__m2m_random_token(runtime_state, 32u, &out->client_secret,
                                        error);
  }
  if (status == LONEJSON_STATUS_OK &&
      (modes & LONEJSON_M2M_AUTH_BEARER) != 0u) {
    status =
        lonejson__m2m_random_token(runtime_state, 32u, &out->api_key, error);
  }
  if (status == LONEJSON_STATUS_OK && out->client_secret != NULL) {
    status =
        lonejson__m2m_random_token(runtime_state, 16u, &secret_salt, error);
  }
  if (status == LONEJSON_STATUS_OK && out->client_secret != NULL) {
    status = lonejson__m2m_hash_secret(runtime_state, secret_salt,
                                       out->client_secret, &secret_hash, error);
  }
  if (status == LONEJSON_STATUS_OK && out->api_key != NULL) {
    status =
        lonejson__m2m_random_token(runtime_state, 16u, &api_key_salt, error);
  }
  if (status == LONEJSON_STATUS_OK && out->api_key != NULL) {
    status = lonejson__m2m_hash_secret(runtime_state, api_key_salt,
                                       out->api_key, &api_key_hash, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    max_record_bytes = request->max_record_bytes == 0u
                           ? 64u * 1024u
                           : request->max_record_bytes;
    status = lonejson__m2m_build_record_json(
        out->client_id, secret_salt, secret_hash, api_key_salt, api_key_hash,
        claim_json, claim_len, max_record_bytes, &out->record_json, error);
  }
  lonejson__runtime_borrow_release(&borrow);
  lonejson__owned_free(secret_salt);
  lonejson__owned_free(secret_hash);
  lonejson__owned_free(api_key_salt);
  lonejson__owned_free(api_key_hash);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_m2m_credential_cleanup(out);
  }
  return status;
}

void lonejson_m2m_authentication_init(lonejson_m2m_authentication *auth) {
  if (auth != NULL) {
    memset(auth, 0, sizeof(*auth));
    auth->failure = LONEJSON_AUTH_FAILURE_MISSING_CREDENTIALS;
    lonejson_json_value_init(NULL, &auth->claim);
  }
}

void lonejson_m2m_authentication_cleanup(lonejson_m2m_authentication *auth) {
  if (auth != NULL) {
    lonejson__owned_free(auth->client_id);
    lonejson_json_value_cleanup(&auth->claim);
    memset(auth, 0, sizeof(*auth));
  }
}

static lonejson_status
lonejson__m2m_extract_bearer(const char *authorization_header, char **token,
                             lonejson_error *error) {
  const char *p;
  const char *begin;
  const char *end;
  size_t len;
  char *copy;

  *token = NULL;
  if (authorization_header == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                               0u, 0u, 0u, "Authorization header is required");
  }
  p = authorization_header;
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  if (!lonejson__m2m_ascii_scheme_prefix(p, "Bearer")) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "Authorization scheme is not Bearer");
  }
  p += strlen("Bearer");
  if (*p != ' ' && *p != '\t') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "Bearer token separator is required");
  }
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  begin = p;
  while (*p != '\0' && *p != ' ' && *p != '\t') {
    ++p;
  }
  end = p;
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  if (begin == end || *p != '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "Bearer token is malformed");
  }
  len = (size_t)(end - begin);
  copy = (char *)lonejson__owned_malloc(NULL, len + 1u);
  if (copy == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to copy Bearer token");
  }
  memcpy(copy, begin, len);
  copy[len] = '\0';
  *token = copy;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__m2m_basic_decode(const char *authorization_header, char **client_id,
                           char **client_secret, lonejson_error *error) {
  const char *p;
  const char *encoded;
  size_t len;
  size_t out_cap;
  unsigned char *decoded;
  size_t out_len;
  char *colon;
  lonejson_status status;

  *client_id = NULL;
  *client_secret = NULL;
  if (authorization_header == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                               0u, 0u, 0u, "Authorization header is required");
  }
  p = authorization_header;
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  if (!lonejson__m2m_ascii_scheme_prefix(p, "Basic")) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "Authorization scheme is not Basic");
  }
  p += strlen("Basic");
  if (*p != ' ' && *p != '\t') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "Basic credential separator is required");
  }
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  encoded = p;
  while (*p != '\0' && *p != ' ' && *p != '\t') {
    ++p;
  }
  len = (size_t)(p - encoded);
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  if (len == 0u || *p != '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "Basic credentials are malformed");
  }
  status = lonejson_base64_decoded_len(encoded, len, LONEJSON_BASE64_STANDARD,
                                       &out_cap, error);
  if (status != LONEJSON_STATUS_OK) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "Basic credentials are malformed");
  }
  if (out_cap == SIZE_MAX) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "Basic credential length overflows");
  }
  decoded = (unsigned char *)lonejson__owned_malloc(NULL, out_cap + 1u);
  if (decoded == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate Basic credentials");
  }
  status = lonejson_base64_decode(encoded, len, LONEJSON_BASE64_STANDARD,
                                  decoded, out_cap, &out_len, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__owned_free(decoded);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "Basic credentials are malformed");
  }
  decoded[out_len] = '\0';
  colon = strchr((char *)decoded, ':');
  if (colon == NULL || colon == (char *)decoded || colon[1] == '\0') {
    lonejson__owned_free(decoded);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "Basic credentials require client_id:secret");
  }
  *colon = '\0';
  *client_id = lonejson__owned_strdup(NULL, (char *)decoded);
  *client_secret = lonejson__owned_strdup(NULL, colon + 1);
  lonejson__owned_free(decoded);
  if (*client_id == NULL || *client_secret == NULL) {
    lonejson__owned_free(*client_id);
    lonejson__owned_free(*client_secret);
    *client_id = NULL;
    *client_secret = NULL;
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to copy Basic credentials");
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_m2m_verify_authorization(
    lonejson *runtime, const lonejson_m2m_verify_request *request,
    lonejson_m2m_authentication *out, lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson__m2m_store_doc store;
  unsigned modes;
  lonejson_status status;
  size_t i;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "M2M verify request and output required");
  }
  lonejson_m2m_authentication_cleanup(out);
  lonejson_m2m_authentication_init(out);
  modes = lonejson__m2m_modes_or_default(request->allowed_auth_modes);
  if ((modes & ~LONEJSON_M2M_AUTH_DEFAULT) != 0u) {
    out->failure = LONEJSON_AUTH_FAILURE_MALFORMED_TOKEN;
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "unsupported M2M auth mode");
  }
  status =
      lonejson__m2m_require_crypto(runtime, &borrow, &runtime_state, error);
  if (status != LONEJSON_STATUS_OK) {
    out->failure = LONEJSON_AUTH_FAILURE_CACHE_UNAVAILABLE;
    return status;
  }
  status = lonejson__m2m_parse_store(runtime, request->store, &store, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__runtime_borrow_release(&borrow);
    out->failure = LONEJSON_AUTH_FAILURE_CACHE_UNAVAILABLE;
    return status;
  }
  if ((modes & LONEJSON_M2M_AUTH_BASIC) != 0u) {
    char *client_id = NULL;
    char *client_secret = NULL;
    if (lonejson__m2m_basic_decode(request->authorization_header, &client_id,
                                   &client_secret,
                                   NULL) == LONEJSON_STATUS_OK) {
      for (i = 0u; i < store.credentials.count; ++i) {
        lonejson__m2m_record *record =
            &((lonejson__m2m_record *)store.credentials.items)[i];
        int ok = 0;
        if (record->revoked || record->client_id == NULL ||
            strcmp(record->client_id, client_id) != 0 ||
            record->secret_salt == NULL || record->secret_hash == NULL) {
          continue;
        }
        (void)lonejson__m2m_verify_hash(runtime_state, record->secret_salt,
                                        record->secret_hash, client_secret, &ok,
                                        NULL);
        if (ok) {
          out->client_id = lonejson__owned_strdup(NULL, record->client_id);
          if (out->client_id == NULL) {
            out->failure = LONEJSON_AUTH_FAILURE_CLAIMS_INVALID;
            lonejson__owned_free(client_id);
            lonejson__owned_free(client_secret);
            lonejson__m2m_store_cleanup(&store);
            lonejson__runtime_borrow_release(&borrow);
            return lonejson__set_error(
                error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                "failed to copy authenticated client id");
          }
          out->auth_mode = LONEJSON_M2M_AUTH_BASIC;
          status = lonejson_json_value_set_buffer(
              &out->claim, record->claim.json, record->claim.len, error);
          out->failure = status == LONEJSON_STATUS_OK
                             ? LONEJSON_AUTH_FAILURE_NONE
                             : LONEJSON_AUTH_FAILURE_CLAIMS_INVALID;
          lonejson__owned_free(client_id);
          lonejson__owned_free(client_secret);
          lonejson__m2m_store_cleanup(&store);
          lonejson__runtime_borrow_release(&borrow);
          return status;
        }
      }
    }
    lonejson__owned_free(client_id);
    lonejson__owned_free(client_secret);
  }
  if ((modes & LONEJSON_M2M_AUTH_BEARER) != 0u) {
    char *api_key = NULL;
    if (lonejson__m2m_extract_bearer(request->authorization_header, &api_key,
                                     NULL) == LONEJSON_STATUS_OK) {
      for (i = 0u; i < store.credentials.count; ++i) {
        lonejson__m2m_record *record =
            &((lonejson__m2m_record *)store.credentials.items)[i];
        int ok = 0;
        if (record->revoked || record->client_id == NULL ||
            record->api_key_salt == NULL || record->api_key_hash == NULL) {
          continue;
        }
        (void)lonejson__m2m_verify_hash(runtime_state, record->api_key_salt,
                                        record->api_key_hash, api_key, &ok,
                                        NULL);
        if (ok) {
          out->client_id = lonejson__owned_strdup(NULL, record->client_id);
          if (out->client_id == NULL) {
            out->failure = LONEJSON_AUTH_FAILURE_CLAIMS_INVALID;
            lonejson__owned_free(api_key);
            lonejson__m2m_store_cleanup(&store);
            lonejson__runtime_borrow_release(&borrow);
            return lonejson__set_error(
                error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                "failed to copy authenticated client id");
          }
          out->auth_mode = LONEJSON_M2M_AUTH_BEARER;
          status = lonejson_json_value_set_buffer(
              &out->claim, record->claim.json, record->claim.len, error);
          out->failure = status == LONEJSON_STATUS_OK
                             ? LONEJSON_AUTH_FAILURE_NONE
                             : LONEJSON_AUTH_FAILURE_CLAIMS_INVALID;
          lonejson__owned_free(api_key);
          lonejson__m2m_store_cleanup(&store);
          lonejson__runtime_borrow_release(&borrow);
          return status;
        }
      }
      lonejson__owned_free(api_key);
    }
  }
  lonejson__m2m_store_cleanup(&store);
  lonejson__runtime_borrow_release(&borrow);
  out->failure = request->authorization_header == NULL
                     ? LONEJSON_AUTH_FAILURE_MISSING_CREDENTIALS
                     : LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE;
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "M2M credentials are not accepted");
}

void lonejson_m2m_signup_init(lonejson_m2m_signup *signup) {
  if (signup != NULL) {
    memset(signup, 0, sizeof(*signup));
    lonejson_owned_buffer_init(&signup->query);
    lonejson_owned_buffer_init(&signup->url);
    lonejson_owned_buffer_init(&signup->record_json);
  }
}

void lonejson_m2m_signup_cleanup(lonejson_m2m_signup *signup) {
  if (signup != NULL) {
    lonejson__owned_free(signup->signup_id);
    lonejson__owned_free(signup->signup_secret);
    lonejson_owned_buffer_free(&signup->query);
    lonejson_owned_buffer_free(&signup->url);
    lonejson_owned_buffer_free(&signup->record_json);
    memset(signup, 0, sizeof(*signup));
  }
}

static lonejson_status lonejson__m2m_build_signup_record_json(
    const char *signup_id, const char *secret_salt, const char *secret_hash,
    const char *claim_json, size_t claim_len, size_t max_bytes,
    lonejson_owned_buffer *out, lonejson_error *error) {
  int has_member = 0;
  lonejson_status status;

  lonejson_owned_buffer_free(out);
  status = lonejson__oauth2_form_append_raw(out, "{", 1u, max_bytes, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__m2m_append_member_string(out, "signup_id", signup_id,
                                                &has_member, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__m2m_append_member_string(out, "secret_salt", secret_salt,
                                                &has_member, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__m2m_append_member_string(out, "secret_hash", secret_hash,
                                                &has_member, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_raw(out, ",\"claim\":", 9u, max_bytes,
                                              error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_raw(out, claim_json, claim_len,
                                              max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_raw(out, "}", 1u, max_bytes, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(out);
  }
  return status;
}

lonejson_status
lonejson_m2m_signup_generate(lonejson *runtime,
                             const lonejson_m2m_signup_request *request,
                             lonejson_m2m_signup *out, lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  const char *claim_json;
  const char *secret_param;
  const char *id_param;
  char *secret_salt = NULL;
  char *secret_hash = NULL;
  size_t claim_len;
  size_t max_url_bytes;
  size_t max_record_bytes;
  int has_pair = 0;
  lonejson_status status;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL || request->claim_json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "M2M signup request is required");
  }
  lonejson_m2m_signup_cleanup(out);
  claim_json = request->claim_json;
  claim_len = request->claim_len == 0u ? strlen(request->claim_json)
                                       : request->claim_len;
  status = lonejson_validate_buffer(runtime, claim_json, claim_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      lonejson__m2m_require_crypto(runtime, &borrow, &runtime_state, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      lonejson__m2m_random_token(runtime_state, 16u, &out->signup_id, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__m2m_random_token(runtime_state, 32u, &out->signup_secret,
                                        error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status =
        lonejson__m2m_random_token(runtime_state, 16u, &secret_salt, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__m2m_hash_secret(runtime_state, secret_salt,
                                       out->signup_secret, &secret_hash, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    max_record_bytes = request->max_record_bytes == 0u
                           ? 64u * 1024u
                           : request->max_record_bytes;
    status = lonejson__m2m_build_signup_record_json(
        out->signup_id, secret_salt, secret_hash, claim_json, claim_len,
        max_record_bytes, &out->record_json, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    secret_param =
        request->secret_param != NULL ? request->secret_param : "signup_secret";
    id_param = request->id_param != NULL ? request->id_param : "signup_id";
    max_url_bytes =
        request->max_url_bytes == 0u ? 4096u : request->max_url_bytes;
    status = lonejson__oauth2_form_append_pair(
        &out->query, id_param, out->signup_id, &has_pair, max_url_bytes, error);
    if (status == LONEJSON_STATUS_OK) {
      status = lonejson__oauth2_form_append_pair(&out->query, secret_param,
                                                 out->signup_secret, &has_pair,
                                                 max_url_bytes, error);
    }
    if (status == LONEJSON_STATUS_OK && request->base_url != NULL) {
      const char *join = strchr(request->base_url, '?') == NULL ? "?" : "&";
      status = lonejson__oauth2_form_append_raw(&out->url, request->base_url,
                                                strlen(request->base_url),
                                                max_url_bytes, error);
      if (status == LONEJSON_STATUS_OK) {
        status = lonejson__oauth2_form_append_raw(&out->url, join, 1u,
                                                  max_url_bytes, error);
      }
      if (status == LONEJSON_STATUS_OK) {
        status = lonejson__oauth2_form_append_raw(
            &out->url, out->query.data, out->query.len, max_url_bytes, error);
      }
    }
  }
  lonejson__runtime_borrow_release(&borrow);
  lonejson__owned_free(secret_salt);
  lonejson__owned_free(secret_hash);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_m2m_signup_cleanup(out);
  }
  return status;
}

void lonejson_m2m_signup_complete_init(
    lonejson_m2m_signup_completion *complete) {
  if (complete != NULL) {
    memset(complete, 0, sizeof(*complete));
    lonejson_m2m_credential_init(&complete->credential);
  }
}

void lonejson_m2m_signup_complete_cleanup(
    lonejson_m2m_signup_completion *complete) {
  if (complete != NULL) {
    lonejson__owned_free(complete->signup_id);
    lonejson__owned_free(complete->email);
    lonejson_m2m_credential_cleanup(&complete->credential);
    memset(complete, 0, sizeof(*complete));
  }
}

lonejson_status lonejson_m2m_signup_complete(
    lonejson *runtime, const lonejson_m2m_signup_complete_request *request,
    lonejson_m2m_signup_completion *out, lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson__m2m_store_doc store;
  size_t i;
  lonejson_status status;
  char *claim_copy = NULL;
  size_t claim_len = 0u;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL || request->signup_secret == NULL ||
      request->email == NULL || request->email[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "M2M signup store, secret, email, and output "
                               "are required");
  }
  lonejson_m2m_signup_complete_cleanup(out);
  status =
      lonejson__m2m_require_crypto(runtime, &borrow, &runtime_state, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__m2m_parse_store(runtime, request->store, &store, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__runtime_borrow_release(&borrow);
    return status;
  }
  for (i = 0u; i < store.signups.count; ++i) {
    lonejson__m2m_signup_record *signup =
        &((lonejson__m2m_signup_record *)store.signups.items)[i];
    int ok = 0;
    if (signup->revoked || signup->signup_id == NULL ||
        signup->secret_salt == NULL || signup->secret_hash == NULL) {
      continue;
    }
    if (request->signup_id != NULL &&
        strcmp(request->signup_id, signup->signup_id) != 0) {
      continue;
    }
    (void)lonejson__m2m_verify_hash(runtime_state, signup->secret_salt,
                                    signup->secret_hash, request->signup_secret,
                                    &ok, NULL);
    if (ok) {
      lonejson_m2m_credential_request credential_request;
      claim_copy = (char *)lonejson__owned_malloc(NULL, signup->claim.len + 1u);
      if (claim_copy == NULL) {
        status =
            lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                0u, 0u, "failed to copy signup claim JSON");
        lonejson__m2m_store_cleanup(&store);
        lonejson__runtime_borrow_release(&borrow);
        return status;
      }
      memcpy(claim_copy, signup->claim.json, signup->claim.len);
      claim_copy[signup->claim.len] = '\0';
      claim_len = signup->claim.len;
      out->signup_id = lonejson__owned_strdup(NULL, signup->signup_id);
      out->email = lonejson__owned_strdup(NULL, request->email);
      lonejson__m2m_store_cleanup(&store);
      lonejson__runtime_borrow_release(&borrow);
      if (out->signup_id == NULL || out->email == NULL) {
        lonejson__owned_free(claim_copy);
        lonejson_m2m_signup_complete_cleanup(out);
        return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                   0u, 0u,
                                   "failed to copy signup completion data");
      }
      memset(&credential_request, 0, sizeof(credential_request));
      credential_request.claim_json = claim_copy;
      credential_request.claim_len = claim_len;
      credential_request.auth_modes = request->credential_auth_modes;
      status = lonejson_m2m_credential_generate(runtime, &credential_request,
                                                &out->credential, error);
      lonejson__owned_free(claim_copy);
      if (status != LONEJSON_STATUS_OK) {
        lonejson_m2m_signup_complete_cleanup(out);
      }
      return status;
    }
  }
  lonejson__m2m_store_cleanup(&store);
  lonejson__runtime_borrow_release(&borrow);
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "M2M signup secret is not accepted");
}
#endif
#endif
