#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lonejson.h"

typedef struct streamed_query_request {
  char namespace_[16];
  lj_json_value selector;
  lj_json_value fields;
} streamed_query_request;

static const lj_field streamed_query_request_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(streamed_query_request, namespace_, "namespace",
                              LJ_OVERFLOW_FAIL),
    LJ_FIELD_JSON_VALUE_REQ(streamed_query_request, selector, "selector"),
    LJ_FIELD_JSON_VALUE(streamed_query_request, fields, "fields")};
LJ_MAP_DEFINE(streamed_query_request_map, streamed_query_request,
              streamed_query_request_fields);

static int write_temp_text_file(char *path_template, const char *text) {
  int fd;

  fd = mkstemp(path_template);
  if (fd < 0) {
    return -1;
  }
  if (write(fd, text, strlen(text)) != (ssize_t)strlen(text)) {
    close(fd);
    unlink(path_template);
    return -1;
  }
  return fd;
}

int main(void) {
  static const char selector_json[] =
      "{\"and\":[{\"eq\":{\"field\":\"/status\",\"value\":\"open\"}},"
      "{\"range\":{\"field\":\"/latency_ms\",\"gte\":12.5,\"lt\":300}}]}";
  char *large_fields;
  size_t len = 0u;
  size_t i;
  char selector_path[] = "/tmp/lonejson-json-selector-XXXXXX";
  char fields_path[] = "/tmp/lonejson-json-fields-XXXXXX";
  int selector_fd;
  int fields_fd;
  lj *runtime;
  streamed_query_request req;
  lj_error error;
  lj_status status;
  lj_config config;
  char *pretty;

  config = lj_default_config();
  config.write_pretty = 1;
  runtime = lj_new(&config, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  large_fields = (char *)malloc(65536u);
  if (large_fields == NULL) {
    fprintf(stderr, "malloc failed\n");
    lj_free(runtime);
    return 1;
  }
  large_fields[len++] = '[';
  for (i = 0u; i < 256u; ++i) {
    int wrote = snprintf(large_fields + len, 65536u - len, "%s\"/items/%lu\"",
                         (i == 0u) ? "" : ",", (unsigned long)i);
    if (wrote <= 0) {
      free(large_fields);
      return 1;
    }
    len += (size_t)wrote;
  }
  large_fields[len++] = ']';
  large_fields[len] = '\0';

  selector_fd = write_temp_text_file(selector_path, selector_json);
  if (selector_fd < 0) {
    perror("selector temp file");
    free(large_fields);
    lj_free(runtime);
    return 1;
  }
  close(selector_fd);

  fields_fd = write_temp_text_file(fields_path, large_fields);
  if (fields_fd < 0) {
    perror("fields temp file");
    unlink(selector_path);
    free(large_fields);
    lj_free(runtime);
    return 1;
  }
  close(fields_fd);

  lj_init(runtime, &streamed_query_request_map, &req, NULL);
  strcpy(req.namespace_, "default");

  status = lj_json_value_set_path(&req.selector, selector_path, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "selector set_path failed: %s\n", error.message);
    unlink(selector_path);
    unlink(fields_path);
    free(large_fields);
    lj_free(runtime);
    return 1;
  }
  status = lj_json_value_set_path(&req.fields, fields_path, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "fields set_path failed: %s\n", error.message);
    lj_cleanup(&streamed_query_request_map, &req, NULL);
    unlink(selector_path);
    unlink(fields_path);
    free(large_fields);
    lj_free(runtime);
    return 1;
  }

  pretty = lj_serialize_alloc(runtime, &streamed_query_request_map, &req, NULL,
                              &error);
  if (pretty == NULL) {
    fprintf(stderr, "serialize failed: %s\n", error.message);
    lj_cleanup(&streamed_query_request_map, &req, NULL);
    unlink(selector_path);
    unlink(fields_path);
    free(large_fields);
    lj_free(runtime);
    return 1;
  }

  printf("selector path=%s\n", selector_path);
  printf("fields path=%s\n", fields_path);
  printf("pretty size=%lu bytes\n", (unsigned long)strlen(pretty));
  printf("%.200s...\n", pretty);

  LONEJSON_FREE(pretty);
  lj_cleanup(&streamed_query_request_map, &req, NULL);
  unlink(selector_path);
  unlink(fields_path);
  free(large_fields);
  lj_free(runtime);
  return 0;
}
