#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "lonejson.h"

typedef struct binary_blob {
  char id[16];
  lonejson_spooled payload;
} binary_blob;

static const lonejson_field binary_blob_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(binary_blob, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_BASE64_STREAM_CLASS(binary_blob, payload, "payload",
                                       LONEJSON_SPOOL_CLASS_BLOB)};
LONEJSON_MAP_DEFINE(binary_blob_map, binary_blob, binary_blob_fields);

static char *base64_encode_bytes(const unsigned char *data, size_t len) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char *encoded;
  size_t out_len;
  size_t in;
  size_t out;

  out_len = ((len + 2u) / 3u) * 4u;
  encoded = (char *)malloc(out_len + 1u);
  if (encoded == NULL) {
    return NULL;
  }
  in = 0u;
  out = 0u;
  while (in < len) {
    unsigned char a = data[in++];
    unsigned char b = 0u;
    unsigned char c = 0u;
    int have_b = 0;
    int have_c = 0;

    if (in < len) {
      b = data[in++];
      have_b = 1;
    }
    if (in < len) {
      c = data[in++];
      have_c = 1;
    }

    encoded[out++] = alphabet[(a >> 2u) & 0x3Fu];
    encoded[out++] = alphabet[((a & 0x03u) << 4u) | (b >> 4u)];
    encoded[out++] = have_b ? alphabet[((b & 0x0Fu) << 2u) | (c >> 6u)] : '=';
    encoded[out++] = have_c ? alphabet[c & 0x3Fu] : '=';
  }
  encoded[out] = '\0';
  return encoded;
}

static char *make_json(const char *base64_text) {
  const char *fmt = "{\"id\":\"blob-1\",\"payload\":\"%s\"}";
  size_t need = (size_t)snprintf(NULL, 0, fmt, base64_text);
  char *json = (char *)malloc(need + 1u);

  if (json == NULL) {
    return NULL;
  }
  snprintf(json, need + 1u, fmt, base64_text);
  return json;
}

int main(void) {
  unsigned char raw[240];
  unsigned char preview[16];
  char *base64_text;
  char *json;
  binary_blob blob;
  lonejson_error error;
  lonejson_read_result chunk;
  struct stat st;
  char temp_path[LONEJSON_SPOOL_TEMP_PATH_CAPACITY];
  int before_cleanup_exists;
  int after_cleanup_missing;
  size_t i;
  lonejson_config config;
  lonejson *runtime;

  for (i = 0u; i < sizeof(raw); ++i) {
    raw[i] = (unsigned char)((i * 7u) & 0xFFu);
  }
  base64_text = base64_encode_bytes(raw, sizeof(raw));
  if (base64_text == NULL) {
    fprintf(stderr, "failed to allocate example input\n");
    return 1;
  }
  json = make_json(base64_text);
  if (json == NULL) {
    fprintf(stderr, "failed to allocate example input\n");
    free(base64_text);
    return 1;
  }

  config = lonejson_default_config();
  config.spool_blob.memory_limit = 96u;
  config.spool_blob.max_bytes = 0u;
  config.spool_blob.temp_dir = "/tmp";
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    free(base64_text);
    LONEJSON_FREE(json);
    return 1;
  }

  if (lonejson_parse_cstr(runtime, &binary_blob_map, &blob, json, &error) !=
      LONEJSON_STATUS_OK) {
    fprintf(stderr, "parse failed: %s\n", error.message);
    lonejson_free(runtime);
    free(base64_text);
    LONEJSON_FREE(json);
    return 1;
  }

  if (!lonejson_spooled_spilled(&blob.payload)) {
    fprintf(stderr, "expected the example to spill to disk\n");
    lonejson_cleanup(&binary_blob_map, &blob, NULL);
    lonejson_free(runtime);
    free(base64_text);
    LONEJSON_FREE(json);
    return 1;
  }

  strncpy(temp_path, blob.payload.temp_path, sizeof(temp_path) - 1u);
  temp_path[sizeof(temp_path) - 1u] = '\0';
  before_cleanup_exists =
      (temp_path[0] != '\0' && stat(temp_path, &st) == 0) ? 1 : 0;

  if (lonejson_spooled_rewind(&blob.payload, &error) != LONEJSON_STATUS_OK) {
    fprintf(stderr, "rewind failed: %s\n", error.message);
    lonejson_cleanup(&binary_blob_map, &blob, NULL);
    lonejson_free(runtime);
    free(base64_text);
    LONEJSON_FREE(json);
    return 1;
  }
  chunk = lonejson_spooled_read(&blob.payload, preview, sizeof(preview));

  printf(
      "id=%s decoded_bytes=%lu spilled=%s temp=%s exists_before_cleanup=%s\n",
      blob.id, (unsigned long)lonejson_spooled_size(&blob.payload),
      lonejson_spooled_spilled(&blob.payload) ? "yes" : "no",
      temp_path[0] != '\0' ? temp_path : "(anonymous)",
      before_cleanup_exists ? "yes" : "no");
  printf("first_bytes=");
  for (i = 0u; i < chunk.bytes_read; ++i) {
    printf("%02x", (unsigned)preview[i]);
  }
  printf("\n");

  lonejson_cleanup(&binary_blob_map, &blob, NULL);
  lonejson_free(runtime);
  after_cleanup_missing =
      (temp_path[0] != '\0' && stat(temp_path, &st) != 0 && errno == ENOENT)
          ? 1
          : 0;
  printf("exists_after_cleanup=%s\n", after_cleanup_missing ? "no" : "yes");

  free(base64_text);
  LONEJSON_FREE(json);
  return after_cleanup_missing ? 0 : 1;
}
