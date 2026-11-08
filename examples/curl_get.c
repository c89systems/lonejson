#include <stdio.h>
#include <stdlib.h>

#include "lonejson.h"

typedef struct remote_payload {
  char *message;
  lonejson_int64 id;
} remote_payload;

static const char *find_test_ca_path(void) {
  const char *configured_path = getenv("LONEJSON_CURL_E2E_CAINFO");
  static const char *const candidates[] = {
      "build/devenv/credentials/server.crt",
      "../build/devenv/credentials/server.crt",
      "../../build/devenv/credentials/server.crt"};
  FILE *fp;
  size_t i;

  if (configured_path != NULL && configured_path[0] != '\0') {
    fp = fopen(configured_path, "rb");
    if (fp != NULL) {
      fclose(fp);
      return configured_path;
    }
  }
  for (i = 0u; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
    fp = fopen(candidates[i], "rb");
    if (fp != NULL) {
      fclose(fp);
      return candidates[i];
    }
  }
  return NULL;
}

static const lonejson_field remote_payload_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(remote_payload, message, "message"),
    LONEJSON_FIELD_I64_REQ(remote_payload, id, "id")};
LONEJSON_MAP_DEFINE(remote_payload_map, remote_payload, remote_payload_fields);

int main(void) {
  CURL *curl;
  CURLcode rc;
  const char *ca_path;
  const char *url;
  remote_payload payload;
  lonejson_curl_parse parse_ctx = {0};
  lonejson *runtime;
  lonejson_error error;

  runtime = lonejson_new(NULL, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  if (lonejson_curl_parse_init(&parse_ctx, runtime, &remote_payload_map,
                               &payload) != LONEJSON_STATUS_OK) {
    fprintf(stderr, "parse init failed: %s\n", parse_ctx.error.message);
    lonejson_free(runtime);
    return 1;
  }

  curl = curl_easy_init();
  if (curl == NULL) {
    fprintf(stderr, "curl_easy_init failed\n");
    lonejson_curl_parse_cleanup(&parse_ctx);
    lonejson_free(runtime);
    return 1;
  }

  ca_path = find_test_ca_path();
  if (ca_path == NULL) {
    fprintf(stderr, "could not locate e2e TLS certificate\n");
    curl_easy_cleanup(curl);
    lonejson_curl_parse_cleanup(&parse_ctx);
    lonejson_free(runtime);
    return 1;
  }

  url = getenv("LONEJSON_CURL_E2E_GET_URL");
  if (url == NULL || url[0] == '\0') {
    url = "https://localhost:8443/variants/ingest-target.json";
  }
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CAINFO, ca_path);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lonejson_curl_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &parse_ctx);

  rc = curl_easy_perform(curl);
  if (rc != CURLE_OK ||
      lonejson_curl_parse_finish(&parse_ctx) != LONEJSON_STATUS_OK) {
    fprintf(stderr, "request failed: %s\n", curl_easy_strerror(rc));
    if (rc == CURLE_COULDNT_CONNECT) {
      fprintf(stderr,
              "hint: start the local Podman environment with 'make dev-up'\n");
    }
    curl_easy_cleanup(curl);
    lonejson_curl_parse_cleanup(&parse_ctx);
    lonejson_free(runtime);
    return 1;
  }

  printf("id=%ld message=%s\n", (long)payload.id, payload.message);
  curl_easy_cleanup(curl);
  lonejson_cleanup(&remote_payload_map, &payload, NULL);
  lonejson_curl_parse_cleanup(&parse_ctx);
  lonejson_free(runtime);
  return 0;
}
