#ifdef LONEJSON_WITH_JWT
#ifdef LONEJSON_WITH_OPENSSL
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
#endif

static void test_base64_url_raw_decode_vectors(void) {
  lonejson_error error;
  unsigned char out[8];
  size_t needed;
  size_t decoded_len;

  lonejson_error_init(&error);
  EXPECT(lonejson_base64_decoded_len("", 0u, LONEJSON_BASE64_URL_RAW,
                                     &decoded_len,
                                     &error) == LONEJSON_STATUS_OK);
  EXPECT(decoded_len == 0u);

  memset(out, 0xaa, sizeof(out));
  EXPECT(lonejson_base64_decode("SGVsbG8", 7u, LONEJSON_BASE64_URL_RAW, out,
                                sizeof(out), &needed,
                                &error) == LONEJSON_STATUS_OK);
  EXPECT(needed == 5u);
  EXPECT(memcmp(out, "Hello", 5u) == 0);

  memset(out, 0xaa, sizeof(out));
  EXPECT(lonejson_base64_decode("_w", 2u, LONEJSON_BASE64_URL_RAW, out,
                                sizeof(out), &needed,
                                &error) == LONEJSON_STATUS_OK);
  EXPECT(needed == 1u);
  EXPECT(out[0] == 0xffu);

  memset(out, 0xaa, sizeof(out));
  EXPECT(lj_base64_decode("SGVsbG8", 7u, LJ_BASE64_URL_RAW, out, 4u, &needed,
                          &error) == LJ_STATUS_TRUNCATED);
  EXPECT(needed == 5u);
  EXPECT(out[0] == 0xaau);

  EXPECT(lonejson_base64_decode("A", 1u, LONEJSON_BASE64_URL_RAW, out,
                                sizeof(out), &needed,
                                &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_base64_decode("AA=", 3u, LONEJSON_BASE64_URL_RAW, out,
                                sizeof(out), &needed,
                                &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_base64_decode("AA+/", 4u, LONEJSON_BASE64_URL_RAW, out,
                                sizeof(out), &needed,
                                &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_base64_decode("AA", 2u, LONEJSON_BASE64_URL_RAW, NULL, 1u,
                                &needed,
                                &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_base64_decode("AA", 2u, LONEJSON_BASE64_URL_RAW, out,
                                sizeof(out), NULL,
                                &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
}

static void test_jwt_compact_parse_segments(void) {
  static const char token[] = "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIn0."
                              "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIn0."
                              "c2ln";
  lonejson_jwt_compact jwt;
  lonejson_error error;

  lonejson_error_init(&error);
  EXPECT(lonejson_jwt_parse_compact(token, strlen(token), &jwt, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(jwt.header.data == token);
  EXPECT(jwt.header.len == 35u);
  EXPECT(jwt.payload.data == token + 36u);
  EXPECT(jwt.payload.len == 35u);
  EXPECT(jwt.signature.data == token + 72u);
  EXPECT(jwt.signature.len == 4u);
  EXPECT(jwt.signing_input.data == token);
  EXPECT(jwt.signing_input.len == 71u);

  memset(&jwt, 0, sizeof(jwt));
  EXPECT(lj_jwt_parse_compact("AA.AA.", 6u, &jwt, &error) == LJ_STATUS_OK);
  EXPECT(jwt.header.len == 2u);
  EXPECT(jwt.payload.len == 2u);
  EXPECT(jwt.signature.len == 0u);
  EXPECT(jwt.signing_input.len == 5u);
}

static void test_jwt_compact_parse_failures(void) {
  lonejson_jwt_compact jwt;
  lonejson_error error;

  lonejson_error_init(&error);
  EXPECT(lonejson_jwt_parse_compact(NULL, 0u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_jwt_parse_compact("AA.AA.AA", 8u, NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_jwt_parse_compact("", 0u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_parse_compact("AA.AA", 5u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_parse_compact(".AA.AA", 6u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_parse_compact("AA..AA", 6u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_parse_compact("AA.AA.AA.AA", 11u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_parse_compact("A.AA.AA", 7u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(error.offset == 1u);
  EXPECT(lonejson_jwt_parse_compact("AA.A=.AA", 8u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(error.offset == 4u);
  EXPECT(lonejson_jwt_parse_compact("AA.AA.A+", 8u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(error.offset == 7u);
}

static void test_jwk_parse_json_shapes(void) {
  lonejson_jwk jwk;
  lonejson_error error;
  const char *rsa =
      "{\"kty\":\"RSA\",\"kid\":\"rsa1\",\"use\":\"sig\",\"alg\":\"RS256\","
      "\"key_ops\":[\"verify\"],\"x5t\":\"AA\",\"x5t#S256\":\"AA\","
      "\"x5c\":[\"AA==\"],\"n\":\"AQIDBA\",\"e\":\"AQAB\"}";
  const char *ec =
      "{\"kty\":\"EC\",\"kid\":\"ec1\",\"crv\":\"P-256\",\"x\":\"AAEC\","
      "\"y\":\"AwQF\"}";
  const char *oct = "{\"kty\":\"oct\",\"kid\":\"sym1\",\"k\":\"c2VjcmV0\"}";

  lonejson_error_init(&error);
  lonejson_jwk_init(&jwk);
  EXPECT(lonejson_jwk_parse_json(test_default_runtime(), rsa, strlen(rsa), &jwk,
                                 &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(jwk.kty, "RSA") == 0);
  EXPECT(strcmp(jwk.kid, "rsa1") == 0);
  EXPECT(strcmp(jwk.alg, "RS256") == 0);
  EXPECT(jwk.key_ops.count == 1u);
  EXPECT(strcmp(jwk.key_ops.items[0], "verify") == 0);
  EXPECT(strcmp(jwk.x5t, "AA") == 0);
  EXPECT(strcmp(jwk.x5t_s256, "AA") == 0);
  EXPECT(jwk.x5c.count == 1u);
  EXPECT(strcmp(jwk.x5c.items[0], "AA==") == 0);
  EXPECT(strcmp(jwk.n, "AQIDBA") == 0);
  EXPECT(strcmp(jwk.e, "AQAB") == 0);
  lonejson_jwk_cleanup(&jwk);
  EXPECT(jwk.kty == NULL);

  EXPECT(lj_jwk_parse_json(test_default_runtime(), ec, strlen(ec), &jwk,
                           &error) == LJ_STATUS_OK);
  EXPECT(strcmp(jwk.kty, "EC") == 0);
  EXPECT(strcmp(jwk.crv, "P-256") == 0);
  EXPECT(strcmp(jwk.x, "AAEC") == 0);
  EXPECT(strcmp(jwk.y, "AwQF") == 0);

  EXPECT(lonejson_jwk_parse_json(test_default_runtime(), oct, strlen(oct), &jwk,
                                 &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(jwk.kty, "oct") == 0);
  EXPECT(jwk.crv == NULL);
  EXPECT(jwk.x == NULL);
  EXPECT(jwk.y == NULL);
  EXPECT(strcmp(jwk.k, "c2VjcmV0") == 0);
  lonejson_jwk_cleanup(&jwk);
}

static void test_jwks_parse_and_select(void) {
  static const char jwks_json[] =
      "{\"keys\":["
      "{\"kty\":\"RSA\",\"kid\":\"first\",\"use\":\"sig\",\"alg\":\"RS256\","
      "\"n\":\"AQIDBA\",\"e\":\"AQAB\"},"
      "{\"kty\":\"RSA\",\"kid\":\"second\",\"use\":\"enc\",\"alg\":\"RSA-"
      "OAEP\","
      "\"n\":\"BQYH\",\"e\":\"AQAB\"},"
      "{\"kty\":\"EC\",\"kid\":\"ec\",\"use\":\"sig\",\"alg\":\"ES256\","
      "\"crv\":\"P-256\",\"x\":\"AAEC\",\"y\":\"AwQF\"}"
      "]}";
  lonejson_jwks jwks;
  lonejson_jwk_select_options options;
  const lonejson_jwk *selected;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_jwks_init(&jwks);
  EXPECT(lonejson_jwks_parse_json(test_default_runtime(), jwks_json,
                                  strlen(jwks_json), &jwks,
                                  &error) == LONEJSON_STATUS_OK);
  EXPECT(jwks.keys.count == 3u);

  memset(&options, 0, sizeof(options));
  options.kid = "second";
  options.kty = "RSA";
  EXPECT(lonejson_jwks_select(&jwks, &options, &selected, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(selected != NULL);
  EXPECT(strcmp(selected->kid, "second") == 0);
  EXPECT(strcmp(selected->use, "enc") == 0);

  memset(&options, 0, sizeof(options));
  options.use = "sig";
  options.alg = "ES256";
  EXPECT(lj_jwks_select(&jwks, &options, &selected, &error) == LJ_STATUS_OK);
  EXPECT(selected != NULL);
  EXPECT(strcmp(selected->kid, "ec") == 0);

  memset(&options, 0, sizeof(options));
  options.kid = "missing";
  selected = (const lonejson_jwk *)1;
  EXPECT(lonejson_jwks_select(&jwks, &options, &selected, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(selected == NULL);

  EXPECT(
      lonejson_jwks_parse_json(
          test_default_runtime(),
          "{\"keys\":[{\"kty\":\"oct\",\"kid\":\"sym\",\"k\":\"c2VjcmV0\"}]}",
          strlen("{\"keys\":[{\"kty\":\"oct\",\"kid\":\"sym\","
                 "\"k\":\"c2VjcmV0\"}]}"),
          &jwks, &error) == LONEJSON_STATUS_OK);
  EXPECT(jwks.keys.count == 1u);
  selected = &((const lonejson_jwk *)jwks.keys.items)[0];
  EXPECT(strcmp(selected->kid, "sym") == 0);
  EXPECT(strcmp(selected->kty, "oct") == 0);

  lonejson_jwks_cleanup(&jwks);
  EXPECT(jwks.keys.items == NULL);
  EXPECT(jwks.keys.count == 0u);
}

static void test_jwk_parse_failures(void) {
  lonejson_jwk jwk;
  lonejson_jwks jwks;
  const lonejson_jwk *selected;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_jwk_init(&jwk);
  {
    const char *json = "{\"kid\":\"x\"}";
    EXPECT(lonejson_jwk_parse_json(test_default_runtime(), json, strlen(json),
                                   &jwk, &error) ==
           LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  }
  {
    const char *json = "{\"kty\":\"RSA\",\"n\":\"AQID\"}";
    EXPECT(lonejson_jwk_parse_json(test_default_runtime(), json, strlen(json),
                                   &jwk,
                                   &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  {
    const char *json = "{\"kty\":\"RSA\",\"n\":\"AQ=\",\"e\":\"AQAB\"}";
    EXPECT(lonejson_jwk_parse_json(test_default_runtime(), json, strlen(json),
                                   &jwk,
                                   &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  {
    const char *json =
        "{\"kty\":\"RSA\",\"n\":\"AQID\",\"e\":\"AQAB\",\"x5t\":\"@@\"}";
    EXPECT(lonejson_jwk_parse_json(test_default_runtime(), json, strlen(json),
                                   &jwk,
                                   &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  {
    const char *json =
        "{\"kty\":\"RSA\",\"n\":\"AQID\",\"e\":\"AQAB\",\"x5c\":[\"@@\"]}";
    EXPECT(lonejson_jwk_parse_json(test_default_runtime(), json, strlen(json),
                                   &jwk,
                                   &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  EXPECT(jwk.kty == NULL);

  lonejson_jwks_init(&jwks);
  {
    const char *json = "{\"keys\":[]}";
    EXPECT(lonejson_jwks_parse_json(test_default_runtime(), json, strlen(json),
                                    &jwks,
                                    &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  {
    const char *json =
        "{\"keys\":[{\"kty\":\"EC\",\"x\":\"AA\",\"y\":\"AA\"}]}";
    EXPECT(lonejson_jwks_parse_json(test_default_runtime(), json, strlen(json),
                                    &jwks,
                                    &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  EXPECT(lonejson_jwks_select(NULL, NULL, &selected, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_jwks_select(&jwks, NULL, NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_jwks_cleanup(&jwks);
}

static lonejson_jwt_claim_policy test_jwt_policy(void) {
  static const char *const algs[] = {"RS256"};
  static const char *const issuers[] = {"issuer"};
  static const char *const audiences[] = {"api"};
  static const char *const required[] = {"iss", "aud", "exp"};
  lonejson_jwt_claim_policy policy;

  memset(&policy, 0, sizeof(policy));
  policy.accepted_algs = algs;
  policy.accepted_alg_count = sizeof(algs) / sizeof(algs[0]);
  policy.accepted_issuers = issuers;
  policy.accepted_issuer_count = sizeof(issuers) / sizeof(issuers[0]);
  policy.accepted_audiences = audiences;
  policy.accepted_audience_count = sizeof(audiences) / sizeof(audiences[0]);
  policy.required_claims = required;
  policy.required_claim_count = sizeof(required) / sizeof(required[0]);
  policy.now = 1000;
  policy.allowed_clock_skew = 30;
  return policy;
}

static const char test_jwt_rs256_token[] =
    "eyJhbGciOiJSUzI1NiIsImtpZCI6InJzYS10ZXN0IiwidHlwIjoiSldUIn0."
    "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJuYm"
    "YiOjkwMCwiaWF0IjoxMDAwfQ."
    "PoouvDAoloqvsfTQxadOTpQGXKyeHq0lx6WQEPv0qvg59KMuy8lD-XBTBPCF_MpQGoe3DS"
    "84CSg27iktG7z12Qv6TX1gqbJUO2wkwhuW4dIWFPY9tDhI3e05W5yz8D70wARx7CL9tHKW"
    "sOpLwHRWf5ugfrq1PuofcC9atB7D-QUfrmmJ01NXbQl4aq6DJ02M7azHTLq-15X3TuE2CH"
    "N5P_zo_zkaJT8V0QhoQ3MUhUE_pBxMtAByRIUOEW32RbWjYgkwZ_zxaVkbhXv1CYznQCzi"
    "kX2wXn9OQ_1z0TCH7bT5Ao3EXEQeiK7Fhuq8lyPFbkhDc_yCjxFRjm7ufSFZbg";

static const char test_jwt_rs256_jwk_json[] =
    "{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\",\"alg\":\"RS256\","
    "\"n\":\"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-"
    "oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8"
    "g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4Gcsp"
    "GtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1O"
    "gnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWy"
    "YoQ\","
    "\"e\":\"AQAB\"}";

static const char test_jwt_rs256_jwks_json[] =
    "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\","
    "\"alg\":\"RS256\","
    "\"n\":\"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-"
    "oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8"
    "g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4Gcsp"
    "GtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1O"
    "gnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWy"
    "YoQ\","
    "\"e\":\"AQAB\"}]}";

static const char test_jwt_rs256_other_kid_jwks_json[] =
    "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"other-key\",\"use\":\"sig\","
    "\"alg\":\"RS256\","
    "\"n\":\"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-"
    "oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8"
    "g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4Gcsp"
    "GtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1O"
    "gnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWy"
    "YoQ\","
    "\"e\":\"AQAB\"}]}";

static const char test_jwt_rs256_same_kid_wrong_alg_first_jwks_json[] =
    "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\","
    "\"alg\":\"PS256\",\"n\":\"AQIDBA\",\"e\":\"AQAB\"},"
    "{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\","
    "\"alg\":\"RS256\","
    "\"n\":\"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-"
    "oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8"
    "g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4Gcsp"
    "GtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1O"
    "gnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWy"
    "YoQ\","
    "\"e\":\"AQAB\"}]}";

static const char test_jwt_rs256_minimal_jwks_json[] =
    "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"rsa-test\","
    "\"n\":\"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-"
    "oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8"
    "g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4Gcsp"
    "GtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1O"
    "gnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWy"
    "YoQ\","
    "\"e\":\"AQAB\"}]}";

#ifdef LONEJSON_WITH_OPENSSL
static unsigned char *
test_jwt_base64_decode_alloc(const char *value, lonejson_base64_variant variant,
                             size_t *out_len) {
  lonejson_error error;
  unsigned char *out;
  size_t len;
  size_t needed;

  lonejson_error_init(&error);
  EXPECT(lonejson_base64_decoded_len(value, strlen(value), variant, &len,
                                     &error) == LONEJSON_STATUS_OK);
  out = (unsigned char *)malloc(len == 0u ? 1u : len);
  EXPECT(out != NULL);
  EXPECT(lonejson_base64_decode(value, strlen(value), variant, out, len,
                                &needed, &error) == LONEJSON_STATUS_OK);
  EXPECT(needed == len);
  *out_len = len;
  return out;
}

static EVP_PKEY *test_jwt_rsa_pkey_from_jwk(const lonejson_jwk *jwk) {
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

  n = test_jwt_base64_decode_alloc(jwk->n, LONEJSON_BASE64_URL_RAW, &n_len);
  e = test_jwt_base64_decode_alloc(jwk->e, LONEJSON_BASE64_URL_RAW, &e_len);
  n_bn = BN_bin2bn(n, (int)n_len, NULL);
  e_bn = BN_bin2bn(e, (int)e_len, NULL);
  builder = OSSL_PARAM_BLD_new();
  ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
  EXPECT(n_bn != NULL);
  EXPECT(e_bn != NULL);
  EXPECT(builder != NULL);
  EXPECT(ctx != NULL);
  EXPECT(OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_N, n_bn) == 1);
  EXPECT(OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_E, e_bn) == 1);
  params = OSSL_PARAM_BLD_to_param(builder);
  EXPECT(params != NULL);
  EXPECT(EVP_PKEY_fromdata_init(ctx) == 1);
  EXPECT(EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) == 1);
  EXPECT(pkey != NULL);
  free(n);
  free(e);
  BN_free(n_bn);
  BN_free(e_bn);
  OSSL_PARAM_free(params);
  OSSL_PARAM_BLD_free(builder);
  EVP_PKEY_CTX_free(ctx);
  return pkey;
}

static X509_NAME *test_jwt_x509_name(const char *cn) {
  X509_NAME *name = X509_NAME_new();
  EXPECT(name != NULL);
  EXPECT(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                    (const unsigned char *)cn, -1, -1, 0) == 1);
  return name;
}

static void test_jwt_x509_add_ext(X509 *cert, int nid, const char *value) {
  X509_EXTENSION *ext = X509V3_EXT_conf_nid(NULL, NULL, nid, (char *)value);
  EXPECT(ext != NULL);
  EXPECT(X509_add_ext(cert, ext, -1) == 1);
  X509_EXTENSION_free(ext);
}

static X509 *test_jwt_make_cert(EVP_PKEY *subject_key, EVP_PKEY *issuer_key,
                                X509_NAME *subject, X509_NAME *issuer,
                                long serial, int ca) {
  X509 *cert = X509_new();
  EXPECT(cert != NULL);
  EXPECT(X509_set_version(cert, 2) == 1);
  EXPECT(ASN1_INTEGER_set(X509_get_serialNumber(cert), serial) == 1);
  EXPECT(X509_gmtime_adj(X509_get_notBefore(cert), -60) != NULL);
  EXPECT(X509_gmtime_adj(X509_get_notAfter(cert), 3600) != NULL);
  EXPECT(X509_set_subject_name(cert, subject) == 1);
  EXPECT(X509_set_issuer_name(cert, issuer) == 1);
  EXPECT(X509_set_pubkey(cert, subject_key) == 1);
  test_jwt_x509_add_ext(cert, NID_basic_constraints,
                        ca ? "critical,CA:TRUE" : "critical,CA:FALSE");
  test_jwt_x509_add_ext(cert, NID_key_usage,
                        ca ? "critical,keyCertSign,cRLSign"
                           : "critical,digitalSignature");
  EXPECT(X509_sign(cert, issuer_key, EVP_sha256()) > 0);
  return cert;
}

static char *test_jwt_x509_base64_der(X509 *cert) {
  unsigned char *der = NULL;
  unsigned char *cursor;
  char *out;
  int der_len;
  size_t encoded_len;
  size_t needed;
  lonejson_error error;

  lonejson_error_init(&error);
  der_len = i2d_X509(cert, NULL);
  EXPECT(der_len > 0);
  der = (unsigned char *)malloc((size_t)der_len);
  EXPECT(der != NULL);
  cursor = der;
  EXPECT(i2d_X509(cert, &cursor) == der_len);
  EXPECT(lonejson_base64_encoded_len((size_t)der_len, LONEJSON_BASE64_STANDARD,
                                     &encoded_len,
                                     &error) == LONEJSON_STATUS_OK);
  out = (char *)malloc(encoded_len + 1u);
  EXPECT(out != NULL);
  EXPECT(lonejson_base64_encode(der, (size_t)der_len, LONEJSON_BASE64_STANDARD,
                                out, encoded_len + 1u, &needed,
                                &error) == LONEJSON_STATUS_OK);
  EXPECT(needed == encoded_len);
  out[encoded_len] = '\0';
  free(der);
  return out;
}

static char *test_jwt_x509_thumbprint(X509 *cert, const EVP_MD *md,
                                      lonejson_base64_variant variant) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0u;
  char *out;
  size_t encoded_len;
  size_t needed;
  lonejson_error error;

  lonejson_error_init(&error);
  EXPECT(X509_digest(cert, md, digest, &digest_len) == 1);
  EXPECT(lonejson_base64_encoded_len((size_t)digest_len, variant, &encoded_len,
                                     &error) == LONEJSON_STATUS_OK);
  out = (char *)malloc(encoded_len + 1u);
  EXPECT(out != NULL);
  EXPECT(lonejson_base64_encode(digest, (size_t)digest_len, variant, out,
                                encoded_len + 1u, &needed,
                                &error) == LONEJSON_STATUS_OK);
  EXPECT(needed == encoded_len);
  out[encoded_len] = '\0';
  return out;
}
#endif

static const char test_jwt_ps256_token[] =
    "eyJhbGciOiJQUzI1NiIsImtpZCI6InBzLXRlc3QiLCJ0eXAiOiJKV1QifQ."
    "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzdWJqZWN0IiwiYXVkIjoiYXBpIiwiZXhwIjoyM"
    "DAwLCJuYmYiOjkwMCwiaWF0IjoxMDAwfQ."
    "OdIVXXtzJsvBRJzTctmftYO4sDiVYLMy9r2O7yzmwwUPN0e-q4JWRiUc5aAITUDfWLbcn"
    "gBHcVrSCYL2I2o4AASQi2P1A44nKHXsV3Zrn442tSxmyShDiyrpr-mDTJHLI1G949SIA"
    "bF8Kt9zhQ3zcq9BC2fi-nl87Y7dngDWpFVqjF30QQMBV-l0MMlhEQrQT5u9LzlFj9xGE"
    "2etdjs6KpHrI15gOnlGoC7H7hBX4QP3PK0y7B6RMh9HqSkOoINNnAA2srmhMhaoC3ZAh"
    "awXx4fYS0hweeldTE39ZZiNmPk6wlHWLX8anu4AjFe22ND4mlbrBQX_Yt1uekCUUrb0I"
    "A";

static const char test_jwt_ps256_jwk_json[] =
    "{\"kty\":\"RSA\",\"kid\":\"ps-test\",\"use\":\"sig\",\"alg\":\"PS256\","
    "\"n\":\"rLE6MybeCWHjxJ-DG8PFFPGYeVwop5iEbtM75iTuQGglZIDmPRGbV62w6wnp"
    "ILXeaCIBG2Z_JgPh6Zr-2Ic81g8wjAK33Ihd-afm2Ey1qzZltClHyF9pj1TnWn47KnTop"
    "hziLpHuA3AtAyOxv9uyffHLsK6Y5fFtqLWgAynP1q4yDfjevY77TquLw05fmOnjyTbf9"
    "T8cdYAH_LejYltyVS0D6oPdxGFJ8XMNyUwkNHE_w-d4piFzMG_rediEb_l6YT8fn8qyr"
    "LwuxxgrDW88Rhr10W1w52jlHbwljJiN8dgNMR7e-mNe5-iWxAQQQCW3Pf8FQyiC5J3k"
    "o5kK5OjSaQ\","
    "\"e\":\"AQAB\"}";

static const char test_jwt_es256_token[] =
    "eyJhbGciOiJFUzI1NiIsImtpZCI6ImVjLXRlc3QiLCJ0eXAiOiJKV1QifQ."
    "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzdWJqZWN0IiwiYXVkIjoiYXBpIiwiZXhwIjoyM"
    "DAwLCJuYmYiOjkwMCwiaWF0IjoxMDAwfQ."
    "95YnhtNbEKRS6fmhs543TmLdnljwB3r-wID-L4mGWcOYY4mEmIhmRlbTX1HM_GDKeLGK"
    "yAEw2vY78a4c51eCWQ";

static const char test_jwt_es256_jwk_json[] =
    "{\"kty\":\"EC\",\"kid\":\"ec-test\",\"use\":\"sig\",\"alg\":\"ES256\","
    "\"crv\":\"P-256\",\"x\":\"sKrqPeMnxHsFzQE1uhjgF1PK0x6oM9YBnJKeunXnVx8\","
    "\"y\":\"KTGlbL_TW5NzmtqJRMuTWywE45o4xtm80MeDA84pv4M\"}";

static const char test_jwt_eddsa_token[] =
    "eyJhbGciOiJFZERTQSIsImtpZCI6ImVkLXRlc3QiLCJ0eXAiOiJKV1QifQ."
    "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzdWJqZWN0IiwiYXVkIjoiYXBpIiwiZXhwIjoyM"
    "DAwLCJuYmYiOjkwMCwiaWF0IjoxMDAwfQ."
    "vZ8EHlu8rMpJbieq2KShKybLutH-oljg9sqjFy8tzk5a4lGoj_9eIKoeckMS5RwMTGwp"
    "GeBgM5gQMl-yFVceCg";

static const char test_jwt_eddsa_jwk_json[] =
    "{\"kty\":\"OKP\",\"kid\":\"ed-test\",\"use\":\"sig\",\"alg\":\"EdDSA\","
    "\"crv\":\"Ed25519\",\"x\":"
    "\"clqC7T1N8EI4ekdG3YiNhqGO6Ef30cKhOB4IOKDXjCM\"}";

static void test_jwt_decode_and_validate_claims(void) {
  static const char token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJuYmYi"
      "OjkwMCwiaWF0IjoxMDAwfQ."
      "c2ln";
  static const char aud_array_token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOlsiYXBpIiwib3RoZXIiXSwiZXhwIjoyMDAwfQ."
      "c2ln";
  static const char nonce_token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJu"
      "b25jZSI6Im5vbmNlLTQ1NiJ9."
      "c2ln";
  static const char jose_policy_token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIiwiY3JpdCI6WyJleHAt"
      "dGVzdCJdLCJ4NXQiOiJBQSIsIng1dCNTMjU2IjoiQUEiLCJ4NWMiOlsiQUE9PSJdfQ."
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjpbImFwaSIsImNsaWVudCJdLCJh"
      "enAiOiJjbGllbnQiLCJzY29wZSI6InJlYWQgd3JpdGUiLCJzY3AiOlsiYWRtaW4iLCJi"
      "aWxsaW5nLnJlYWQiXSwiZXhwIjoyMDAwfQ."
      "c2ln";
  static const char no_azp_token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOlsiYXBpIiwiY2xpZW50Il0sInNjb3BlIjoicmVh"
      "ZCIsImV4cCI6MjAwMH0."
      "c2ln";
  static const char *const algs[] = {"RS256"};
  static const char *const issuers[] = {"issuer"};
  static const char *const audiences[] = {"api", "client"};
  static const char *const accepted_crit[] = {"exp-test"};
  static const char *const required_scopes[] = {"read", "admin"};
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwt_claim_policy policy;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  policy = test_jwt_policy();

  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), token,
                                     strlen(token), &policy, &header, &claims,
                                     &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(header.alg, "RS256") == 0);
  EXPECT(strcmp(header.kid, "k1") == 0);
  EXPECT(strcmp(header.typ, "JWT") == 0);
  EXPECT(strcmp(claims.iss, "issuer") == 0);
  EXPECT(strcmp(claims.sub, "s") == 0);
  EXPECT(strcmp(claims.aud, "api") == 0);
  EXPECT(claims.has_exp != 0 && claims.exp == 2000);
  EXPECT(claims.has_nbf != 0 && claims.nbf == 900);
  EXPECT(claims.has_iat != 0 && claims.iat == 1000);
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_OK);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);

  EXPECT(lj_jwt_decode_compact(test_default_runtime(), aud_array_token,
                               strlen(aud_array_token), &policy, &header,
                               &claims, &error) == LJ_STATUS_OK);
  EXPECT(claims.aud == NULL);
  EXPECT(claims.aud_array.count == 2u);
  EXPECT(strcmp(claims.aud_array.items[0], "api") == 0);
  EXPECT(strcmp(claims.aud_array.items[1], "other") == 0);
  EXPECT(lj_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LJ_STATUS_OK);
  lj_jwt_header_cleanup(&header);
  lj_jwt_claims_cleanup(&claims);

  policy = test_jwt_policy();
  policy.expected_nonce = "nonce-456";
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nonce_token,
                                     strlen(nonce_token), &policy, &header,
                                     &claims, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(claims.nonce, "nonce-456") == 0);
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_OK);
  policy.expected_nonce = "other";
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);

  memset(&policy, 0, sizeof(policy));
  policy.accepted_algs = algs;
  policy.accepted_alg_count = sizeof(algs) / sizeof(algs[0]);
  policy.accepted_issuers = issuers;
  policy.accepted_issuer_count = sizeof(issuers) / sizeof(issuers[0]);
  policy.accepted_audiences = audiences;
  policy.accepted_audience_count = sizeof(audiences) / sizeof(audiences[0]);
  policy.accepted_crit = accepted_crit;
  policy.accepted_crit_count = sizeof(accepted_crit) / sizeof(accepted_crit[0]);
  policy.required_scopes = required_scopes;
  policy.required_scope_count =
      sizeof(required_scopes) / sizeof(required_scopes[0]);
  policy.expected_azp = "client";
  policy.require_azp_when_multiple_audiences = 1;
  policy.require_all_audiences_accepted = 1;
  policy.now = 1000;

  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), jose_policy_token,
                                     strlen(jose_policy_token), &policy,
                                     &header, &claims,
                                     &error) == LONEJSON_STATUS_OK);
  EXPECT(header.crit.count == 1u);
  EXPECT(strcmp(header.crit.items[0], "exp-test") == 0);
  EXPECT(strcmp(header.x5t, "AA") == 0);
  EXPECT(strcmp(header.x5t_s256, "AA") == 0);
  EXPECT(header.x5c.count == 1u);
  EXPECT(strcmp(header.x5c.items[0], "AA==") == 0);
  EXPECT(strcmp(claims.azp, "client") == 0);
  EXPECT(strcmp(claims.scope, "read write") == 0);
  EXPECT(claims.scp.count == 2u);
  EXPECT(strcmp(claims.scp.items[0], "admin") == 0);
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_OK);
  policy.accepted_crit_count = 0u;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  policy.accepted_crit_count = sizeof(accepted_crit) / sizeof(accepted_crit[0]);
  policy.expected_azp = "other";
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  policy.expected_azp = "client";
  policy.required_scope_count = 1u;
  policy.required_scopes = &required_scopes[1];
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_OK);
  policy.required_scopes = required_scopes;
  policy.required_scope_count = 2u;
  policy.accepted_audience_count = 1u;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);

  policy.accepted_audience_count = sizeof(audiences) / sizeof(audiences[0]);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), no_azp_token,
                                     strlen(no_azp_token), &policy, &header,
                                     &claims, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
}

static void test_jwt_validate_rs256_signature(void) {
  lonejson_jwt_compact compact;
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwk jwk;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  lonejson_jwk_init(&jwk);

  EXPECT(lonejson_jwt_parse_compact(test_jwt_rs256_token,
                                    strlen(test_jwt_rs256_token), &compact,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(
      lonejson_jwt_decode_compact(test_default_runtime(), test_jwt_rs256_token,
                                  strlen(test_jwt_rs256_token), NULL, &header,
                                  &claims, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwk_parse_json(test_default_runtime(),
                                 test_jwt_rs256_jwk_json,
                                 strlen(test_jwt_rs256_jwk_json), &jwk,
                                 &error) == LONEJSON_STATUS_OK);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lj_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LJ_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_validate_signature_with_runtime(
             test_default_runtime(), &compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_OK);
#else
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lj_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LJ_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_validate_signature_with_runtime(
             test_default_runtime(), &compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
#endif

  lonejson_jwk_cleanup(&jwk);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
}

static void test_jwt_validate_x5c_signature_policy(void) {
#ifdef LONEJSON_WITH_OPENSSL
  lonejson_jwt_compact compact;
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwk jwk;
  lonejson_error error;
  lonejson_config runtime_config;
  lonejson *runtime;
  lonejson_auth_provider provider;
  lonejson_openssl_auth_provider_config provider_config;
  EVP_PKEY *jwt_key = NULL;
  EVP_PKEY *ca_key = NULL;
  X509_NAME *ca_name = NULL;
  X509_NAME *leaf_name = NULL;
  X509 *ca_cert = NULL;
  X509 *leaf_cert = NULL;
  X509 *bad_leaf_cert = NULL;
  X509_STORE *store = NULL;
  char **x5c_items;
  char *leaf_b64;
  char *ca_b64;
  char *bad_leaf_b64;
  char *thumb_sha1;
  char *thumb_sha256;
  char *saved_x5t_s256;

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  lonejson_jwk_init(&jwk);
  memset(&provider_config, 0, sizeof(provider_config));

  EXPECT(lonejson_jwt_parse_compact(test_jwt_rs256_token,
                                    strlen(test_jwt_rs256_token), &compact,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(
      lonejson_jwt_decode_compact(test_default_runtime(), test_jwt_rs256_token,
                                  strlen(test_jwt_rs256_token), NULL, &header,
                                  &claims, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwk_parse_json(test_default_runtime(),
                                 test_jwt_rs256_jwk_json,
                                 strlen(test_jwt_rs256_jwk_json), &jwk,
                                 &error) == LONEJSON_STATUS_OK);

  jwt_key = test_jwt_rsa_pkey_from_jwk(&jwk);
  ca_key = EVP_RSA_gen(2048);
  EXPECT(ca_key != NULL);
  ca_name = test_jwt_x509_name("lonejson test ca");
  leaf_name = test_jwt_x509_name("lonejson test jwt leaf");
  ca_cert = test_jwt_make_cert(ca_key, ca_key, ca_name, ca_name, 1, 1);
  leaf_cert = test_jwt_make_cert(jwt_key, ca_key, leaf_name, ca_name, 2, 0);
  bad_leaf_cert = test_jwt_make_cert(ca_key, ca_key, leaf_name, ca_name, 3, 0);
  leaf_b64 = test_jwt_x509_base64_der(leaf_cert);
  ca_b64 = test_jwt_x509_base64_der(ca_cert);
  bad_leaf_b64 = test_jwt_x509_base64_der(bad_leaf_cert);
  thumb_sha1 =
      test_jwt_x509_thumbprint(leaf_cert, EVP_sha1(), LONEJSON_BASE64_URL_RAW);
  thumb_sha256 = test_jwt_x509_thumbprint(leaf_cert, EVP_sha256(),
                                          LONEJSON_BASE64_URL_RAW);

  x5c_items = (char **)malloc(2u * sizeof(x5c_items[0]));
  EXPECT(x5c_items != NULL);
  x5c_items[0] = leaf_b64;
  x5c_items[1] = ca_b64;
  jwk.x5c.items = x5c_items;
  jwk.x5c.count = 2u;
  jwk.x5c.capacity = 2u;
  jwk.x5c.flags = LONEJSON_ARRAY_OWNS_ITEMS;
  jwk.x5t = thumb_sha1;
  jwk.x5t_s256 = thumb_sha256;

  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  store = X509_STORE_new();
  EXPECT(store != NULL);
  EXPECT(X509_STORE_add_cert(store, ca_cert) == 1);
  provider_config.x509_store = store;
  EXPECT(lonejson_auth_provider_init_openssl(&provider, &provider_config,
                                             &error) == LONEJSON_STATUS_OK);
  EXPECT(provider.user_data == store);
  memset(&provider_config, 0, sizeof(provider_config));
  runtime_config = lonejson_default_config();
  runtime = lonejson_new(&runtime_config, &error);
  EXPECT(runtime != NULL);
  EXPECT(lonejson_set_auth_provider(runtime, &provider, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwt_validate_signature_with_runtime(
             runtime, &compact, &header, &jwk, &error) == LONEJSON_STATUS_OK);

  saved_x5t_s256 = header.x5t_s256;
  header.x5t_s256 = (char *)malloc(3u);
  EXPECT(header.x5t_s256 != NULL);
  memcpy(header.x5t_s256, "AA", 3u);
  EXPECT(lonejson_jwt_validate_signature_with_runtime(runtime, &compact,
                                                      &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  free(header.x5t_s256);
  header.x5t_s256 = saved_x5t_s256;

  jwk.x5c.items[0] = bad_leaf_b64;
  EXPECT(lonejson_jwt_validate_signature_with_runtime(runtime, &compact,
                                                      &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  free(bad_leaf_b64);
  jwk.x5c.items[0] = leaf_b64;

  lonejson_free(runtime);
  X509_STORE_free(store);
  X509_free(bad_leaf_cert);
  X509_free(leaf_cert);
  X509_free(ca_cert);
  X509_NAME_free(leaf_name);
  X509_NAME_free(ca_name);
  EVP_PKEY_free(ca_key);
  EVP_PKEY_free(jwt_key);
  free(jwk.x5t);
  free(jwk.x5t_s256);
  free(jwk.x5c.items[0]);
  free(jwk.x5c.items[1]);
  free(jwk.x5c.items);
  jwk.x5t = NULL;
  jwk.x5t_s256 = NULL;
  memset(&jwk.x5c, 0, sizeof(jwk.x5c));
  lonejson_jwk_cleanup(&jwk);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
#endif
}

static void test_jwt_validate_recommended_signatures(void) {
#ifdef LONEJSON_WITH_OPENSSL
  static const struct {
    const char *token;
    const char *jwk_json;
  } cases[] = {{test_jwt_ps256_token, test_jwt_ps256_jwk_json},
               {test_jwt_es256_token, test_jwt_es256_jwk_json},
               {test_jwt_eddsa_token, test_jwt_eddsa_jwk_json}};
  size_t i;
  lonejson_error error;

  lonejson_error_init(&error);
  for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    lonejson_jwt_compact compact;
    lonejson_jwt_header header;
    lonejson_jwt_claims claims;
    lonejson_jwk jwk;

    lonejson_jwt_header_init(&header);
    lonejson_jwt_claims_init(&claims);
    lonejson_jwk_init(&jwk);
    EXPECT(lonejson_jwt_parse_compact(cases[i].token, strlen(cases[i].token),
                                      &compact, &error) == LONEJSON_STATUS_OK);
    EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), cases[i].token,
                                       strlen(cases[i].token), NULL, &header,
                                       &claims, &error) == LONEJSON_STATUS_OK);
    EXPECT(lonejson_jwk_parse_json(test_default_runtime(), cases[i].jwk_json,
                                   strlen(cases[i].jwk_json), &jwk,
                                   &error) == LONEJSON_STATUS_OK);
    EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
           LONEJSON_STATUS_TYPE_MISMATCH);
    EXPECT(lonejson_jwt_validate_signature_with_runtime(
               test_default_runtime(), &compact, &header, &jwk, &error) ==
           LONEJSON_STATUS_OK);
    if (strcmp(header.alg, "ES256") == 0) {
      char *jwk_x = jwk.x;
      char *jwk_y = jwk.y;

      jwk.x = NULL;
      EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
             LONEJSON_STATUS_TYPE_MISMATCH);
      EXPECT(lonejson_jwt_validate_signature_with_runtime(
                 test_default_runtime(), &compact, &header, &jwk, &error) ==
             LONEJSON_STATUS_TYPE_MISMATCH);
      jwk.x = jwk_x;
      jwk.y = NULL;
      EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
             LONEJSON_STATUS_TYPE_MISMATCH);
      EXPECT(lonejson_jwt_validate_signature_with_runtime(
                 test_default_runtime(), &compact, &header, &jwk, &error) ==
             LONEJSON_STATUS_TYPE_MISMATCH);
      jwk.y = jwk_y;
    } else if (strcmp(header.alg, "EdDSA") == 0) {
      char *jwk_x = jwk.x;

      jwk.x = NULL;
      EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
             LONEJSON_STATUS_TYPE_MISMATCH);
      EXPECT(lonejson_jwt_validate_signature_with_runtime(
                 test_default_runtime(), &compact, &header, &jwk, &error) ==
             LONEJSON_STATUS_TYPE_MISMATCH);
      jwk.x = jwk_x;
    }
    lonejson_jwk_cleanup(&jwk);
    lonejson_jwt_header_cleanup(&header);
    lonejson_jwt_claims_cleanup(&claims);
  }
#endif
}

static void test_jwt_auth_provider_runtime_boundary(void) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_jwt_compact compact;
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwk jwk;
  lonejson_error error;
#ifdef LONEJSON_WITH_OPENSSL
  lonejson_auth_provider provider;
#endif

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  lonejson_jwk_init(&jwk);
  config = lonejson_default_config();
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);

  EXPECT(lonejson_jwt_parse_compact(test_jwt_rs256_token,
                                    strlen(test_jwt_rs256_token), &compact,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(runtime->jwt_decode_compact != NULL);
  EXPECT(runtime->jwk_parse_json != NULL);
  EXPECT(runtime->jwt_validate_signature_with_runtime != NULL);
  EXPECT(runtime->jwt_decode_compact(
             runtime, test_jwt_rs256_token, strlen(test_jwt_rs256_token), NULL,
             &header, &claims, &error) == LONEJSON_STATUS_OK);
  EXPECT(runtime->jwk_parse_json(runtime, test_jwt_rs256_jwk_json,
                                 strlen(test_jwt_rs256_jwk_json), &jwk,
                                 &error) == LONEJSON_STATUS_OK);
  EXPECT(runtime->jwt_validate_signature_with_runtime(runtime, &compact,
                                                      &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_set_auth_provider(runtime, NULL, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(runtime->set_auth_provider(runtime, NULL, &error) ==
         LONEJSON_STATUS_OK);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_auth_provider_init_openssl(&provider, NULL, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lj_set_auth_provider(runtime, &provider, &error) == LJ_STATUS_OK);
  EXPECT(runtime->jwt_validate_signature_with_runtime(
             runtime, &compact, &header, &jwk, &error) == LONEJSON_STATUS_OK);
  EXPECT(lj_jwt_validate_signature_with_runtime(runtime, &compact, &header,
                                                &jwk, &error) == LJ_STATUS_OK);
#endif

  lonejson_jwk_cleanup(&jwk);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
  lonejson_free(runtime);
}

static void test_jwt_validate_signature_failures(void) {
  lonejson_jwt_compact compact;
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwk jwk;
  lonejson_error error;
  char tampered[sizeof(test_jwt_rs256_token)];
  char oversized_n[11001];
  char *header_alg;
  char *jwk_alg;
  char *jwk_use;
  char *jwk_kty;
  char *jwk_kid;
  char *jwk_n;
  lonejson_string_array jwk_key_ops;
  char *disallowed_key_ops[1];

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  lonejson_jwk_init(&jwk);

  EXPECT(lonejson_jwt_parse_compact(test_jwt_rs256_token,
                                    strlen(test_jwt_rs256_token), &compact,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(
      lonejson_jwt_decode_compact(test_default_runtime(), test_jwt_rs256_token,
                                  strlen(test_jwt_rs256_token), NULL, &header,
                                  &claims, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwk_parse_json(test_default_runtime(),
                                 test_jwt_rs256_jwk_json,
                                 strlen(test_jwt_rs256_jwk_json), &jwk,
                                 &error) == LONEJSON_STATUS_OK);
  header_alg = header.alg;
  jwk_alg = jwk.alg;
  jwk_use = jwk.use;
  jwk_kty = jwk.kty;
  jwk_kid = jwk.kid;
  jwk_n = jwk.n;
  jwk_key_ops = jwk.key_ops;
  disallowed_key_ops[0] = (char *)"sign";

  memcpy(tampered, test_jwt_rs256_token, sizeof(tampered));
  tampered[sizeof(tampered) - 2u] =
      tampered[sizeof(tampered) - 2u] == 'A' ? 'B' : 'A';
  EXPECT(lonejson_jwt_parse_compact(tampered, strlen(tampered), &compact,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  EXPECT(lonejson_jwt_parse_compact(test_jwt_rs256_token,
                                    strlen(test_jwt_rs256_token), &compact,
                                    &error) == LONEJSON_STATUS_OK);
  header.alg = (char *)"ES256";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  header.alg = (char *)"none";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  header.alg = (char *)"RS256";
  jwk.kid = (char *)"other-key";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  jwk.kid = (char *)"rsa-test";
  jwk.alg = (char *)"ES256";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  jwk.alg = (char *)"RS256";
  jwk.use = (char *)"enc";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  jwk.use = (char *)"sig";
  jwk.key_ops.items = disallowed_key_ops;
  jwk.key_ops.count = 1u;
  jwk.key_ops.capacity = 1u;
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_validate_signature_with_runtime(
             test_default_runtime(), &compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  jwk.key_ops = jwk_key_ops;
  jwk.kty = (char *)"EC";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  jwk.kty = (char *)"RSA";
  memset(oversized_n, 'A', sizeof(oversized_n) - 1u);
  oversized_n[sizeof(oversized_n) - 1u] = '\0';
  jwk.n = oversized_n;
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
#else
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
#endif

  EXPECT(lonejson_jwt_validate_signature(NULL, &header, &jwk, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_jwt_validate_signature(&compact, NULL, &jwk, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  header.alg = header_alg;
  jwk.alg = jwk_alg;
  jwk.use = jwk_use;
  jwk.kty = jwk_kty;
  jwk.kid = jwk_kid;
  jwk.n = jwk_n;
  jwk.key_ops = jwk_key_ops;
  lonejson_jwk_cleanup(&jwk);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
}

static void test_jwt_claim_validation_failures(void) {
  static const char valid_token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJuYmYi"
      "OjkwMCwiaWF0IjoxMDAwfQ."
      "c2ln";
  static const char none_token[] =
      "eyJhbGciOiJub25lIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9.";
  static const char expired_token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjkwMH0."
      "c2ln";
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwt_claim_policy policy;
  lonejson_error error;
  lonejson_int64 min_i64 = -((lonejson_int64)(LONEJSON_UINT64_MAX >> 1)) - 1;
  const char *bad_issuer = "other";
  const char *bad_audience = "other";
  const char *none_alg = "none";
  const char *required_sub = "sub";
  const char *required_nonce = "nonce";

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  policy = test_jwt_policy();

  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), valid_token,
                                     strlen(valid_token), &policy, &header,
                                     &claims, &error) == LONEJSON_STATUS_OK);
  policy.accepted_issuers = &bad_issuer;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  policy = test_jwt_policy();
  policy.accepted_audiences = &bad_audience;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  policy = test_jwt_policy();
  policy.accepted_algs = &none_alg;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  policy = test_jwt_policy();
  policy.allowed_clock_skew = -1;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  policy = test_jwt_policy();
  policy.required_claims = &required_sub;
  policy.required_claim_count = 1u;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_OK);
  policy.required_claims = &required_nonce;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);

  policy = test_jwt_policy();
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), none_token,
                                     strlen(none_token), &policy, &header,
                                     &claims, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);

  policy = test_jwt_policy();
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), expired_token,
                                     strlen(expired_token), &policy, &header,
                                     &claims, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);

  memset(&header, 0, sizeof(header));
  memset(&claims, 0, sizeof(claims));
  policy = test_jwt_policy();
  header.alg = (char *)"RS256";
  claims.iss = (char *)"issuer";
  claims.aud = (char *)"api";
  claims.exp = min_i64;
  claims.has_exp = 1;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
}

static void test_jwt_decode_claim_failures(void) {
  static const char duplicate_header[] =
      "eyJhbGciOiJSUzI1NiIsImFsZyI6IkVTMjU2In0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char bad_aud_item[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOlsxXX0."
      "c2ln";
  static const char fractional_exp[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjEuNX0."
      "c2ln";
  static const char negative_exp[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOi0xfQ."
      "c2ln";
  static const char nonce_number[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDAsIm5vbmNlIjoxMjN9."
      "c2ln";
  static const char crit_object[] =
      "eyJhbGciOiJSUzI1NiIsImNyaXQiOnt9fQ."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char bad_crit_item[] =
      "eyJhbGciOiJSUzI1NiIsImNyaXQiOls0Ml19."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char crit_string[] =
      "eyJhbGciOiJSUzI1NiIsImNyaXQiOiJleHAtdGVzdCJ9."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char bad_scp_item[] =
      "eyJhbGciOiJSUzI1NiJ9."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJzY3AiOls0Ml0sImV4cCI6MjAwMH0."
      "c2ln";
  static const char bad_x5c[] =
      "eyJhbGciOiJSUzI1NiIsIng1YyI6WyJAQCJdfQ."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char x5c_string[] =
      "eyJhbGciOiJSUzI1NiIsIng1YyI6IkFBPT0ifQ."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char root_array[] =
      "W10."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char nul_alg[] =
      "eyJhbGciOiJSUzI1Nlx1MDAwMG5vbmUiLCJraWQiOiJrMSJ9."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char nul_kid[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxXHUwMDAwZXZpbCJ9."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char nul_typ[] =
      "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVFx1MDAwMGV2aWwifQ."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char nul_x5t[] =
      "eyJhbGciOiJSUzI1NiIsIng1dCI6IkFBXHUwMDAwZXZpbCJ9."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char nul_x5t_s256[] =
      "eyJhbGciOiJSUzI1NiIsIng1dCNTMjU2IjoiQUFcdTAwMDBldmlsIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char nul_x5c[] =
      "eyJhbGciOiJSUzI1NiIsIng1YyI6WyJBQT09XHUwMDAwZXZpbCJdfQ."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char nul_issuer[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXJcdTAwMDBldmlsIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAw"
      "fQ."
      "c2ln";
  static const char nul_sub[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzXHUwMDAwZXZpbCIsImF1ZCI6ImFwaSIs"
      "ImV4cCI6MjAwMH0."
      "c2ln";
  static const char nul_audience[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGlcdTAwMDBldmlsIiwiZXhwIjoyMDAw"
      "fQ."
      "c2ln";
  static const char nul_audience_array[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOlsiYXBpXHUwMDAwZXZpbCJdLCJleHAiOjIw"
      "MDB9."
      "c2ln";
  static const char nul_nonce[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJub25jZSI6Im5vbmNlXHUwMDAw"
      "ZXZpbCIsImV4cCI6MjAwMH0."
      "c2ln";
  static const char nul_scope[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJzY29wZSI6InJlYWRcdTAwMDB3"
      "cml0ZSIsImV4cCI6MjAwMH0."
      "c2ln";
  static const char nul_azp[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOlsiYXBpIiwiY2xpZW50Il0sImF6cCI6ImNs"
      "aWVudFx1MDAwMGV2aWwiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char nul_scp_string[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJzY3AiOiJhZG1pblx1MDAwMGV2"
      "aWwiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char nul_scp[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJzY3AiOlsiYWRtaW5cdTAwMDBl"
      "dmlsIl0sImV4cCI6MjAwMH0."
      "c2ln";
  static const char nul_crit[] =
      "eyJhbGciOiJSUzI1NiIsImNyaXQiOlsiZXhwLXRlc3RcdTAwMDBldmlsIl19."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwt_claim_policy policy;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  policy = test_jwt_policy();
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), duplicate_header,
                                     strlen(duplicate_header), &policy, &header,
                                     &claims, &error) ==
         LONEJSON_STATUS_DUPLICATE_FIELD);
  EXPECT(header.alg == NULL);
  EXPECT(claims.iss == NULL);

  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), bad_aud_item,
                                     strlen(bad_aud_item), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), fractional_exp,
                                     strlen(fractional_exp), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), negative_exp,
                                     strlen(negative_exp), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nonce_number,
                                     strlen(nonce_number), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), crit_object, strlen(crit_object), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), bad_crit_item,
                                     strlen(bad_crit_item), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), crit_string, strlen(crit_string), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), bad_scp_item,
                                     strlen(bad_scp_item), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), bad_x5c,
                                     strlen(bad_x5c), &policy, &header, &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), x5c_string, strlen(x5c_string), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), root_array, strlen(root_array), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_TYPE_MISMATCH);

  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_alg,
                                     strlen(nul_alg), &policy, &header, &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_kid,
                                     strlen(nul_kid), &policy, &header, &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_typ,
                                     strlen(nul_typ), &policy, &header, &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_x5t,
                                     strlen(nul_x5t), &policy, &header, &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_x5t_s256,
                                     strlen(nul_x5t_s256), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_x5c,
                                     strlen(nul_x5c), &policy, &header, &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), nul_issuer, strlen(nul_issuer), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_sub,
                                     strlen(nul_sub), &policy, &header, &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_audience,
                                     strlen(nul_audience), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_audience_array,
                                     strlen(nul_audience_array), &policy,
                                     &header, &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), nul_nonce, strlen(nul_nonce), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), nul_scope, strlen(nul_scope), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_azp,
                                     strlen(nul_azp), &policy, &header, &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_scp_string,
                                     strlen(nul_scp_string), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nul_scp,
                                     strlen(nul_scp), &policy, &header, &claims,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), nul_crit, strlen(nul_crit), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_INVALID_JSON);

  policy.max_token_bytes = 8u;
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), root_array, strlen(root_array), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_OVERFLOW);
  policy = test_jwt_policy();
  policy.max_decoded_claims_bytes = 4u;
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), root_array, strlen(root_array), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_OVERFLOW);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
}

#ifdef LONEJSON_WITH_OIDC
static void test_oidc_discovery_url(void) {
  lonejson_owned_buffer out;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_owned_buffer_init(&out);
  EXPECT(lonejson_oidc_discovery_url("https://id.example", &out, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data,
                "https://id.example/.well-known/openid-configuration") == 0);
  lonejson_owned_buffer_free(&out);

  lonejson_owned_buffer_init(&out);
  EXPECT(lj_oidc_discovery_url("https://id.example/tenant/", &out, &error) ==
         LJ_STATUS_OK);
  EXPECT(strcmp(out.data,
                "https://id.example/tenant/.well-known/openid-configuration") ==
         0);
  lonejson_owned_buffer_free(&out);

  lonejson_owned_buffer_init(&out);
  EXPECT(lonejson_oidc_discovery_url("http://id.example", &out, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_discovery_url("https://", &out, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_discovery_url("https://id.example?x=1", &out, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_discovery_url("https://id.example#frag", &out, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_discovery_url(NULL, &out, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_discovery_url("https://id.example", NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_owned_buffer_free(&out);
}

static void test_oidc_discovery_parse_and_validate(void) {
  static const char json[] =
      "{\"issuer\":\"https://id.example/tenant\","
      "\"authorization_endpoint\":\"https://id.example/auth\","
      "\"token_endpoint\":\"https://id.example/token\","
      "\"jwks_uri\":\"https://id.example/jwks\"}";
  lonejson_oidc_discovery discovery;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oidc_discovery_init(&discovery);
  EXPECT(lonejson_oidc_discovery_parse_json(test_default_runtime(), json,
                                            strlen(json), &discovery,
                                            &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(discovery.issuer, "https://id.example/tenant") == 0);
  EXPECT(strcmp(discovery.authorization_endpoint, "https://id.example/auth") ==
         0);
  EXPECT(strcmp(discovery.token_endpoint, "https://id.example/token") == 0);
  EXPECT(strcmp(discovery.jwks_uri, "https://id.example/jwks") == 0);
  EXPECT(lonejson_oidc_discovery_validate_issuer(&discovery,
                                                 "https://id.example/tenant",
                                                 &error) == LONEJSON_STATUS_OK);
  EXPECT(lj_oidc_discovery_validate_issuer(&discovery, "https://id.example",
                                           &error) == LJ_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_oidc_discovery_parse_json(
             test_default_runtime(),
             "{\"issuer\":\"https://id.example/other\","
             "\"token_endpoint\":\"https://id.example/token2\","
             "\"jwks_uri\":\"https://id.example/jwks2\"}",
             strlen("{\"issuer\":\"https://id.example/other\","
                    "\"token_endpoint\":\"https://id.example/token2\","
                    "\"jwks_uri\":\"https://id.example/jwks2\"}"),
             &discovery, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(discovery.issuer, "https://id.example/other") == 0);
  EXPECT(discovery.authorization_endpoint == NULL);
  EXPECT(strcmp(discovery.token_endpoint, "https://id.example/token2") == 0);
  EXPECT(strcmp(discovery.jwks_uri, "https://id.example/jwks2") == 0);
  lj_oidc_discovery_cleanup(&discovery);
  EXPECT(discovery.issuer == NULL);
}

static void test_oidc_discovery_failures(void) {
  lonejson_oidc_discovery discovery;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oidc_discovery_init(&discovery);
  {
    const char *json =
        "{\"issuer\":\"https://id.example\",\"jwks_uri\":\"https://id.example/"
        "jwks\"}";
    EXPECT(lonejson_oidc_discovery_parse_json(
               test_default_runtime(), json, strlen(json), &discovery,
               &error) == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  }
  EXPECT(discovery.issuer == NULL);

  {
    const char *json = "{\"issuer\":\"http://id.example\","
                       "\"token_endpoint\":\"https://id.example/token\","
                       "\"jwks_uri\":\"https://id.example/jwks\"}";
    EXPECT(lonejson_oidc_discovery_parse_json(
               test_default_runtime(), json, strlen(json), &discovery,
               &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  EXPECT(discovery.issuer == NULL);

  {
    const char *json = "{\"issuer\":\"https://id.example\","
                       "\"token_endpoint\":\"https://id.example/token\","
                       "\"jwks_uri\":\"/jwks\"}";
    EXPECT(lonejson_oidc_discovery_parse_json(
               test_default_runtime(), json, strlen(json), &discovery,
               &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  EXPECT(discovery.issuer == NULL);

  EXPECT(lonejson_oidc_discovery_parse_json(test_default_runtime(), "{}", 2u,
                                            NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_discovery_parse_json(test_default_runtime(), NULL, 1u,
                                            &discovery, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_discovery_validate_issuer(NULL, "https://id.example",
                                                 &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_discovery_validate_issuer(&discovery,
                                                 "http://id.example", &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  lonejson_oidc_discovery_cleanup(&discovery);
}

static lonejson_oidc_jwks_cache_policy test_oidc_jwks_cache_policy(void) {
  lonejson_oidc_jwks_cache_policy policy;

  memset(&policy, 0, sizeof(policy));
  policy.issuer = "https://id.example";
  policy.jwks_uri = "https://id.example/jwks";
  policy.max_jwks_bytes = 4096u;
  policy.now = 1000;
  policy.ttl_seconds = 60;
  return policy;
}

static void test_oidc_jwks_cache_update_and_select(void) {
  static const char jwks_json[] =
      "{\"keys\":["
      "{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\",\"alg\":\"RS256\","
      "\"n\":\"AQIDBA\",\"e\":\"AQAB\"},"
      "{\"kty\":\"EC\",\"kid\":\"ec\",\"use\":\"sig\",\"alg\":\"ES256\","
      "\"crv\":\"P-256\",\"x\":\"AAEC\",\"y\":\"AwQF\"}"
      "]}";
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy policy;
  lonejson_jwk_select_options options;
  const lonejson_jwk *selected;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oidc_jwks_cache_init(&cache);
  policy = test_oidc_jwks_cache_policy();
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, jwks_json,
             strlen(jwks_json), &error) == LONEJSON_STATUS_OK);
  EXPECT(cache.has_jwks);
  EXPECT(cache.fetched_at == 1000);
  EXPECT(cache.expires_at == 1060);
  EXPECT(strcmp(cache.issuer, "https://id.example") == 0);
  EXPECT(strcmp(cache.jwks_uri, "https://id.example/jwks") == 0);
  EXPECT(lj_oidc_jwks_cache_is_fresh(&cache, &policy));
  {
    lonejson_oidc_jwks_cache_policy invalid_policy;
    memset(&invalid_policy, 0, sizeof(invalid_policy));
    EXPECT(!lonejson_oidc_jwks_cache_is_fresh(&cache, &invalid_policy));
    invalid_policy = test_oidc_jwks_cache_policy();
    invalid_policy.issuer = NULL;
    EXPECT(!lonejson_oidc_jwks_cache_is_fresh(&cache, &invalid_policy));
    invalid_policy = test_oidc_jwks_cache_policy();
    invalid_policy.jwks_uri = NULL;
    EXPECT(!lonejson_oidc_jwks_cache_is_fresh(&cache, &invalid_policy));
    invalid_policy = test_oidc_jwks_cache_policy();
    invalid_policy.now = -1;
    EXPECT(!lonejson_oidc_jwks_cache_is_fresh(&cache, &invalid_policy));
  }

  memset(&options, 0, sizeof(options));
  options.kid = "rsa-test";
  options.kty = "RSA";
  options.alg = "RS256";
  options.use = "sig";
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, &policy, &options, &selected,
                                         &error) == LONEJSON_STATUS_OK);
  EXPECT(selected != NULL);
  EXPECT(strcmp(selected->kid, "rsa-test") == 0);

  policy.now = 1060;
  EXPECT(!lonejson_oidc_jwks_cache_is_fresh(&cache, &policy));
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, &policy, &options, &selected,
                                         &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  policy = test_oidc_jwks_cache_policy();
  policy.issuer = "https://other.example";
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, &policy, &options, &selected,
                                         &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  lonejson_oidc_jwks_cache_cleanup(&cache);
  EXPECT(cache.jwks.keys.items == NULL);
}

static void test_oidc_jwks_cache_failure_modes(void) {
  static const char good_jwks[] =
      "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\","
      "\"alg\":\"RS256\",\"n\":\"AQIDBA\",\"e\":\"AQAB\"}]}";
  static const char bad_jwks[] = "{\"keys\":[]}";
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy policy;
  lonejson_jwk_select_options options;
  const lonejson_jwk *selected;
  lonejson_error error;
  const char *old_issuer;

  lonejson_error_init(&error);
  lonejson_oidc_jwks_cache_init(&cache);
  memset(&options, 0, sizeof(options));
  policy = test_oidc_jwks_cache_policy();
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, good_jwks,
             strlen(good_jwks), &error) == LONEJSON_STATUS_OK);
  old_issuer = cache.issuer;

  policy.max_jwks_bytes = 4u;
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, good_jwks,
             strlen(good_jwks), &error) == LONEJSON_STATUS_OVERFLOW);
  EXPECT(cache.issuer == old_issuer);

  policy = test_oidc_jwks_cache_policy();
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, bad_jwks,
             strlen(bad_jwks), &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(cache.issuer == old_issuer);

  policy.ttl_seconds = 0;
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, good_jwks,
             strlen(good_jwks), &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  policy = test_oidc_jwks_cache_policy();
  policy.now = -1;
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, good_jwks,
             strlen(good_jwks), &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, &policy, &options, &selected,
                                         &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  policy = test_oidc_jwks_cache_policy();
  policy.issuer = "http://id.example";
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, good_jwks,
             strlen(good_jwks), &error) == LONEJSON_STATUS_INVALID_JSON);
  policy = test_oidc_jwks_cache_policy();
  policy.now = (lonejson_int64)(LONEJSON_UINT64_MAX >> 1) - 1;
  policy.ttl_seconds = 2;
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, good_jwks,
             strlen(good_jwks), &error) == LONEJSON_STATUS_OVERFLOW);

  EXPECT(lonejson_oidc_jwks_cache_select(&cache, NULL, &options, &selected,
                                         &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, &policy, &options, NULL,
                                         &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_oidc_jwks_cache_cleanup(&cache);
}

static void test_oidc_jwks_cache_curl_adapter(void) {
#ifdef LONEJSON_WITH_CURL
  static const char chunk0[] =
      "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"rsa-test\",";
  static const char chunk1[] =
      "\"use\":\"sig\",\"alg\":\"RS256\",\"n\":\"AQIDBA\",\"e\":\"AQAB\"}]}";
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_parse parse = {0};
  lonejson_oidc_jwks_cache_policy policy;
  lonejson_jwk_select_options options;
  const lonejson_jwk *selected;
  size_t wrote;

  lonejson_oidc_jwks_cache_init(&cache);
  memset(&parse, 0, sizeof(parse));
  policy = test_oidc_jwks_cache_policy();
  EXPECT(test_default_runtime()->oidc_jwks_cache_parse_init(
             test_default_runtime(), &parse, &cache, &policy) ==
         LONEJSON_STATUS_OK);
  wrote = lonejson_oidc_jwks_cache_write_callback((char *)chunk0, 1u,
                                                  strlen(chunk0), &parse);
  EXPECT(wrote == strlen(chunk0));
  wrote = parse.write_callback(&parse, (char *)chunk1, 1u, strlen(chunk1));
  EXPECT(wrote == strlen(chunk1));
  EXPECT(parse.finish(&parse) == LONEJSON_STATUS_OK);

  memset(&options, 0, sizeof(options));
  options.kid = "rsa-test";
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, &policy, &options, &selected,
                                         &parse.error) == LONEJSON_STATUS_OK);
  EXPECT(selected != NULL);
  parse.cleanup(&parse);

  policy.max_jwks_bytes = 8u;
  EXPECT(lj_oidc_jwks_cache_parse_init(&parse, test_default_runtime(), &cache,
                                       &policy) == LJ_STATUS_OK);
  wrote = lj_oidc_jwks_cache_write_callback((char *)chunk0, 1u, strlen(chunk0),
                                            &parse);
  EXPECT(wrote == 0u);
  EXPECT(lonejson_oidc_jwks_cache_parse_finish(&parse) ==
         LONEJSON_STATUS_OVERFLOW);
  lonejson_oidc_jwks_cache_parse_cleanup(&parse);
  lonejson_oidc_jwks_cache_cleanup(&cache);
#endif
}

typedef struct test_oidc_http_provider_state {
  int fail_callback;
  int ignore_response_limit;
  int token_transient_failures;
  int requests;
} test_oidc_http_provider_state;

static lonejson_status
test_oidc_http_provider_respond(lonejson_http_response *response,
                                const char *body, size_t max_bytes,
                                lonejson_error *error) {
  size_t len = strlen(body);
  lonejson_http_response_cleanup(response);
  if (max_bytes != 0u && len > max_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "test response exceeds configured limit");
  }
  response->status_code = 200;
  return lonejson_owned_buffer_sink(&response->body, body, len, error);
}

static lonejson_status test_oidc_http_provider_request(
    void *user_data, const lonejson_http_request *request,
    lonejson_http_response *response, lonejson_error *error) {
  static const char discovery_json[] =
      "{\"issuer\":\"https://issuer.example\","
      "\"authorization_endpoint\":\"https://issuer.example/auth\","
      "\"token_endpoint\":\"https://issuer.example/token\","
      "\"jwks_uri\":\"https://issuer.example/jwks\","
      "\"introspection_endpoint\":\"https://issuer.example/introspect\","
      "\"revocation_endpoint\":\"https://issuer.example/revoke\","
      "\"userinfo_endpoint\":\"https://issuer.example/userinfo\"}";
  static const char token_json[] =
      "{\"access_token\":\"abc.def.sig\",\"token_type\":\"Bearer\","
      "\"expires_in\":60}";
  static const char introspection_json[] =
      "{\"active\":true,\"scope\":\"read write\",\"client_id\":\"client\","
      "\"sub\":\"subject\",\"aud\":\"api\",\"iss\":\"https://issuer.example\","
      "\"exp\":123,\"iat\":100,\"nbf\":90}";
  static const char userinfo_json[] =
      "{\"sub\":\"subject\",\"email\":\"user@example.com\","
      "\"email_verified\":true,\"custom\":1}";
  test_oidc_http_provider_state *state =
      (test_oidc_http_provider_state *)user_data;

  EXPECT(state != NULL);
  EXPECT(request != NULL);
  EXPECT(response != NULL);
  ++state->requests;
  if (state->fail_callback) {
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "test HTTP provider failed");
  }
  if (strcmp(request->url,
             "https://issuer.example/.well-known/openid-configuration") == 0) {
    EXPECT(strcmp(request->method, "GET") == 0);
    EXPECT(request->user_agent != NULL);
    EXPECT(strcmp(request->user_agent, "lonejson-test/1") == 0);
    EXPECT(request->body == NULL);
    EXPECT(request->body_len == 0u);
    return test_oidc_http_provider_respond(
        response, discovery_json,
        state->ignore_response_limit ? 0u : request->max_response_bytes, error);
  }
  if (strcmp(request->url, "https://issuer.example/jwks") == 0) {
    EXPECT(strcmp(request->method, "GET") == 0);
    EXPECT(request->user_agent != NULL);
    EXPECT(strcmp(request->user_agent, "lonejson-test/1") == 0);
    EXPECT(request->body == NULL);
    EXPECT(request->body_len == 0u);
    return test_oidc_http_provider_respond(response, test_jwt_rs256_jwks_json,
                                           request->max_response_bytes, error);
  }
  if (strcmp(request->url, "https://issuer.example/token") == 0) {
    EXPECT(strcmp(request->method, "POST") == 0);
    EXPECT(request->user_agent != NULL);
    EXPECT(strcmp(request->user_agent, "lonejson-test/1") == 0);
    EXPECT(request->content_type != NULL);
    EXPECT(strcmp(request->content_type, "application/x-www-form-urlencoded") ==
           0);
    EXPECT(request->body != NULL);
    EXPECT(request->body_len != 0u);
    EXPECT(strstr((const char *)request->body, "grant_type=") != NULL);
    if (state->token_transient_failures > 0) {
      --state->token_transient_failures;
      lonejson_http_response_cleanup(response);
      response->status_code = 500;
      return lonejson_owned_buffer_sink(
          &response->body, "{\"error\":\"temporarily_unavailable\"}", 35u,
          error);
    }
    return test_oidc_http_provider_respond(response, token_json,
                                           request->max_response_bytes, error);
  }
  if (strcmp(request->url, "https://issuer.example/introspect") == 0) {
    EXPECT(strcmp(request->method, "POST") == 0);
    EXPECT(request->content_type != NULL);
    EXPECT(strcmp(request->content_type, "application/x-www-form-urlencoded") ==
           0);
    EXPECT(request->body != NULL);
    EXPECT(strstr((const char *)request->body, "token=access") != NULL ||
           strstr((const char *)request->body, "token=reserved") != NULL);
    EXPECT(strstr((const char *)request->body,
                  "token_type_hint=access_token") != NULL);
    if (request->authorization != NULL) {
      if (strstr((const char *)request->body, "token=reserved") != NULL) {
        EXPECT(strcmp(request->authorization,
                      "Basic Y2xpZW50K2lkOnNlYyUyQnJldCUyNiUyNQ==") == 0);
      } else {
        EXPECT(strcmp(request->authorization, "Basic Y2xpZW50OnNlY3JldA==") ==
               0);
      }
      EXPECT(strstr((const char *)request->body, "client_secret=") == NULL);
    }
    return test_oidc_http_provider_respond(response, introspection_json,
                                           request->max_response_bytes, error);
  }
  if (strcmp(request->url, "https://issuer.example/revoke") == 0) {
    EXPECT(strcmp(request->method, "POST") == 0);
    EXPECT(request->content_type != NULL);
    EXPECT(strcmp(request->content_type, "application/x-www-form-urlencoded") ==
           0);
    EXPECT(request->body != NULL);
    EXPECT(strstr((const char *)request->body, "token=refresh") != NULL);
    EXPECT(strstr((const char *)request->body,
                  "token_type_hint=refresh_token") != NULL);
    if (request->authorization != NULL) {
      EXPECT(strcmp(request->authorization, "Basic Y2xpZW50OnNlY3JldA==") == 0);
      EXPECT(strstr((const char *)request->body, "client_secret=") == NULL);
    }
    return test_oidc_http_provider_respond(response, "{}", 0u, error);
  }
  if (strcmp(request->url, "https://issuer.example/userinfo") == 0) {
    EXPECT(strcmp(request->method, "GET") == 0);
    EXPECT(request->authorization != NULL);
    EXPECT(strcmp(request->authorization, "Bearer access") == 0);
    EXPECT(request->body == NULL);
    EXPECT(request->body_len == 0u);
    return test_oidc_http_provider_respond(response, userinfo_json,
                                           request->max_response_bytes, error);
  }
  lonejson_http_response_cleanup(response);
  response->status_code = 404;
  return lonejson_owned_buffer_sink(&response->body, "not found", 9u, error);
}

static void test_oidc_http_provider_helpers(void) {
  lonejson *runtime;
  lonejson_error error;
  lonejson_http_provider provider;
  lonejson_http_provider_config provider_config;
  test_oidc_http_provider_state state;
  lonejson_oidc_discovery discovery;
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy policy;
  lonejson_oauth2_client_credentials token_request;
  lonejson_oauth2_refresh_token refresh_request;
  lonejson_oauth2_token_introspection introspection_request;
  lonejson_oauth2_token_revocation revocation_request;
  lonejson_oidc_authorization_code_token code_request;
  lonejson_oauth2_token_response token_response;
  lonejson_oauth2_introspection_response introspection_response;
  lonejson_oidc_userinfo_request userinfo_request;
  lonejson_oidc_userinfo_response userinfo_response;
  static const char discovery_json[] =
      "{\"issuer\":\"https://issuer.example\","
      "\"authorization_endpoint\":\"https://issuer.example/auth\","
      "\"token_endpoint\":\"https://issuer.example/token\","
      "\"jwks_uri\":\"https://issuer.example/jwks\","
      "\"introspection_endpoint\":\"https://issuer.example/introspect\","
      "\"revocation_endpoint\":\"https://issuer.example/revoke\","
      "\"userinfo_endpoint\":\"https://issuer.example/userinfo\"}";

  lonejson_error_init(&error);
  memset(&provider, 0, sizeof(provider));
  memset(&provider_config, 0, sizeof(provider_config));
  memset(&state, 0, sizeof(state));
  runtime = lonejson_new(NULL, &error);
  EXPECT(runtime != NULL);

  EXPECT(lonejson_set_http_provider(runtime, &provider, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_fetch_discovery(runtime, "https://issuer.example", 4096u,
                                       &discovery, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  provider_config.user_data = &state;
  provider_config.user_agent = "lonejson-test/1";
  provider_config.request = test_oidc_http_provider_request;
  EXPECT(lonejson_http_provider_init(&provider, &provider_config, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(provider.user_data == &state);
  EXPECT(provider.user_agent == provider_config.user_agent);
  EXPECT(provider.request == test_oidc_http_provider_request);
  EXPECT(lonejson_http_provider_init_simple(
             &provider, &state, "lonejson-test/1",
             test_oidc_http_provider_request, &error) == LONEJSON_STATUS_OK);
  EXPECT(provider.user_data == &state);
  EXPECT(provider.user_agent != NULL);
  EXPECT(strcmp(provider.user_agent, "lonejson-test/1") == 0);
  EXPECT(provider.request == test_oidc_http_provider_request);
  EXPECT(runtime->set_http_provider(runtime, &provider, &error) ==
         LONEJSON_STATUS_OK);

  lonejson_oidc_discovery_init(&discovery);
  EXPECT(runtime->oidc_discovery_parse_json != NULL);
  EXPECT(runtime->oidc_fetch_discovery != NULL);
  EXPECT(runtime->oidc_jwks_cache_update_json != NULL);
  EXPECT(runtime->oidc_jwks_cache_refresh != NULL);
  EXPECT(runtime->oauth2_token_response_parse_json != NULL);
  EXPECT(runtime->oauth2_client_credentials_request != NULL);
  EXPECT(runtime->oauth2_refresh_token_request != NULL);
  EXPECT(runtime->oauth2_introspect_token_request != NULL);
  EXPECT(runtime->oauth2_revoke_token_request != NULL);
  EXPECT(runtime->oidc_fetch_userinfo != NULL);
  EXPECT(runtime->oidc_authorization_code_token_request != NULL);
  EXPECT(runtime->oidc_validate_bearer_token != NULL);
  EXPECT(runtime->oidc_discovery_parse_json(runtime, discovery_json,
                                            strlen(discovery_json), &discovery,
                                            &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(discovery.issuer, "https://issuer.example") == 0);
  EXPECT(strcmp(discovery.introspection_endpoint,
                "https://issuer.example/introspect") == 0);
  EXPECT(strcmp(discovery.revocation_endpoint,
                "https://issuer.example/revoke") == 0);
  EXPECT(strcmp(discovery.userinfo_endpoint,
                "https://issuer.example/userinfo") == 0);
  lonejson_oidc_discovery_cleanup(&discovery);

  lonejson_oidc_discovery_init(&discovery);
  EXPECT(runtime->oidc_fetch_discovery(runtime, "https://issuer.example", 4096u,
                                       &discovery,
                                       &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(discovery.issuer, "https://issuer.example") == 0);
  EXPECT(strcmp(discovery.jwks_uri, "https://issuer.example/jwks") == 0);
  EXPECT(strcmp(discovery.introspection_endpoint,
                "https://issuer.example/introspect") == 0);
  EXPECT(strcmp(discovery.revocation_endpoint,
                "https://issuer.example/revoke") == 0);
  EXPECT(strcmp(discovery.userinfo_endpoint,
                "https://issuer.example/userinfo") == 0);
  lonejson_oidc_discovery_cleanup(&discovery);

  state.ignore_response_limit = 1;
  lonejson_oidc_discovery_init(&discovery);
  EXPECT(runtime->oidc_fetch_discovery(runtime, "https://issuer.example", 16u,
                                       &discovery,
                                       &error) == LONEJSON_STATUS_OVERFLOW);
  lonejson_oidc_discovery_cleanup(&discovery);
  state.ignore_response_limit = 0;

  lonejson_oidc_jwks_cache_init(&cache);
  policy = test_oidc_jwks_cache_policy();
  policy.issuer = "https://issuer.example";
  policy.jwks_uri = "https://issuer.example/jwks";
  EXPECT(runtime->oidc_jwks_cache_update_json(
             runtime, &cache, &policy, test_jwt_rs256_jwks_json,
             strlen(test_jwt_rs256_jwks_json), &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_oidc_jwks_cache_is_fresh(&cache, &policy));
  lonejson_oidc_jwks_cache_cleanup(&cache);

  lonejson_oidc_jwks_cache_init(&cache);
  EXPECT(runtime->oidc_jwks_cache_refresh(runtime, &cache, &policy, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_oidc_jwks_cache_is_fresh(&cache, &policy));
  lonejson_oidc_jwks_cache_cleanup(&cache);

  memset(&token_request, 0, sizeof(token_request));
  token_request.client_id = "client";
  token_request.client_secret = "secret";
  lonejson_oauth2_token_response_init(&token_response);
  EXPECT(runtime->oauth2_token_response_parse_json(
             runtime,
             "{\"access_token\":\"parsed\",\"token_type\":\"Bearer\","
             "\"expires_in\":1}",
             strlen("{\"access_token\":\"parsed\",\"token_type\":\"Bearer\","
                    "\"expires_in\":1}"),
             4096u, &token_response, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(token_response.access_token, "parsed") == 0);
  lonejson_oauth2_token_response_cleanup(&token_response);

  EXPECT(runtime->oauth2_client_credentials_request(
             runtime, "https://issuer.example/token", &token_request, 4096u,
             &token_response, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(token_response.access_token, "abc.def.sig") == 0);
  EXPECT(token_response.has_expires_in);
  EXPECT(token_response.expires_in == 60);
  lonejson_oauth2_token_response_cleanup(&token_response);

  memset(&refresh_request, 0, sizeof(refresh_request));
  refresh_request.refresh_token = "refresh token";
  refresh_request.client_id = "client";
  refresh_request.client_secret = "secret";
  EXPECT(runtime->oauth2_refresh_token_request(
             runtime, "https://issuer.example/token", &refresh_request, 4096u,
             &token_response, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(token_response.access_token, "abc.def.sig") == 0);
  lonejson_oauth2_token_response_cleanup(&token_response);

  memset(&code_request, 0, sizeof(code_request));
  code_request.client_id = "client";
  code_request.code = "code+123";
  code_request.redirect_uri = "http://127.0.0.1/cb";
  code_request.code_verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
  EXPECT(runtime->oidc_authorization_code_token_request(
             runtime, "https://issuer.example/token", &code_request, 4096u,
             &token_response, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(token_response.token_type, "Bearer") == 0);
  lonejson_oauth2_token_response_cleanup(&token_response);

  EXPECT(runtime->oauth2_client_credentials_request(
             runtime, "https://issuer.example/token", &token_request, 8u,
             &token_response, &error) == LONEJSON_STATUS_OVERFLOW);

  memset(&introspection_request, 0, sizeof(introspection_request));
  introspection_request.token = "access";
  introspection_request.token_type_hint = "access_token";
  introspection_request.client_id = "client";
  introspection_request.client_secret = "secret";
  lonejson_oauth2_introspection_response_init(&introspection_response);
  EXPECT(runtime->oauth2_introspect_token_request(
             runtime, "https://issuer.example/introspect",
             &introspection_request, 4096u, &introspection_response,
             &error) == LONEJSON_STATUS_OK);
  EXPECT(introspection_response.has_active);
  EXPECT(introspection_response.active);
  EXPECT(strcmp(introspection_response.scope, "read write") == 0);
  EXPECT(strcmp(introspection_response.client_id, "client") == 0);
  EXPECT(strcmp(introspection_response.sub, "subject") == 0);
  EXPECT(strcmp(introspection_response.aud, "api") == 0);
  EXPECT(introspection_response.has_exp);
  EXPECT(introspection_response.exp == 123);
  lonejson_oauth2_introspection_response_cleanup(&introspection_response);

  introspection_request.token = "reserved";
  introspection_request.client_id = "client id";
  introspection_request.client_secret = "sec+ret&%";
  introspection_request.use_basic_auth = 1;
  lonejson_oauth2_introspection_response_init(&introspection_response);
  EXPECT(runtime->oauth2_introspect_token_request(
             runtime, "https://issuer.example/introspect",
             &introspection_request, 4096u, &introspection_response,
             &error) == LONEJSON_STATUS_OK);
  EXPECT(introspection_response.active);
  lonejson_oauth2_introspection_response_cleanup(&introspection_response);
  introspection_request.use_basic_auth = 0;
  introspection_request.token = "access";
  introspection_request.client_id = "client";
  introspection_request.client_secret = "secret";

  memset(&revocation_request, 0, sizeof(revocation_request));
  revocation_request.token = "refresh";
  revocation_request.token_type_hint = "refresh_token";
  revocation_request.client_id = "client";
  revocation_request.client_secret = "secret";
  EXPECT(runtime->oauth2_revoke_token_request(
             runtime, "https://issuer.example/revoke", &revocation_request,
             &error) == LONEJSON_STATUS_OK);
  revocation_request.use_basic_auth = 1;
  EXPECT(runtime->oauth2_revoke_token_request(
             runtime, "https://issuer.example/revoke", &revocation_request,
             &error) == LONEJSON_STATUS_OK);

  memset(&userinfo_request, 0, sizeof(userinfo_request));
  userinfo_request.access_token = "access";
  lonejson_oidc_userinfo_response_init(&userinfo_response);
  EXPECT(runtime->oidc_fetch_userinfo(
             runtime, "https://issuer.example/userinfo", &userinfo_request,
             &userinfo_response, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(userinfo_response.sub, "subject") == 0);
  EXPECT(strcmp(userinfo_response.email, "user@example.com") == 0);
  EXPECT(userinfo_response.has_email_verified);
  EXPECT(userinfo_response.email_verified);
  EXPECT(userinfo_response.json != NULL);
  EXPECT(strstr(userinfo_response.json, "\"custom\":1") != NULL);
  lonejson_oidc_userinfo_response_cleanup(&userinfo_response);

  EXPECT(state.requests >= 11);

  state.fail_callback = 1;
  EXPECT(runtime->oidc_fetch_discovery(runtime, "https://issuer.example", 4096u,
                                       &discovery, &error) ==
         LONEJSON_STATUS_CALLBACK_FAILED);
  state.fail_callback = 0;

  EXPECT(lonejson_set_http_provider(runtime, NULL, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_oidc_fetch_discovery(runtime, "https://issuer.example", 4096u,
                                       &discovery, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  EXPECT(lonejson_http_provider_init(&provider, NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  provider_config.request = NULL;
  EXPECT(lonejson_http_provider_init(&provider, &provider_config, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lj_http_provider_init_simple(&provider, &state, "lonejson-test/1",
                                      NULL,
                                      &error) == LJ_STATUS_INVALID_ARGUMENT);

  lonejson_free(runtime);
}

static void test_oauth2_client_credentials_body(void) {
  lonejson_oauth2_client_credentials request;
  lonejson_oauth2_refresh_token refresh_request;
  lonejson_oauth2_token_introspection introspection_request;
  lonejson_oauth2_token_revocation revocation_request;
  lonejson_oidc_authorization_code_token code_request;
  lonejson_owned_buffer out;
  lonejson_error error;

  memset(&request, 0, sizeof(request));
  lonejson_owned_buffer_init(&out);
  lonejson_error_init(&error);
  request.client_id = "client id";
  request.client_secret = "s+e&c=r%t";
  request.scope = "read write";
  request.audience = "https://api.example/a?b=c";
  request.resource = "urn:example:resource";
  EXPECT(lonejson_oauth2_client_credentials_body(&request, &out, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data, "grant_type=client_credentials&client_id=client+id&"
                          "client_secret=s%2Be%26c%3Dr%25t&scope=read+write&"
                          "audience=https%3A%2F%2Fapi.example%2Fa%3Fb%3Dc&"
                          "resource=urn%3Aexample%3Aresource") == 0);
  lonejson_owned_buffer_free(&out);

  request.max_body_bytes = 8u;
  EXPECT(lj_oauth2_client_credentials_body(&request, &out, &error) ==
         LJ_STATUS_OVERFLOW);
  EXPECT(out.data == NULL);

  request.max_body_bytes = 0u;
  request.client_secret = "";
  EXPECT(lonejson_oauth2_client_credentials_body(&request, &out, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  request.client_secret = "secret";
  request.client_id = NULL;
  EXPECT(lonejson_oauth2_client_credentials_body(&request, &out, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&refresh_request, 0, sizeof(refresh_request));
  refresh_request.refresh_token = "r+e&f";
  refresh_request.client_id = "client id";
  refresh_request.client_secret = "secret";
  refresh_request.scope = "read write";
  EXPECT(lonejson_oauth2_refresh_token_body(&refresh_request, &out, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data,
                "grant_type=refresh_token&refresh_token=r%2Be%26f&"
                "client_id=client+id&client_secret=secret&scope=read+write") ==
         0);
  lonejson_owned_buffer_free(&out);

  refresh_request.max_body_bytes = 8u;
  EXPECT(lonejson_oauth2_refresh_token_body(&refresh_request, &out, &error) ==
         LONEJSON_STATUS_OVERFLOW);
  refresh_request.max_body_bytes = 0u;
  refresh_request.refresh_token = NULL;
  EXPECT(lonejson_oauth2_refresh_token_body(&refresh_request, &out, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  refresh_request.refresh_token = "refresh";
  refresh_request.client_id = NULL;
  EXPECT(lonejson_oauth2_refresh_token_body(&refresh_request, &out, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&introspection_request, 0, sizeof(introspection_request));
  introspection_request.token = "a+b&c";
  introspection_request.token_type_hint = "access_token";
  introspection_request.client_id = "client id";
  introspection_request.client_secret = "secret";
  EXPECT(lonejson_oauth2_token_introspection_body(
             &introspection_request, &out, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data, "token=a%2Bb%26c&token_type_hint=access_token&"
                          "client_id=client+id&client_secret=secret") == 0);
  lonejson_owned_buffer_free(&out);

  introspection_request.max_body_bytes = 8u;
  EXPECT(lj_oauth2_token_introspection_body(&introspection_request, &out,
                                            &error) == LJ_STATUS_OVERFLOW);
  introspection_request.max_body_bytes = 0u;
  introspection_request.token = NULL;
  EXPECT(lonejson_oauth2_token_introspection_body(&introspection_request, &out,
                                                  &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  introspection_request.token = "token";
  introspection_request.client_id = NULL;
  EXPECT(lonejson_oauth2_token_introspection_body(&introspection_request, &out,
                                                  &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  introspection_request.client_id = "client";
  introspection_request.client_secret = "secret";
  introspection_request.use_basic_auth = 1;
  EXPECT(lonejson_oauth2_token_introspection_body(
             &introspection_request, &out, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data, "token=token&token_type_hint=access_token") == 0);
  EXPECT(strstr(out.data, "client_secret") == NULL);
  lonejson_owned_buffer_free(&out);

  memset(&revocation_request, 0, sizeof(revocation_request));
  revocation_request.token = "refresh";
  revocation_request.token_type_hint = "refresh_token";
  revocation_request.client_id = "client";
  revocation_request.client_secret = "secret";
  EXPECT(lonejson_oauth2_token_revocation_body(&revocation_request, &out,
                                               &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data, "token=refresh&token_type_hint=refresh_token&"
                          "client_id=client&client_secret=secret") == 0);
  lonejson_owned_buffer_free(&out);

  revocation_request.max_body_bytes = 8u;
  EXPECT(lj_oauth2_token_revocation_body(&revocation_request, &out, &error) ==
         LJ_STATUS_OVERFLOW);
  revocation_request.max_body_bytes = 0u;
  revocation_request.token = NULL;
  EXPECT(lonejson_oauth2_token_revocation_body(&revocation_request, &out,
                                               &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  revocation_request.token = "token";
  revocation_request.client_id = NULL;
  EXPECT(lonejson_oauth2_token_revocation_body(&revocation_request, &out,
                                               &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  revocation_request.client_id = "client";
  revocation_request.client_secret = "secret";
  revocation_request.use_basic_auth = 1;
  EXPECT(lonejson_oauth2_token_revocation_body(&revocation_request, &out,
                                               &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data, "token=token&token_type_hint=refresh_token") == 0);
  EXPECT(strstr(out.data, "client_secret") == NULL);
  lonejson_owned_buffer_free(&out);

  memset(&code_request, 0, sizeof(code_request));
  code_request.client_id = "client id";
  code_request.code = "code+123";
  code_request.redirect_uri = "http://127.0.0.1:1234/cb";
  code_request.code_verifier = "verifier value";
  code_request.client_secret = "secret";
  EXPECT(lonejson_oidc_authorization_code_token_body(
             &code_request, &out, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data,
                "grant_type=authorization_code&client_id=client+id&"
                "code=code%2B123&"
                "redirect_uri=http%3A%2F%2F127.0.0.1%3A1234%2Fcb&"
                "code_verifier=verifier+value&client_secret=secret") == 0);
  lonejson_owned_buffer_free(&out);

  code_request.max_body_bytes = 8u;
  EXPECT(lj_oidc_authorization_code_token_body(&code_request, &out, &error) ==
         LJ_STATUS_OVERFLOW);
  code_request.max_body_bytes = 0u;
  code_request.code_verifier = NULL;
  EXPECT(lonejson_oidc_authorization_code_token_body(
             &code_request, &out, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_owned_buffer_free(&out);
}

static void test_oauth2_token_response_parse(void) {
  static const char response_json[] =
      "{\"access_token\":\"token\",\"token_type\":\"bearer\","
      "\"expires_in\":3600,\"scope\":\"read write\","
      "\"refresh_token\":\"refresh\",\"id_token\":\"id.jwt\"}";
  static const char introspection_json[] =
      "{\"active\":false,\"scope\":\"read\",\"client_id\":\"client\","
      "\"username\":\"user\",\"token_type\":\"Bearer\",\"sub\":\"sub\","
      "\"aud\":\"aud\",\"iss\":\"https://issuer.example\",\"jti\":\"jti\","
      "\"exp\":10,\"iat\":2,\"nbf\":1}";
  static const char userinfo_json[] =
      "{\"sub\":\"sub\",\"name\":\"User\",\"preferred_username\":\"user\","
      "\"email\":\"user@example.com\",\"email_verified\":false,"
      "\"extra\":{\"ok\":true}}";
  lonejson_oauth2_token_response response;
  lonejson_oauth2_introspection_response introspection;
  lonejson_oidc_userinfo_response userinfo;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oauth2_token_response_init(&response);
  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(), response_json, strlen(response_json), 0u,
             &response, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(response.access_token, "token") == 0);
  EXPECT(strcmp(response.token_type, "bearer") == 0);
  EXPECT(response.has_expires_in);
  EXPECT(response.expires_in == 3600);
  EXPECT(strcmp(response.scope, "read write") == 0);
  EXPECT(strcmp(response.refresh_token, "refresh") == 0);
  EXPECT(strcmp(response.id_token, "id.jwt") == 0);
  lonejson_oauth2_token_response_cleanup(&response);
  EXPECT(response.access_token == NULL);

  lonejson_oauth2_introspection_response_init(&introspection);
  EXPECT(lj_oauth2_introspection_response_parse_json(
             test_default_runtime(), introspection_json,
             strlen(introspection_json), 0u, &introspection,
             &error) == LJ_STATUS_OK);
  EXPECT(introspection.has_active);
  EXPECT(!introspection.active);
  EXPECT(strcmp(introspection.scope, "read") == 0);
  EXPECT(strcmp(introspection.client_id, "client") == 0);
  EXPECT(strcmp(introspection.username, "user") == 0);
  EXPECT(strcmp(introspection.token_type, "Bearer") == 0);
  EXPECT(strcmp(introspection.sub, "sub") == 0);
  EXPECT(strcmp(introspection.aud, "aud") == 0);
  EXPECT(strcmp(introspection.iss, "https://issuer.example") == 0);
  EXPECT(strcmp(introspection.jti, "jti") == 0);
  EXPECT(introspection.has_exp && introspection.exp == 10);
  EXPECT(introspection.has_iat && introspection.iat == 2);
  EXPECT(introspection.has_nbf && introspection.nbf == 1);
  lonejson_oauth2_introspection_response_cleanup(&introspection);
  EXPECT(introspection.aud == NULL);

  reset_lonejson_alloc_stats();
  lonejson_oauth2_introspection_response_init(&introspection);
  EXPECT(lj_oauth2_introspection_response_parse_json(
             test_default_runtime(), introspection_json,
             strlen(introspection_json), 0u, &introspection,
             &error) == LJ_STATUS_OK);
  EXPECT(introspection.aud != NULL);
  EXPECT(g_alloc_record_count > 0u);
  lonejson_oauth2_introspection_response_cleanup(&introspection);
  EXPECT(introspection.aud == NULL);
  EXPECT(g_alloc_record_count == 0u);
  reset_lonejson_alloc_stats();

  lonejson_oidc_userinfo_response_init(&userinfo);
  EXPECT(lj_oidc_userinfo_response_parse_json(
             test_default_runtime(), userinfo_json, strlen(userinfo_json), 0u,
             &userinfo, &error) == LJ_STATUS_OK);
  EXPECT(strcmp(userinfo.sub, "sub") == 0);
  EXPECT(strcmp(userinfo.name, "User") == 0);
  EXPECT(strcmp(userinfo.preferred_username, "user") == 0);
  EXPECT(strcmp(userinfo.email, "user@example.com") == 0);
  EXPECT(userinfo.has_email_verified);
  EXPECT(!userinfo.email_verified);
  EXPECT(userinfo.json != NULL);
  EXPECT(userinfo.len == strlen(userinfo_json));
  EXPECT(strstr(userinfo.json, "\"extra\":{\"ok\":true}") != NULL);
  lonejson_oidc_userinfo_response_cleanup(&userinfo);
}

static void test_oauth2_token_flow_helpers(void) {
  lonejson *runtime;
  lonejson_error error;
  lonejson_http_provider provider;
  test_oidc_http_provider_state state;
  lonejson_oauth2_token_response response;
  lonejson_oauth2_token_flow flow;
  lonejson_oauth2_token_flow source;
  lonejson_oauth2_token_flow_policy policy;
  lonejson_oauth2_token_flow_result result;
  int before_requests;

  lonejson_error_init(&error);
  memset(&provider, 0, sizeof(provider));
  memset(&state, 0, sizeof(state));
  memset(&policy, 0, sizeof(policy));
  runtime = lonejson_new(NULL, &error);
  EXPECT(runtime != NULL);
  EXPECT(runtime->oauth2_token_flow_ensure != NULL);
  EXPECT(lonejson_http_provider_init_simple(
             &provider, &state, "lonejson-test/1",
             test_oidc_http_provider_request, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_set_http_provider(runtime, &provider, &error) ==
         LONEJSON_STATUS_OK);

  lonejson_oauth2_token_flow_init(&flow);
  lonejson_oauth2_token_response_init(&response);
  EXPECT(lonejson_oauth2_token_response_parse_json(
             runtime,
             "{\"access_token\":\"initial\",\"token_type\":\"Bearer\","
             "\"refresh_token\":\"refresh token\",\"scope\":\"read\","
             "\"expires_in\":120}",
             strlen("{\"access_token\":\"initial\",\"token_type\":\"Bearer\","
                    "\"refresh_token\":\"refresh token\",\"scope\":\"read\","
                    "\"expires_in\":120}"),
             4096u, &response, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_oauth2_token_flow_update_response(
             &flow, &response, -1, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(flow.access_token == NULL);
  EXPECT(lonejson_oauth2_token_flow_update_response(
             &flow, &response, 1000, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(flow.access_token, "initial") == 0);
  EXPECT(strcmp(flow.refresh_token, "refresh token") == 0);
  EXPECT(flow.has_expires_at && flow.expires_at == 1120);
  lonejson_oauth2_token_response_cleanup(&response);

  memset(&source, 0, sizeof(source));
  source.access_token = (char *)"initial";
  source.token_type = (char *)"Bearer";
  source.refresh_token = (char *)"refresh token";
  source.scope = (char *)"read";
  source.id_token = (char *)"id token";
  source.expires_at = 1120;
  source.has_expires_at = 1;
  EXPECT(lonejson_oauth2_token_flow_assign(&flow, &source, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(flow.access_token, "initial") == 0);
  EXPECT(strcmp(flow.token_type, "Bearer") == 0);
  EXPECT(strcmp(flow.refresh_token, "refresh token") == 0);
  EXPECT(strcmp(flow.scope, "read") == 0);
  EXPECT(strcmp(flow.id_token, "id token") == 0);
  EXPECT(flow.has_expires_at && flow.expires_at == 1120);
  EXPECT(flow.access_token != source.access_token);
  EXPECT(flow.refresh_token != source.refresh_token);
  EXPECT(lj_oauth2_token_flow_assign(NULL, &source, &error) ==
         LJ_STATUS_INVALID_ARGUMENT);
  EXPECT(lj_oauth2_token_flow_assign(&flow, NULL, &error) ==
         LJ_STATUS_INVALID_ARGUMENT);

  EXPECT(!lonejson_oauth2_token_flow_is_expired(&flow, 1001, 10));
  before_requests = state.requests;
  policy.now = 1001;
  EXPECT(runtime->oauth2_token_flow_ensure(runtime, &flow, &policy, &result,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(result.state == LONEJSON_OAUTH2_TOKEN_FLOW_READY);
  EXPECT(result.attempts == 0u);
  EXPECT(state.requests == before_requests);

  policy.now = 1120;
  policy.disable_refresh = 1;
  EXPECT(lonejson_oauth2_token_flow_ensure(runtime, &flow, &policy, &result,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(result.state == LONEJSON_OAUTH2_TOKEN_FLOW_NEEDS_INTERACTION);
  policy.disable_refresh = 0;
  policy.token_endpoint = "https://issuer.example/token";
  policy.client_id = "client";
  policy.client_secret = "secret";
  state.token_transient_failures = 1;
  before_requests = state.requests;
  EXPECT(lonejson_oauth2_token_flow_ensure(runtime, &flow, &policy, &result,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(result.state == LONEJSON_OAUTH2_TOKEN_FLOW_REFRESHED);
  EXPECT(result.refreshed);
  EXPECT(result.attempts == 2u);
  EXPECT(state.requests == before_requests + 2);
  EXPECT(strcmp(flow.access_token, "abc.def.sig") == 0);
  EXPECT(strcmp(flow.refresh_token, "refresh token") == 0);
  EXPECT(flow.has_expires_at && flow.expires_at == 1180);

  policy.now = 1180;
  policy.max_retries = 0u;
  policy.disable_retry = 1;
  state.token_transient_failures = 1;
  EXPECT(lonejson_oauth2_token_flow_ensure(runtime, &flow, &policy, &result,
                                           &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(result.attempts == 1u);
  policy.disable_retry = 0;

  policy.now = 1180;
  policy.token_endpoint = NULL;
  EXPECT(lonejson_oauth2_token_flow_ensure(runtime, &flow, &policy, &result,
                                           &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(result.state == LONEJSON_OAUTH2_TOKEN_FLOW_FAILED);
  EXPECT(result.attempts == 0u);
  policy.token_endpoint = "https://issuer.example/token";
  policy.client_id = NULL;
  EXPECT(lonejson_oauth2_token_flow_ensure(runtime, &flow, &policy, &result,
                                           &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(result.state == LONEJSON_OAUTH2_TOKEN_FLOW_FAILED);
  policy.client_id = "client";

  lonejson_oauth2_token_flow_cleanup(&flow);
  EXPECT(lonejson_oauth2_token_flow_ensure(runtime, &flow, &policy, &result,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(result.state == LONEJSON_OAUTH2_TOKEN_FLOW_NEEDS_INTERACTION);

  lonejson_oauth2_token_flow_cleanup(&flow);
  lonejson_free(runtime);
}

static void test_oauth2_token_response_failures(void) {
  lonejson_oauth2_token_response response;
  lonejson_oauth2_introspection_response introspection;
  lonejson_oidc_userinfo_response userinfo;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oauth2_token_response_init(&response);
  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(), "{\"error\":\"invalid_client\"}",
             strlen("{\"error\":\"invalid_client\"}"), 0u, &response,
             &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(response.error == NULL);

  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(), "{\"access_token\":\"token\"}",
             strlen("{\"access_token\":\"token\"}"), 0u, &response,
             &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(response.access_token == NULL);

  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(),
             "{\"access_token\":\"token\",\"token_type\":\"mac\"}",
             strlen("{\"access_token\":\"token\",\"token_type\":\"mac\"}"), 0u,
             &response, &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(response.access_token == NULL);

  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(),
             "{\"access_token\":\"token\",\"token_type\":\"Bearer\","
             "\"expires_in\":-1}",
             strlen("{\"access_token\":\"token\",\"token_type\":\"Bearer\","
                    "\"expires_in\":-1}"),
             0u, &response, &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(response.access_token == NULL);

  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(),
             "{\"access_token\":\"token\",\"token_type\":\"Bearer\"}",
             strlen("{\"access_token\":\"token\",\"token_type\":\"Bearer\"}"),
             8u, &response, &error) == LONEJSON_STATUS_OVERFLOW);
  lonejson_oauth2_token_response_cleanup(&response);

  lonejson_oauth2_introspection_response_init(&introspection);
  EXPECT(test_default_runtime()->oauth2_introspection_response_parse_json(
             test_default_runtime(), "{\"scope\":\"read\"}",
             strlen("{\"scope\":\"read\"}"), 0u, &introspection,
             &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(introspection.scope == NULL);
  EXPECT(test_default_runtime()->oauth2_introspection_response_parse_json(
             test_default_runtime(), "{\"active\":\"true\"}",
             strlen("{\"active\":\"true\"}"), 0u, &introspection,
             &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(test_default_runtime()->oauth2_introspection_response_parse_json(
             test_default_runtime(), "{\"active\":true}",
             strlen("{\"active\":true}"), 8u, &introspection,
             &error) == LONEJSON_STATUS_OVERFLOW);
  EXPECT(test_default_runtime()->oauth2_introspection_response_parse_json(
             test_default_runtime(), "{\"active\":true,\"aud\":[\"api\"]}",
             strlen("{\"active\":true,\"aud\":[\"api\"]}"), 0u, &introspection,
             &error) == LONEJSON_STATUS_OK);
  EXPECT(introspection.has_active);
  EXPECT(introspection.active);
  EXPECT(introspection.aud == NULL);
  lonejson_oauth2_introspection_response_cleanup(&introspection);

  lonejson_oidc_userinfo_response_init(&userinfo);
  EXPECT(test_default_runtime()->oidc_userinfo_response_parse_json(
             test_default_runtime(), "{\"sub\":\"sub\"}",
             strlen("{\"sub\":\"sub\"}"), 8u, &userinfo,
             &error) == LONEJSON_STATUS_OVERFLOW);
  EXPECT(userinfo.json == NULL);
  EXPECT(test_default_runtime()->oidc_userinfo_response_parse_json(
             test_default_runtime(), "{\"email_verified\":\"yes\"}",
             strlen("{\"email_verified\":\"yes\"}"), 0u, &userinfo,
             &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(userinfo.json == NULL);
  EXPECT(test_default_runtime()->oidc_userinfo_response_parse_json(
             test_default_runtime(),
             "{\"sub\":\"subject\",\"email_verified\":true}",
             strlen("{\"sub\":\"subject\",\"email_verified\":true}"), 0u,
             &userinfo, &error) == LONEJSON_STATUS_OK);
  EXPECT(userinfo.sub != NULL);
  if (userinfo.sub != NULL) {
    EXPECT(strcmp(userinfo.sub, "subject") == 0);
  }
  EXPECT(userinfo.has_email_verified && userinfo.email_verified);
  lonejson_oidc_userinfo_response_cleanup(&userinfo);
}

static lonejson_status test_auth_provider_random_bytes(void *user,
                                                       unsigned char *dst,
                                                       size_t len,
                                                       lonejson_error *error) {
  size_t i;
  (void)user;
  (void)error;
  for (i = 0u; i < len; ++i) {
    dst[i] = (unsigned char)i;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
test_auth_provider_pkce_sha256(void *user, const void *data, size_t len,
                               unsigned char out[32], lonejson_error *error) {
  static const char verifier[] = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
  size_t i;
  (void)error;
  if (data != NULL && len == sizeof(verifier) - 1u &&
      memcmp(data, verifier, sizeof(verifier) - 1u) == 0 && user != NULL) {
    memcpy(out, user, 32u);
    return LONEJSON_STATUS_OK;
  }
  for (i = 0u; i < 32u; ++i) {
    out[i] = (unsigned char)i;
  }
  return LONEJSON_STATUS_OK;
}

static void test_oidc_pkce_challenge_and_generate(void) {
  static const char verifier[] = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
  static const unsigned char verifier_digest[32] = {
      0x13u, 0xd3u, 0x1eu, 0x96u, 0x1au, 0x1au, 0xd8u, 0xecu,
      0x2fu, 0x16u, 0xb1u, 0x0cu, 0x4cu, 0x98u, 0x2eu, 0x08u,
      0x76u, 0xa8u, 0x78u, 0xadu, 0x6du, 0xf1u, 0x44u, 0x56u,
      0x6eu, 0xe1u, 0x89u, 0x4au, 0xcbu, 0x70u, 0xf9u, 0xc3u};
  lonejson_owned_buffer challenge;
  lonejson_oidc_pkce pkce;
  lonejson_auth_provider provider;
  lonejson_auth_provider sha_only_provider;
  lonejson_auth_provider random_only_provider;
  lonejson_config config;
  lonejson *runtime;
  lonejson_error error;
  lonejson_error_init(&error);
  lonejson_owned_buffer_init(&challenge);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_oidc_pkce_challenge(verifier, &challenge, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  lonejson_oidc_pkce_init(&pkce);
  EXPECT(lj_oidc_pkce_generate(0u, &pkce, &error) == LJ_STATUS_TYPE_MISMATCH);
  lonejson_oidc_pkce_cleanup(&pkce);
#else
  EXPECT(lonejson_oidc_pkce_challenge(verifier, &challenge, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_oidc_pkce_init(&pkce);
  EXPECT(lj_oidc_pkce_generate(0u, &pkce, &error) == LJ_STATUS_TYPE_MISMATCH);
#endif

  memset(&provider, 0, sizeof(provider));
  provider.user_data = (void *)verifier_digest;
  provider.random_bytes = test_auth_provider_random_bytes;
  provider.sha256 = test_auth_provider_pkce_sha256;
  config = lonejson_default_config();
  config.auth_provider = &provider;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  EXPECT(runtime->oidc_pkce_challenge_with_runtime != NULL);
  EXPECT(runtime->oidc_pkce_generate_with_runtime != NULL);
  EXPECT(lonejson_oidc_pkce_challenge_with_runtime(
             runtime, verifier, &challenge, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(challenge.data,
                "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM") == 0);
  lonejson_owned_buffer_free(&challenge);
  lonejson_oidc_pkce_init(&pkce);
  EXPECT(lj_oidc_pkce_generate_with_runtime(runtime, 32u, &pkce, &error) ==
         LJ_STATUS_OK);
  EXPECT(strcmp(pkce.code_verifier,
                "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8") == 0);
  EXPECT(strcmp(pkce.code_challenge,
                "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8") == 0);
  lonejson_oidc_pkce_cleanup(&pkce);
  lonejson_free(runtime);

  memset(&sha_only_provider, 0, sizeof(sha_only_provider));
  sha_only_provider.sha256 = test_auth_provider_pkce_sha256;
  config = lonejson_default_config();
  config.auth_provider = &sha_only_provider;
  runtime = lonejson_new(&config, &error);
  EXPECT(lonejson_oidc_pkce_generate_with_runtime(
             runtime, 32u, &pkce, &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_free(runtime);

  memset(&random_only_provider, 0, sizeof(random_only_provider));
  random_only_provider.random_bytes = test_auth_provider_random_bytes;
  config = lonejson_default_config();
  config.auth_provider = &random_only_provider;
  runtime = lonejson_new(&config, &error);
  EXPECT(lonejson_oidc_pkce_challenge_with_runtime(runtime, verifier,
                                                   &challenge, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_free(runtime);

  EXPECT(lonejson_oidc_pkce_challenge("too-short", &challenge, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_pkce_generate(31u, &pkce, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
}

static void test_oidc_authorization_url(void) {
  lonejson_oidc_authorization_request request;
  lonejson_owned_buffer url;
  lonejson_error error;

  memset(&request, 0, sizeof(request));
  lonejson_owned_buffer_init(&url);
  lonejson_error_init(&error);
  request.authorization_endpoint = "https://id.example/auth";
  request.client_id = "client id";
  request.redirect_uri = "http://127.0.0.1:1234/cb";
  request.scope = "openid profile";
  request.state = "state-123";
  request.nonce = "nonce-456";
  request.code_challenge = "challenge";
  request.audience = "https://api.example";
  EXPECT(lonejson_oidc_authorization_url(&request, &url, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(url.data,
                "https://id.example/auth?response_type=code&"
                "client_id=client+id&"
                "redirect_uri=http%3A%2F%2F127.0.0.1%3A1234%2Fcb&"
                "scope=openid+profile&state=state-123&nonce=nonce-456&"
                "code_challenge=challenge&code_challenge_method=S256&"
                "audience=https%3A%2F%2Fapi.example") == 0);
  lonejson_owned_buffer_free(&url);

  request.max_url_bytes = 16u;
  EXPECT(lonejson_oidc_authorization_url(&request, &url, &error) ==
         LONEJSON_STATUS_OVERFLOW);
  EXPECT(url.data == NULL);
  request.max_url_bytes = 0u;
  request.authorization_endpoint = "http://id.example/auth";
  EXPECT(lonejson_oidc_authorization_url(&request, &url, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  request.authorization_endpoint = "https://id.example/auth#fragment";
  EXPECT(lonejson_oidc_authorization_url(&request, &url, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(url.data == NULL);
}

static void test_oidc_authorization_callback_parse(void) {
  lonejson_oidc_authorization_callback callback;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oidc_authorization_callback_init(&callback);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "?code=abc%2B123&state=state+123",
             strlen("?code=abc%2B123&state=state+123"), "state 123", 0u,
             &callback, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(callback.code, "abc+123") == 0);
  EXPECT(strcmp(callback.state, "state 123") == 0);
  lonejson_oidc_authorization_callback_cleanup(&callback);

  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "code=abc&state=wrong", strlen("code=abc&state=wrong"), "state",
             0u, &callback, &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(callback.code == NULL);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "error=access_denied&state=state",
             strlen("error=access_denied&state=state"), "state", 0u, &callback,
             &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(callback.error == NULL);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "error=access_denied&error=server_error&state=state",
             strlen("error=access_denied&error=server_error&state=state"),
             "state", 0u, &callback,
             &error) == LONEJSON_STATUS_DUPLICATE_FIELD);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "error=access_denied&error_uri=https%3A%2F%2Fid.example%2Fe&"
             "error_uri=https%3A%2F%2Fid.example%2Ff&state=state",
             strlen("error=access_denied&error_uri=https%3A%2F%2Fid.example%"
                    "2Fe&error_uri=https%3A%2F%2Fid.example%2Ff&state=state"),
             "state", 0u, &callback,
             &error) == LONEJSON_STATUS_DUPLICATE_FIELD);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "code=abc&code=def&state=state",
             strlen("code=abc&code=def&state=state"), "state", 0u, &callback,
             &error) == LONEJSON_STATUS_DUPLICATE_FIELD);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "code=abc&state=%zz", strlen("code=abc&state=%zz"), "state", 0u,
             &callback, &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "code=abc&state=state%00extra",
             strlen("code=abc&state=state%00extra"), "state", 0u, &callback,
             &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(callback.code == NULL);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "code=abc%00extra&state=state",
             strlen("code=abc%00extra&state=state"), "state", 0u, &callback,
             &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(callback.code == NULL);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "code%00extra=abc&state=state",
             strlen("code%00extra=abc&state=state"), "state", 0u, &callback,
             &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(callback.code == NULL);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "error=access_denied&error_description=bad%00extra&state=state",
             strlen("error=access_denied&error_description=bad%00extra&state="
                    "state"),
             "state", 0u, &callback, &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "code=abc&state=state", strlen("code=abc&state=state"), "state",
             8u, &callback, &error) == LONEJSON_STATUS_OVERFLOW);
  lonejson_oidc_authorization_callback_cleanup(&callback);
}

static void test_oidc_authorization_bearer_token(void) {
  lonejson_jwt_segment token;
  lonejson_error error;

  lonejson_error_init(&error);
  EXPECT(lonejson_oidc_authorization_bearer_token("  bEaReR \t abc.def.sig  ",
                                                  &token, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(token.len == strlen("abc.def.sig"));
  EXPECT(memcmp(token.data, "abc.def.sig", token.len) == 0);
  EXPECT(lj_oidc_authorization_bearer_token("Bearer abc", &token, &error) ==
         LJ_STATUS_OK);
  EXPECT(token.len == 3u);
  EXPECT(lonejson_oidc_authorization_bearer_token(NULL, &token, &error) ==
         LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(lonejson_oidc_authorization_bearer_token(
             "Basic abc", &token, &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_authorization_bearer_token("Bearer ", &token, &error) ==
         LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(lonejson_oidc_authorization_bearer_token(
             "Bearer abc def", &token, &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_authorization_bearer_token("Bearer abc", NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
}

static void test_oidc_validate_bearer_token(void) {
  char header[1024];
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy cache_policy;
  lonejson_jwt_claim_policy claim_policy;
  lonejson_oidc_bearer_validation_request request;
  lonejson_oidc_bearer_validation validation;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oidc_jwks_cache_init(&cache);
  lonejson_oidc_bearer_validation_init(&validation);
  cache_policy = test_oidc_jwks_cache_policy();
  claim_policy = test_jwt_policy();
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &cache_policy,
             test_jwt_rs256_jwks_json, strlen(test_jwt_rs256_jwks_json),
             &error) == LONEJSON_STATUS_OK);
  snprintf(header, sizeof(header), "Bearer %s", test_jwt_rs256_token);
  memset(&request, 0, sizeof(request));
  request.authorization_header = header;
  request.jwks_cache = &cache;
  request.jwks_policy = &cache_policy;
  request.claim_policy = &claim_policy;
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation,
                                             &error) == LONEJSON_STATUS_OK);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_NONE);
  EXPECT(strcmp(lonejson_auth_failure_string(validation.failure), "none") == 0);
  EXPECT(validation.jwk != NULL);
  EXPECT(strcmp(validation.header.kid, "rsa-test") == 0);
  EXPECT(strcmp(validation.claims.iss, "issuer") == 0);
  lonejson_oidc_bearer_validation_cleanup(&validation);
  lonejson_oidc_bearer_validation_init(&validation);
  EXPECT(test_default_runtime()->oidc_validate_bearer_token != NULL);
  EXPECT(test_default_runtime()->oidc_validate_bearer_token(
             test_default_runtime(), &request, &validation, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_NONE);
  EXPECT(lj_oidc_validate_bearer_token(test_default_runtime(), &request,
                                       &validation, &error) == LJ_STATUS_OK);
#else
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);
#endif
  lonejson_oidc_bearer_validation_cleanup(&validation);
  lonejson_oidc_jwks_cache_cleanup(&cache);

  lonejson_oidc_jwks_cache_init(&cache);
  lonejson_oidc_bearer_validation_init(&validation);
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &cache_policy,
             test_jwt_rs256_same_kid_wrong_alg_first_jwks_json,
             strlen(test_jwt_rs256_same_kid_wrong_alg_first_jwks_json),
             &error) == LONEJSON_STATUS_OK);
  request.jwks_cache = &cache;
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation,
                                             &error) == LONEJSON_STATUS_OK);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_NONE);
  EXPECT(validation.jwk != NULL);
  EXPECT(strcmp(validation.jwk->alg, "RS256") == 0);
#else
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);
#endif
  lonejson_oidc_bearer_validation_cleanup(&validation);
  lonejson_oidc_jwks_cache_cleanup(&cache);

  lonejson_oidc_jwks_cache_init(&cache);
  lonejson_oidc_bearer_validation_init(&validation);
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &cache_policy,
             test_jwt_rs256_minimal_jwks_json,
             strlen(test_jwt_rs256_minimal_jwks_json),
             &error) == LONEJSON_STATUS_OK);
  request.jwks_cache = &cache;
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation,
                                             &error) == LONEJSON_STATUS_OK);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_NONE);
  EXPECT(validation.jwk != NULL);
  EXPECT(validation.jwk->alg == NULL);
  EXPECT(validation.jwk->use == NULL);
#else
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);
#endif
  lj_oidc_bearer_validation_cleanup(&validation);
  lonejson_oidc_jwks_cache_cleanup(&cache);
}

static void test_oidc_validate_bearer_token_failures(void) {
  char header[1024];
  char tampered[sizeof(test_jwt_rs256_token)];
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy cache_policy;
  lonejson_jwt_claim_policy claim_policy;
  lonejson_oidc_bearer_validation_request request;
  lonejson_oidc_bearer_validation validation;
  lonejson_error error;
  const char *bad_issuer = "other";
  const char *bad_audience = "other";

  lonejson_error_init(&error);
  lonejson_oidc_jwks_cache_init(&cache);
  lonejson_oidc_bearer_validation_init(&validation);
  cache_policy = test_oidc_jwks_cache_policy();
  claim_policy = test_jwt_policy();
  memset(&request, 0, sizeof(request));
  request.jwks_cache = &cache;
  request.jwks_policy = &cache_policy;
  request.claim_policy = &claim_policy;

  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &cache_policy,
             test_jwt_rs256_jwks_json, strlen(test_jwt_rs256_jwks_json),
             &error) == LONEJSON_STATUS_OK);

  request.authorization_header = NULL;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_MISSING_CREDENTIALS);

  request.authorization_header = "Bearer not-a-jwt";
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_MALFORMED_TOKEN);

  snprintf(header, sizeof(header), "Bearer %s", test_jwt_rs256_token);
  request.authorization_header = header;
  cache_policy.now = 2000;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_CACHE_UNAVAILABLE);
  cache_policy = test_oidc_jwks_cache_policy();
  request.jwks_policy = &cache_policy;

  lonejson_oidc_jwks_cache_cleanup(&cache);
  lonejson_oidc_jwks_cache_init(&cache);
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &cache_policy,
             test_jwt_rs256_other_kid_jwks_json,
             strlen(test_jwt_rs256_other_kid_jwks_json),
             &error) == LONEJSON_STATUS_OK);
  request.jwks_cache = &cache;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_KEY_NOT_FOUND);

  lonejson_oidc_jwks_cache_cleanup(&cache);
  lonejson_oidc_jwks_cache_init(&cache);
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &cache_policy,
             test_jwt_rs256_jwks_json, strlen(test_jwt_rs256_jwks_json),
             &error) == LONEJSON_STATUS_OK);
  request.jwks_cache = &cache;
  memcpy(tampered, test_jwt_rs256_token, sizeof(tampered));
  tampered[sizeof(tampered) - 2u] =
      tampered[sizeof(tampered) - 2u] == 'A' ? 'B' : 'A';
  snprintf(header, sizeof(header), "Bearer %s", tampered);
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);

  snprintf(header, sizeof(header), "Bearer %s", test_jwt_rs256_token);
  claim_policy = test_jwt_policy();
  claim_policy.now = 3000;
  request.claim_policy = &claim_policy;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_EXPIRED_TOKEN);
#else
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);
#endif

  claim_policy = test_jwt_policy();
  claim_policy.accepted_issuers = &bad_issuer;
  request.claim_policy = &claim_policy;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_ISSUER_MISMATCH);
#else
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);
#endif

  claim_policy = test_jwt_policy();
  claim_policy.accepted_audiences = &bad_audience;
  request.claim_policy = &claim_policy;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_AUDIENCE_MISMATCH);
#else
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);
#endif

  lonejson_oidc_bearer_validation_cleanup(&validation);
  lonejson_oidc_jwks_cache_cleanup(&cache);
}

#ifdef LONEJSON_WITH_OPENSSL
static void test_m2m_basic_base64(char *out, size_t out_size, const char *id,
                                  const char *secret) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char raw[512];
  size_t raw_len;
  size_t i = 0u;
  size_t o = 0u;
  int n;

  n = snprintf(raw, sizeof(raw), "%s:%s", id, secret);
  EXPECT(n > 0 && (size_t)n < sizeof(raw));
  raw_len = (size_t)n;
  while (i < raw_len) {
    size_t rem = raw_len - i;
    unsigned int b0 = (unsigned char)raw[i++];
    unsigned int b1 = rem > 1u ? (unsigned char)raw[i++] : 0u;
    unsigned int b2 = rem > 2u ? (unsigned char)raw[i++] : 0u;
    unsigned int bits = (b0 << 16u) | (b1 << 8u) | b2;
    EXPECT(o + 4u < out_size);
    out[o++] = alphabet[(bits >> 18u) & 63u];
    out[o++] = alphabet[(bits >> 12u) & 63u];
    out[o++] = rem > 1u ? alphabet[(bits >> 6u) & 63u] : '=';
    out[o++] = rem > 2u ? alphabet[bits & 63u] : '=';
  }
  out[o] = '\0';
}

static void
test_m2m_revoked_store_json(lonejson_owned_buffer *out,
                            const lonejson_m2m_credential *credential,
                            lonejson_error *error) {
  static const char suffix[] = ",\"revoked\":true}]}";
  EXPECT(lonejson_owned_buffer_sink(out, "{\"credentials\":[", 16u, error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_owned_buffer_sink(out, credential->record_json.data,
                                    credential->record_json.len - 1u,
                                    error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_owned_buffer_sink(out, suffix, sizeof(suffix) - 1u, error) ==
         LONEJSON_STATUS_OK);
}
#endif

static void test_m2m_credential_store_auth(void) {
#ifdef LONEJSON_WITH_OPENSSL
  lonejson_error error;
  lonejson_m2m_credential_request request;
  lonejson_m2m_credential credential;
  lonejson_m2m_credential bad_credential;
  lonejson_m2m_store store;
  lonejson_m2m_verify_request verify;
  lonejson_m2m_authentication auth;
  lonejson_owned_buffer store_json;
  lonejson_owned_buffer revoked_store_json;
  char basic_payload[768];
  char header[1024];
  size_t fail_after;
  lonejson_status alloc_status;

  lonejson_error_init(&error);
  memset(&request, 0, sizeof(request));
  lonejson_m2m_credential_init(&credential);
  lonejson_m2m_credential_init(&bad_credential);
  memset(&store, 0, sizeof(store));
  memset(&verify, 0, sizeof(verify));
  lonejson_m2m_authentication_init(&auth);
  lonejson_owned_buffer_init(&store_json);
  lonejson_owned_buffer_init(&revoked_store_json);

  request.claim_json = "{\"scope\":[\"read\",\"write\"],\"tenant\":\"acme\"}";
  EXPECT(lonejson_m2m_credential_generate(test_default_runtime(), &request,
                                          &credential,
                                          &error) == LONEJSON_STATUS_OK);
  EXPECT(credential.client_id != NULL);
  EXPECT(credential.client_secret != NULL);
  EXPECT(credential.api_key != NULL);
  EXPECT(strstr(credential.record_json.data, credential.client_secret) == NULL);
  EXPECT(strstr(credential.record_json.data, credential.api_key) == NULL);

  request.claim_json = "{\"scope\":[\"read\"";
  EXPECT(lonejson_m2m_credential_generate(test_default_runtime(), &request,
                                          &bad_credential, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  request.claim_json = "{\"scope\":[\"read\",\"write\"],\"tenant\":\"acme\"}";

  EXPECT(lonejson_owned_buffer_sink(&store_json, "{\"credentials\":[", 16u,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_owned_buffer_sink(&store_json, credential.record_json.data,
                                    credential.record_json.len,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_owned_buffer_sink(&store_json, "]}", 2u, &error) ==
         LONEJSON_STATUS_OK);
  store.json = store_json.data;
  store.len = store_json.len;
  verify.store = &store;

  verify.authorization_header = NULL;
  EXPECT(lonejson_m2m_verify_authorization(test_default_runtime(), &verify,
                                           &auth, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(auth.failure == LONEJSON_AUTH_FAILURE_MISSING_CREDENTIALS);

  lonejson_m2m_authentication_cleanup(&auth);
  lonejson_m2m_authentication_init(&auth);
  verify.authorization_header = "Bearer";
  EXPECT(lonejson_m2m_verify_authorization(test_default_runtime(), &verify,
                                           &auth, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(auth.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);

  lonejson_m2m_authentication_cleanup(&auth);
  lonejson_m2m_authentication_init(&auth);
  verify.authorization_header = "Basic !!!";
  EXPECT(lonejson_m2m_verify_authorization(test_default_runtime(), &verify,
                                           &auth, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(auth.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);

  lonejson_m2m_authentication_cleanup(&auth);
  lonejson_m2m_authentication_init(&auth);
  store.json = "{\"credentials\":{}}";
  store.len = strlen(store.json);
  EXPECT(lonejson_m2m_verify_authorization(test_default_runtime(), &verify,
                                           &auth, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(auth.failure == LONEJSON_AUTH_FAILURE_CACHE_UNAVAILABLE);
  store.json = store_json.data;
  store.len = store_json.len;

  test_m2m_basic_base64(basic_payload, sizeof(basic_payload),
                        credential.client_id, credential.client_secret);
  snprintf(header, sizeof(header), "Basic %s", basic_payload);
  verify.authorization_header = header;
  EXPECT(test_default_runtime()->m2m_verify_authorization != NULL);
  EXPECT(test_default_runtime()->m2m_verify_authorization(
             test_default_runtime(), &verify, &auth, &error) ==
         LONEJSON_STATUS_OK);
  if (auth.failure == LONEJSON_AUTH_FAILURE_NONE) {
    EXPECT(auth.auth_mode == LONEJSON_M2M_AUTH_BASIC);
    EXPECT(strcmp(auth.client_id, credential.client_id) == 0);
    EXPECT(auth.claim.kind == LONEJSON_JSON_VALUE_BUFFER);
    EXPECT(strstr(auth.claim.json, "\"tenant\":\"acme\"") != NULL);
  }

  lonejson_m2m_authentication_cleanup(&auth);
  lonejson_m2m_authentication_init(&auth);
  snprintf(header, sizeof(header), "Bearer %s", credential.api_key);
  verify.authorization_header = header;
  EXPECT(lonejson_m2m_verify_authorization(test_default_runtime(), &verify,
                                           &auth,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(auth.auth_mode == LONEJSON_M2M_AUTH_BEARER);

  for (fail_after = 0u; fail_after < 256u; ++fail_after) {
    lonejson_m2m_authentication_cleanup(&auth);
    lonejson_m2m_authentication_init(&auth);
    test_m2m_basic_base64(basic_payload, sizeof(basic_payload),
                          credential.client_id, credential.client_secret);
    snprintf(header, sizeof(header), "Basic %s", basic_payload);
    verify.authorization_header = header;
    test_lonejson_fail_alloc_after(fail_after);
    alloc_status = lonejson_m2m_verify_authorization(test_default_runtime(),
                                                     &verify, &auth, &error);
    test_lonejson_fail_alloc_clear();
    if (alloc_status == LONEJSON_STATUS_OK) {
      EXPECT(auth.failure == LONEJSON_AUTH_FAILURE_NONE);
      EXPECT(auth.client_id != NULL);
      EXPECT(auth.claim.kind == LONEJSON_JSON_VALUE_BUFFER);
    } else if (alloc_status == LONEJSON_STATUS_ALLOCATION_FAILED) {
      EXPECT(auth.failure != LONEJSON_AUTH_FAILURE_NONE);
    }

    lonejson_m2m_authentication_cleanup(&auth);
    lonejson_m2m_authentication_init(&auth);
    snprintf(header, sizeof(header), "Bearer %s", credential.api_key);
    verify.authorization_header = header;
    test_lonejson_fail_alloc_after(fail_after);
    alloc_status = lonejson_m2m_verify_authorization(test_default_runtime(),
                                                     &verify, &auth, &error);
    test_lonejson_fail_alloc_clear();
    if (alloc_status == LONEJSON_STATUS_OK) {
      EXPECT(auth.failure == LONEJSON_AUTH_FAILURE_NONE);
      EXPECT(auth.client_id != NULL);
      EXPECT(auth.claim.kind == LONEJSON_JSON_VALUE_BUFFER);
    } else if (alloc_status == LONEJSON_STATUS_ALLOCATION_FAILED) {
      EXPECT(auth.failure != LONEJSON_AUTH_FAILURE_NONE);
    }
  }

  lonejson_m2m_authentication_cleanup(&auth);
  lonejson_m2m_authentication_init(&auth);
  snprintf(header, sizeof(header), "Bearer %s \t", credential.api_key);
  verify.authorization_header = header;
  EXPECT(lonejson_m2m_verify_authorization(test_default_runtime(), &verify,
                                           &auth,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(auth.auth_mode == LONEJSON_M2M_AUTH_BEARER);

  lonejson_m2m_authentication_cleanup(&auth);
  lonejson_m2m_authentication_init(&auth);
  verify.allowed_auth_modes = LONEJSON_M2M_AUTH_BASIC;
  EXPECT(lonejson_m2m_verify_authorization(test_default_runtime(), &verify,
                                           &auth, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(auth.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);

  lonejson_m2m_authentication_cleanup(&auth);
  lonejson_m2m_authentication_init(&auth);
  test_m2m_basic_base64(basic_payload, sizeof(basic_payload),
                        credential.client_id, "wrong-secret");
  snprintf(header, sizeof(header), "Basic %s", basic_payload);
  verify.allowed_auth_modes = LONEJSON_M2M_AUTH_DEFAULT;
  verify.authorization_header = header;
  EXPECT(lonejson_m2m_verify_authorization(test_default_runtime(), &verify,
                                           &auth, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  lonejson_m2m_authentication_cleanup(&auth);
  lonejson_m2m_authentication_init(&auth);
  test_m2m_revoked_store_json(&revoked_store_json, &credential, &error);
  store.json = revoked_store_json.data;
  store.len = revoked_store_json.len;
  snprintf(header, sizeof(header), "Bearer %s", credential.api_key);
  verify.authorization_header = header;
  EXPECT(lonejson_m2m_verify_authorization(test_default_runtime(), &verify,
                                           &auth, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(auth.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);

  lonejson_m2m_authentication_cleanup(&auth);
  lonejson_m2m_credential_cleanup(&bad_credential);
  lonejson_m2m_credential_cleanup(&credential);
  lonejson_owned_buffer_free(&revoked_store_json);
  lonejson_owned_buffer_free(&store_json);
#endif
}

static void test_m2m_signup_flow(void) {
#ifdef LONEJSON_WITH_OPENSSL
  lonejson_error error;
  lonejson_m2m_signup_request signup_request;
  lonejson_m2m_signup signup;
  lonejson_m2m_signup bad_signup;
  lonejson_m2m_store store;
  lonejson_m2m_signup_complete_request complete_request;
  lonejson_m2m_signup_completion complete;
  lonejson_owned_buffer store_json;

  lonejson_error_init(&error);
  memset(&signup_request, 0, sizeof(signup_request));
  lonejson_m2m_signup_init(&signup);
  lonejson_m2m_signup_init(&bad_signup);
  memset(&store, 0, sizeof(store));
  memset(&complete_request, 0, sizeof(complete_request));
  lonejson_m2m_signup_complete_init(&complete);
  lonejson_owned_buffer_init(&store_json);

  signup_request.base_url = "https://app.example/signup";
  signup_request.claim_json = "{\"scope\":[\"read\"],\"plan\":\"trial\"}";
  EXPECT(lonejson_m2m_signup_generate(test_default_runtime(), &signup_request,
                                      &signup, &error) == LONEJSON_STATUS_OK);
  EXPECT(signup.signup_id != NULL);
  EXPECT(signup.signup_secret != NULL);
  EXPECT(strstr(signup.url.data, "signup_id=") != NULL);
  EXPECT(strstr(signup.url.data, "signup_secret=") != NULL);
  EXPECT(strstr(signup.record_json.data, signup.signup_secret) == NULL);

  lonejson_m2m_signup_cleanup(&bad_signup);
  signup_request.id_param = "signup id";
  signup_request.secret_param = "signup&secret=token";
  EXPECT(lonejson_m2m_signup_generate(test_default_runtime(), &signup_request,
                                      &bad_signup,
                                      &error) == LONEJSON_STATUS_OK);
  EXPECT(strstr(bad_signup.query.data, "signup+id=") != NULL);
  EXPECT(strstr(bad_signup.query.data, "signup%26secret%3Dtoken=") != NULL);
  EXPECT(strstr(bad_signup.query.data, "signup id=") == NULL);
  EXPECT(strstr(bad_signup.query.data, "signup&secret=token=") == NULL);
  EXPECT(strstr(bad_signup.url.data, "signup+id=") != NULL);
  EXPECT(strstr(bad_signup.url.data, "signup%26secret%3Dtoken=") != NULL);
  EXPECT(strstr(bad_signup.url.data, "&secret=token=") == NULL);
  lonejson_m2m_signup_cleanup(&bad_signup);
  signup_request.id_param = NULL;
  signup_request.secret_param = NULL;

  signup_request.max_url_bytes = 8u;
  EXPECT(lonejson_m2m_signup_generate(test_default_runtime(), &signup_request,
                                      &bad_signup,
                                      &error) == LONEJSON_STATUS_OVERFLOW);
  signup_request.max_url_bytes = 0u;
  signup_request.claim_json = "[";
  EXPECT(lonejson_m2m_signup_generate(test_default_runtime(), &signup_request,
                                      &bad_signup,
                                      &error) == LONEJSON_STATUS_INVALID_JSON);
  signup_request.claim_json = "{\"scope\":[\"read\"],\"plan\":\"trial\"}";

  EXPECT(lonejson_owned_buffer_sink(&store_json, "{\"signups\":[", 12u,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_owned_buffer_sink(&store_json, signup.record_json.data,
                                    signup.record_json.len,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_owned_buffer_sink(&store_json, "]}", 2u, &error) ==
         LONEJSON_STATUS_OK);
  store.json = store_json.data;
  store.len = store_json.len;
  complete_request.store = &store;
  complete_request.signup_id = signup.signup_id;
  complete_request.signup_secret = signup.signup_secret;
  complete_request.email = NULL;
  EXPECT(lonejson_m2m_signup_complete(test_default_runtime(), &complete_request,
                                      &complete, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  complete_request.email = "user@example.com";
  complete_request.credential_auth_modes = LONEJSON_M2M_AUTH_BEARER;
  EXPECT(test_default_runtime()->m2m_signup_complete != NULL);
  EXPECT(test_default_runtime()->m2m_signup_complete(
             test_default_runtime(), &complete_request, &complete, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(complete.signup_id, signup.signup_id) == 0);
  EXPECT(strcmp(complete.email, "user@example.com") == 0);
  EXPECT(complete.credential.api_key != NULL);
  EXPECT(complete.credential.client_secret == NULL);
  EXPECT(strstr(complete.credential.record_json.data, "\"plan\":\"trial\"") !=
         NULL);

  lonejson_m2m_signup_complete_cleanup(&complete);
  complete_request.signup_secret = "wrong-secret";
  EXPECT(lonejson_m2m_signup_complete(test_default_runtime(), &complete_request,
                                      &complete,
                                      &error) == LONEJSON_STATUS_TYPE_MISMATCH);

  lonejson_m2m_signup_complete_cleanup(&complete);
  lonejson_m2m_signup_cleanup(&bad_signup);
  lonejson_m2m_signup_cleanup(&signup);
  lonejson_owned_buffer_free(&store_json);
#endif
}
#else
static void test_oidc_discovery_url(void) {}
static void test_oidc_discovery_parse_and_validate(void) {}
static void test_oidc_discovery_failures(void) {}
static void test_oidc_jwks_cache_update_and_select(void) {}
static void test_oidc_jwks_cache_failure_modes(void) {}
static void test_oidc_jwks_cache_curl_adapter(void) {}
static void test_oidc_http_provider_helpers(void) {}
static void test_oauth2_client_credentials_body(void) {}
static void test_oauth2_token_response_parse(void) {}
static void test_oauth2_token_flow_helpers(void) {}
static void test_oauth2_token_response_failures(void) {}
static void test_oidc_pkce_challenge_and_generate(void) {}
static void test_oidc_authorization_url(void) {}
static void test_oidc_authorization_callback_parse(void) {}
static void test_oidc_authorization_bearer_token(void) {}
static void test_oidc_validate_bearer_token(void) {}
static void test_oidc_validate_bearer_token_failures(void) {}
static void test_m2m_credential_store_auth(void) {}
static void test_m2m_signup_flow(void) {}
#endif
#else
static void test_base64_url_raw_decode_vectors(void) {}
static void test_jwt_compact_parse_segments(void) {}
static void test_jwt_compact_parse_failures(void) {}
static void test_jwk_parse_json_shapes(void) {}
static void test_jwks_parse_and_select(void) {}
static void test_jwk_parse_failures(void) {}
static void test_jwt_decode_and_validate_claims(void) {}
static void test_jwt_validate_rs256_signature(void) {}
static void test_jwt_validate_x5c_signature_policy(void) {}
static void test_jwt_validate_recommended_signatures(void) {}
static void test_jwt_auth_provider_runtime_boundary(void) {}
static void test_jwt_validate_signature_failures(void) {}
static void test_jwt_claim_validation_failures(void) {}
static void test_jwt_decode_claim_failures(void) {}
static void test_oidc_discovery_url(void) {}
static void test_oidc_discovery_parse_and_validate(void) {}
static void test_oidc_discovery_failures(void) {}
static void test_oidc_jwks_cache_update_and_select(void) {}
static void test_oidc_jwks_cache_failure_modes(void) {}
static void test_oidc_jwks_cache_curl_adapter(void) {}
static void test_oidc_http_provider_helpers(void) {}
static void test_oauth2_client_credentials_body(void) {}
static void test_oauth2_token_response_parse(void) {}
static void test_oauth2_token_flow_helpers(void) {}
static void test_oauth2_token_response_failures(void) {}
static void test_oidc_pkce_challenge_and_generate(void) {}
static void test_oidc_authorization_url(void) {}
static void test_oidc_authorization_callback_parse(void) {}
static void test_oidc_authorization_bearer_token(void) {}
static void test_oidc_validate_bearer_token(void) {}
static void test_oidc_validate_bearer_token_failures(void) {}
static void test_m2m_credential_store_auth(void) {}
static void test_m2m_signup_flow(void) {}
#endif
