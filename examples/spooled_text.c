#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "lonejson.h"

typedef struct large_message {
  char id[16];
  lj_spooled body;
} large_message;

static const lj_field large_message_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(large_message, id, "id", LJ_OVERFLOW_FAIL),
    LJ_FIELD_STRING_STREAM_CLASS(large_message, body, "body",
                                 LONEJSON_SPOOL_CLASS_LARGE_TEXT)};
LJ_MAP_DEFINE(large_message_map, large_message, large_message_fields);

static char *make_body(size_t len) {
  static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789 -_:,.";
  char *body;
  size_t i;

  body = (char *)malloc(len + 1u);
  if (body == NULL) {
    return NULL;
  }
  for (i = 0u; i < len; ++i) {
    body[i] = alphabet[i % (sizeof(alphabet) - 1u)];
  }
  body[len] = '\0';
  return body;
}

static char *make_json(const char *body) {
  const char *fmt = "{\"id\":\"msg-1\",\"body\":\"%s\"}";
  size_t need = (size_t)snprintf(NULL, 0, fmt, body);
  char *json = (char *)malloc(need + 1u);

  if (json == NULL) {
    return NULL;
  }
  snprintf(json, need + 1u, fmt, body);
  return json;
}

int main(void) {
  char *body;
  char *json;
  large_message message;
  unsigned char preview[49];
  lj_error error;
  lj_read_result chunk;
  struct stat st;
  char temp_path[LJ_SPOOL_TEMP_PATH_CAPACITY];
  int before_cleanup_exists;
  int after_cleanup_missing;
  lj_config config;
  lj *runtime;

  body = make_body(480u);
  if (body == NULL) {
    fprintf(stderr, "failed to allocate example input\n");
    return 1;
  }
  json = make_json(body);
  if (json == NULL) {
    fprintf(stderr, "failed to allocate example input\n");
    free(body);
    return 1;
  }

  config = lj_default_config();
  config.spool_large_text.memory_limit = 96u;
  config.spool_large_text.max_bytes = 0u;
  config.spool_large_text.temp_dir = "/tmp";
  runtime = lj_new(&config, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    free(body);
    LONEJSON_FREE(json);
    return 1;
  }

  if (lj_parse_cstr(runtime, &large_message_map, &message, json, &error) !=
      LJ_STATUS_OK) {
    fprintf(stderr, "parse failed: %s\n", error.message);
    lj_free(runtime);
    free(body);
    LONEJSON_FREE(json);
    return 1;
  }

  if (!lj_spooled_spilled(&message.body)) {
    fprintf(stderr, "expected the example to spill to disk\n");
    lj_cleanup(&large_message_map, &message, NULL);
    lj_free(runtime);
    free(body);
    LONEJSON_FREE(json);
    return 1;
  }

  strncpy(temp_path, message.body.temp_path, sizeof(temp_path) - 1u);
  temp_path[sizeof(temp_path) - 1u] = '\0';
  before_cleanup_exists =
      (temp_path[0] != '\0' && stat(temp_path, &st) == 0) ? 1 : 0;

  if (lj_spooled_rewind(&message.body, &error) != LJ_STATUS_OK) {
    fprintf(stderr, "rewind failed: %s\n", error.message);
    lj_cleanup(&large_message_map, &message, NULL);
    lj_free(runtime);
    free(body);
    LONEJSON_FREE(json);
    return 1;
  }
  chunk = lj_spooled_read(&message.body, preview, sizeof(preview) - 1u);
  preview[chunk.bytes_read] = '\0';

  printf("id=%s bytes=%lu spilled=%s temp=%s exists_before_cleanup=%s\n",
         message.id, (unsigned long)lj_spooled_size(&message.body),
         lj_spooled_spilled(&message.body) ? "yes" : "no",
         temp_path[0] != '\0' ? temp_path : "(anonymous)",
         before_cleanup_exists ? "yes" : "no");
  printf("preview=%s\n", (const char *)preview);

  lj_cleanup(&large_message_map, &message, NULL);
  lj_free(runtime);
  after_cleanup_missing =
      (temp_path[0] != '\0' && stat(temp_path, &st) != 0 && errno == ENOENT)
          ? 1
          : 0;
  printf("exists_after_cleanup=%s\n", after_cleanup_missing ? "no" : "yes");

  free(body);
  LONEJSON_FREE(json);
  return after_cleanup_missing ? 0 : 1;
}
